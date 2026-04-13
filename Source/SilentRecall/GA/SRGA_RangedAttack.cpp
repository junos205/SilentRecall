#include "SRGA_RangedAttack.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Weapon/SRWeaponInstance.h"
#include "Character/SRPlayerCharacter.h"
#include "Interface/SRCharacterInterface.h"

USRGA_RangedAttack::USRGA_RangedAttack()
{
    // ⭐️ [필수] 연사 상태와 타이머를 유지하기 위해 인스턴싱 정책 설정
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void USRGA_RangedAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 1. AnimNotify가 쏠 "총알 발사/명중" 이벤트(Event.Ranged.Fire)를 백그라운드에서 항상 대기합니다.
    FGameplayTag FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
    UAbilityTask_WaitGameplayEvent* WaitFireTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FireEventTag);
    WaitFireTask->EventReceived.AddDynamic(this, &USRGA_RangedAttack::OnFireEventReceived);
    WaitFireTask->ReadyForActivation();

    // 2. 사격(연사 루프) 시작!
    FireShot();
}

void USRGA_RangedAttack::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);
    // 버튼을 떼면 알아서 루프가 멈추므로 특별한 로직이 당장 필요하진 않으나,
    // 필요하다면 여기서 즉시 EndAbility를 호출할 수도 있습니다.
}

void USRGA_RangedAttack::FireShot()
{
    USRWeaponInstance* WeaponInst = Cast<USRWeaponInstance>(GetCurrentSourceObject());
    if (!WeaponInst || !WeaponInst->WeaponData || !WeaponInst->HasAmmo())
    {
        // 총알이 없거나 무기 데이터가 없으면 GA 종료
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!PlayerChar) return;

    // ----------------------------------------------------
    // 1. 총알 소비
    // ----------------------------------------------------
    WeaponInst->ConsumeAmmo();

    // ----------------------------------------------------
    // 2. 애니메이션 재생 (1P는 인터페이스, 3P는 Task)
    // ----------------------------------------------------
    UAnimMontage* FireMontage = WeaponInst->WeaponData->AttackComboMontages.Num() > 0 ? WeaponInst->WeaponData->AttackComboMontages[0] : nullptr;
    if (FireMontage)
    {
        // 1P: 인터페이스로 수동 재생 명령
        if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(PlayerChar))
        {
            CharInterface->PlayWeaponMontage(FireMontage, true);
        }

        // 3P: 태스크로 재생 (완료 시점 추적)
        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, FireMontage, 1.0f
        );
        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_RangedAttack::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_RangedAttack::OnMontageCompleted);
        PlayMontageTask->ReadyForActivation();
    }

    // ----------------------------------------------------
    // 3. 반동 (Recoil) 적용
    // ----------------------------------------------------
    float RecoilPitch = FMath::RandRange(WeaponInst->WeaponData->MinRecoilPitch, WeaponInst->WeaponData->MaxRecoilPitch);
    float RecoilYaw = FMath::RandRange(WeaponInst->WeaponData->MinRecoilYaw, WeaponInst->WeaponData->MaxRecoilYaw);
    if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(PlayerChar))
    {
        CharInterface->ApplyRecoil(RecoilPitch, RecoilYaw);
    }

    // ----------------------------------------------------
    // 4. 연사 (Full-Auto) 루프 제어 (핵심!)
    // ----------------------------------------------------
    if (WeaponInst->WeaponData->bIsAutomatic && GetCurrentAbilitySpec()->InputPressed)
    {
        // 연사 무기이고, 아직 마우스를 꾹 누르고 있다면?
        // FireRate(예: 0.1초)만큼 기다렸다가 다시 FireShot 호출! (루프)
        UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, WeaponInst->WeaponData->FireRate);
        WaitTask->OnFinish.AddDynamic(this, &USRGA_RangedAttack::FireShot);
        WaitTask->ReadyForActivation();
    }
    else
    {
        // 단발 무기이거나, 마우스 버튼을 뗐다면?
        // 광클 방지를 위해 FireRate만큼 쿨타임을 기다린 후 GA 종료!
        UAbilityTask_WaitDelay* CooldownTask = UAbilityTask_WaitDelay::WaitDelay(this, WeaponInst->WeaponData->FireRate);
        CooldownTask->OnFinish.AddDynamic(this, &USRGA_RangedAttack::EndAbilityDelegate);
        CooldownTask->ReadyForActivation();
    }
}

void USRGA_RangedAttack::OnFireEventReceived(FGameplayEventData Payload)
{
    // 1. AN(원거리 트레이스)에서 무전(Payload)으로 보낸 '맞은 놈(Target)' 확인
    AActor* TargetActor = const_cast<AActor*>(Payload.Target.Get());
    if (!TargetActor || !DamageEffectClass) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC)
    {
        // 2. 데미지 이펙트 환경설정(Context) 주머니 만들기
        FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

        // ⭐️ 3. AN에서 정성스럽게 담아 보낸 타격 정보(TargetData)를 꺼내서 Context에 쏙!
        if (Payload.TargetData.IsValid(0))
        {
            const FHitResult* HitResult = Payload.TargetData.Get(0)->GetHitResult();
            if (HitResult)
            {
                ContextHandle.AddHitResult(*HitResult);
            }
        }

        // 4. 이펙트 스펙 만들고 타겟에게 발사!
        // -> 여기서 발사된 이펙트가 타겟의 AttributeSet으로 넘어가서 피를 깎고, HitReact를 부릅니다.
        FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);
        
        if (SpecHandle.IsValid())
        {
            GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
        }
    }
}

void USRGA_RangedAttack::EndAbilityDelegate()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_RangedAttack::OnMontageCompleted()
{
    // 몽타주가 끝났을 때 추가로 처리할 내용이 있다면 여기에 작성
    // (연사 루프가 돌고 있을 수 있으므로 무조건 여기서 EndAbility를 부르지는 않습니다.)
}