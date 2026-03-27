// Fill out your copyright notice in the Description page of Project Settings.


#include "SRGA_Melee.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Kismet/KismetSystemLibrary.h"
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
    FGameplayTag ComboCheckTag = FGameplayTag::RequestGameplayTag(FName("Event.Melee.ComboCheck"));
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboCheckTag);
    EventTask->EventReceived.AddDynamic(this, &USRGA_Melee::OnComboCheckEventReceived);
    EventTask->ReadyForActivation();
}

void USRGA_Melee::OnHitEventReceived(FGameplayEventData Payload)
{
    // // 타격 타이밍이 오면 구형(Sphere) 트레이스를 발사하여 적을 찾습니다.
    // ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    // if (!Character) return;
    //
    // FVector StartLoc = Character->GetActorLocation();
    // FVector ForwardDir = Character->GetActorForwardVector();
    // FVector EndLoc = StartLoc + (ForwardDir * TraceDistance);
    //
    // TArray<AActor*> ActorsToIgnore;
    // ActorsToIgnore.Add(Character);
    //
    // FHitResult HitResult;
    //
    // // 엔진의 구형 트레이스(Sphere Trace)를 사용해 약간 빗나가도 맞도록 넉넉한 판정을 줍니다.
    // bool bHit = UKismetSystemLibrary::SphereTraceSingle(
    //     Character,
    //     StartLoc, EndLoc, TraceRadius,
    //     UEngineTypes::ConvertToTraceType(ECC_Pawn), // 적 폰만 감지
    //     false, ActorsToIgnore,
    //     EDrawDebugTrace::ForDuration, // 디버그용 (빨간 줄/녹색 구체 확인 후 None으로 변경)
    //     HitResult,
    //     true
    // );
    //
    // if (bHit && HitResult.GetActor())
    // {
    //     // 3. 적을 맞췄다면? 데미지 이펙트(GE)를 생성해서 적의 ASC에 꽂아 넣습니다.
    //     UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitResult.GetActor());
    //     
    //     if (TargetASC && DamageEffectClass)
    //     {
    //         // 이펙트 생성 (레벨, 작성자 정보 포함)
    //         FGameplayEffectContextHandle EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
    //         EffectContext.AddHitResult(HitResult); // 맞은 부위 정보 넘겨주기
    //
    //         FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), EffectContext);
    //         
    //         if (SpecHandle.IsValid())
    //         {
    //             // 적에게 최종 데미지 적용!
    //             GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
    //         }
    //     }
    // }
}

void USRGA_Melee::InputPressed(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputPressed(Handle, ActorInfo, ActivationInfo);
}

void USRGA_Melee::OnComboCheckEventReceived(FGameplayEventData Payload)
{
}

void USRGA_Melee::PlayComboSection()
{
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
}
