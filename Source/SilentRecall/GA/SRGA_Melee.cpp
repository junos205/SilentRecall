// Fill out your copyright notice in the Description page of Project Settings.


#include "SRGA_Melee.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"

USRGameplayAbility_Melee::USRGameplayAbility_Melee()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void USRGameplayAbility_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!AttackMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 공격 애니메이션 재생 태스크
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, TEXT("AttackMontage"), AttackMontage, 1.0f);
    
    MontageTask->OnCompleted.AddDynamic(this, &USRGameplayAbility_Melee::OnMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &USRGameplayAbility_Melee::OnMontageCompleted);
    MontageTask->OnCancelled.AddDynamic(this, &USRGameplayAbility_Melee::OnMontageCompleted);
    MontageTask->ReadyForActivation();

    // 2. 타격 타이밍(Anim Notify)에서 날아올 이벤트 대기 태스크
    // (예: "Event.Melee.Hit" 이라는 태그를 기다림)
    FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Melee.Hit"));
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag);
    
    EventTask->EventReceived.AddDynamic(this, &USRGameplayAbility_Melee::OnHitEventReceived);
    EventTask->ReadyForActivation();
}

void USRGameplayAbility_Melee::OnHitEventReceived(FGameplayEventData Payload)
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

void USRGameplayAbility_Melee::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}