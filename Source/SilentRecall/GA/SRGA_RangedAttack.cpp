#include "SRGA_RangedAttack.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Weapon/SRWeaponInstance.h"
#include "Character/SRInventoryComponent.h"
#include "Character/SRPlayerCharacter.h"
#include "Interface/SRCharacterInterface.h"
#include "Weapon/SRProjectile.h"

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
    
    if (WeaponInst->WeaponData->FireCameraShake)
    {
        if (APlayerController* PC = Cast<APlayerController>(PlayerChar->GetController()))
        {
            if (PC->PlayerCameraManager)
            {
                // 1.0f 는 쉐이크의 세기(Scale)입니다. 무기 데미지나 특징에 따라 가변적으로 줄 수도 있습니다.
                PC->PlayerCameraManager->StartCameraShake(WeaponInst->WeaponData->FireCameraShake, 1.0f);
            }
        }
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
    USRWeaponInstance* WeaponInst = Cast<USRWeaponInstance>(GetCurrentSourceObject());
    if (!WeaponInst || !WeaponInst->WeaponData || !DamageEffectClass) return;

    const FHitResult* HitResult = Payload.TargetData.IsValid(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
    if (!HitResult) return;

    // ==========================================
    // 🚀 [A] 투사체 (Projectile) 발사 로직
    // ==========================================
    if (WeaponInst->WeaponData->bIsProjectile && WeaponInst->WeaponData->ProjectileClass)
    {
        AActor* Avatar = GetAvatarActorFromActorInfo();
        FVector MuzzleLocation = HitResult->TraceStart; // 기본값 (보통 카메라 위치)

        // ⭐️ [수정됨] 인터페이스 가짜 함수 대신, 확실한 인벤토리 컴포넌트를 뒤져서 무기를 찾습니다!
        if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(Avatar))
        {
            MuzzleLocation = PlayerChar->GetActiveWeaponMuzzleLocation();
        }

        // 총구 시차 보정: 총구에서 조준선 끝점(TraceEnd)을 바라보는 회전값 계산
        FVector TargetPoint = HitResult->bBlockingHit ? HitResult->ImpactPoint : HitResult->TraceEnd;
        FRotator SpawnRotation = (TargetPoint - MuzzleLocation).Rotation();
        FTransform SpawnTransform(SpawnRotation, MuzzleLocation);

        ASRProjectile* SpawnedProj = GetWorld()->SpawnActorDeferred<ASRProjectile>(
            WeaponInst->WeaponData->ProjectileClass, SpawnTransform, Avatar, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (SpawnedProj)
        {
            SpawnedProj->InstigatorActor = Avatar;
            SpawnedProj->DamageAmount = WeaponInst->WeaponData->BaseDamage;
            SpawnedProj->SetImpactForce(WeaponInst->WeaponData->ImpactForce);
            
            // ⭐️⭐️ [바로 이 부분!!!] ⭐️⭐️
            // GA가 가진 데미지 이펙트(GE_Damage)를 총알의 빈 주머니에 넣어줘야 합니다!
            // 이걸 안 넣어주면 총알은 데미지를 줄 수단이 없어서 그냥 터지기만 합니다.
            SpawnedProj->DamageEffectClass = DamageEffectClass;

            SpawnedProj->FinishSpawning(SpawnTransform);

            FVector ShootDir = (TargetPoint - MuzzleLocation).GetSafeNormal();
            SpawnedProj->SetSpeed(WeaponInst->WeaponData->ProjectileSpeed, ShootDir); 
        }
    }

    // ==========================================
    // ⚡ [B] 히트스캔 (Hitscan) 데미지 및 튕겨내기 로직
    // ==========================================
    AActor* OriginalShooter = GetAvatarActorFromActorInfo();
    AActor* TargetActor = const_cast<AActor*>(Payload.Target.Get());
    if (!TargetActor) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC)
    {
        FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        ContextHandle.AddHitResult(*HitResult);

        // ⭐️ [대망의 히트스캔 튕겨내기(Deflect) 검사!]
        FGameplayTag ParryTag = FGameplayTag::RequestGameplayTag(FName("State.Parrying"));
        if (TargetASC->HasMatchingGameplayTag(ParryTag))
        {
            // 챙!! 타겟이 패링 중이다! 
            UE_LOG(LogTemp, Warning, TEXT("[RangedAttack] Hitscan DEFLECTED by %s!"), *TargetActor->GetName());

            // 1. 공격 대상을 '나 자신(쏜 사람)'으로 바꿔버림! (Ricochet)
            TargetASC = GetAbilitySystemComponentFromActorInfo();
            
            // 2. 이 이펙트의 가해자(Instigator)를 '패링에 성공한 타겟'으로 변경!
            ContextHandle.AddInstigator(TargetActor, TargetActor);

            // (선택) 여기서 튕겨내는 불꽃 파티클이나 "챙!" 소리를 TargetActor 위치에 재생
        }
        else
        {
            // 평범하게 맞음. 가해자는 쏜 사람(나)
            ContextHandle.AddInstigator(OriginalShooter, OriginalShooter);
        }

        // 데미지 이펙트 적용! (튕겨졌다면 내가 내 피를 깎게 됩니다)
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