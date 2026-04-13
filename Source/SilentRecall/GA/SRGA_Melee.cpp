// Fill out your copyright notice in the Description page of Project Settings.


#include "SRGA_Melee.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Weapon/SRWeaponInstance.h"

USRGA_Melee::USRGA_Melee()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void USRGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    CurrentComboIndex = 1;
    bIsComboSaved = false;

    // 1. 첫 타격 재생
    PlayComboSection();

    // ❌ 태스크 생성 코드 삭제됨! (InputTask 부분 전부 날림)

    // 2. 타이밍 체크 대기 태스크 (이것만 남겨둡니다. 몽타주 노티파이를 들어야 하니까요!)
    FGameplayTag ComboCheckTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.CheckCombo"));
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboCheckTag);
    EventTask->EventReceived.AddDynamic(this, &USRGA_Melee::OnComboCheckEventReceived);
    EventTask->ReadyForActivation();

    FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Melee.Hit"));
    UAbilityTask_WaitGameplayEvent* WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag);
    
    // GA 이벤트 발동 시 OnHitEventReceived 함수를 실행
    WaitHitTask->EventReceived.AddDynamic(this, &USRGA_Melee::OnHitEventReceived);
    
    // 태스크 실행
    WaitHitTask->ReadyForActivation();
}

void USRGA_Melee::OnHitEventReceived(FGameplayEventData Payload)
{
    AActor* TargetActor = const_cast<AActor*>(Payload.Target.Get());
    if (!TargetActor || !DamageEffectClass) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC)
    {
        // 1. 컨텍스트 주머니 생성
        FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

        // ⭐️ 2. [핵심 추가] ANS에서 페이로드에 담아 보냈던 타격 정보(TargetData)를 꺼내서 주머니에 쏙 넣습니다!
        if (Payload.TargetData.IsValid(0))
        {
            const FHitResult* HitResult = Payload.TargetData.Get(0)->GetHitResult();
            if (HitResult)
            {
                ContextHandle.AddHitResult(*HitResult);
            }
        }

        // 3. 이펙트 스펙 만들고 발사! (이제 이 스펙 안에 HitResult가 들어있습니다)
        FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);

        if (SpecHandle.IsValid())
        {
            GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
        }
    }
}

void USRGA_Melee::InputPressed(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputPressed(Handle, ActorInfo, ActivationInfo);
    
    bIsComboSaved = true;
}

void USRGA_Melee::OnComboCheckEventReceived(FGameplayEventData Payload)
{
    UE_LOG(LogTemp, Warning, TEXT("[AttackGA] Playing Combo Section: Attack%d"), CurrentComboIndex);

    // 1. 유저가 선입력을 했고, 아직 막타가 아니라면? (콤보 성공)
    if (bIsComboSaved && CurrentComboIndex < MaxComboCount)
    {
        CurrentComboIndex++;
        bIsComboSaved = false; 
        
        // ⭐️ 콤보가 이어질 때만 다음 섹션으로 점프합니다!
        USRWeaponInstance* WeaponInstance = Cast<USRWeaponInstance>(GetCurrentSourceObject());
        if (WeaponInstance && WeaponInstance->WeaponData && WeaponInstance->WeaponData->AttackComboMontages.Num() > 0)
        {
            FName SectionName = FName(*FString::Printf(TEXT("Attack%d"), CurrentComboIndex));
            
            // 어차피 PlayMontageAndWait 태스크가 돌고 있으므로 점프만 시켜주면 부드럽게 넘어갑니다.
            MontageJumpToSection(SectionName);
        }
    }
    // 2. 유저가 입력을 안 했거나, 이미 막타(3타)라면? (콤보 종료)
    else
    {
        bIsComboSaved = false;
        CurrentComboIndex = 1;
        
    }
}

void USRGA_Melee::PlayComboSection()
{
    UE_LOG(LogTemp, Warning, TEXT("[AttackGA] Playing Combo Section: Attack%d"), CurrentComboIndex);
    
    USRWeaponInstance* WeaponInstance = Cast<USRWeaponInstance>(GetCurrentSourceObject());
    if (WeaponInstance && WeaponInstance->WeaponData && WeaponInstance->WeaponData->AttackComboMontages.Num() > 0)
    {
        UAnimMontage* ComboMontage = WeaponInstance->WeaponData->AttackComboMontages[0];
        
        // "Attack1", "Attack2" 같은 섹션 이름을 동적으로 만듭니다.
        FName SectionName = FName(*FString::Printf(TEXT("Attack%d"), CurrentComboIndex));

        // 몽타주를 재생하되, 특정 섹션부터 시작하도록 설정합니다.
        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, ComboMontage, 1.0f, SectionName
        );

        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_Melee::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_Melee::OnMontageCompleted);
        PlayMontageTask->ReadyForActivation();
    }
}

void USRGA_Melee::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    
    bIsComboSaved = false;
    CurrentComboIndex = 1;
}
