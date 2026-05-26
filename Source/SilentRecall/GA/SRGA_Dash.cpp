// Fill out your copyright notice in the Description page of Project Settings.

#include "SRGA_Dash.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h" // ⭐️ 태그 제어용 헤더 추가
#include "GameplayTagContainer.h"   // ⭐️ 태그 제어용 헤더 추가

USRGA_Dash::USRGA_Dash()
{
    FGameplayTagContainer TempTags;
    TempTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Movement.Dash"))); 
    SetAssetTags(TempTags);

    // 대시 중에는 내 몸에 '대시 중'이라는 태그를 달아줍니다.
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Dash")));
    
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // 맞고 있거나 이미 파쿠르 중일 때는 대시 못하게 막기
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Exhausted")));
}

void USRGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // =======================================================================
    // ⭐️ [기력 시스템 연동] 대시 시작! 공중 대시일 경우 기력이 차오를 수 있도록 Jump 태그를 뗍니다.
    // =======================================================================
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC)
    {
        ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump")), 0);
    }

    APlayerController* PC = Cast<APlayerController>(Character->GetController());
    if (!PC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FVector DashDirection = Character->GetActorForwardVector();
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        FVector InputDir = MoveComp->GetCurrentAcceleration().GetSafeNormal();
        if (!InputDir.IsNearlyZero())
        {
            DashDirection = InputDir; 
        }
    }

    FRotator LookRotation = PC->GetControlRotation();
    FVector LookDirection = LookRotation.Vector();
    
    FVector FinalDashVector = DashDirection;
    if (FMath::Abs(LookDirection.Z) > 0.2f) 
    {
        FinalDashVector = (DashDirection + (LookDirection * 0.5f)).GetSafeNormal();
    }
    
    UAbilityTask_ApplyRootMotionConstantForce* DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
        this,
        TEXT("DashRootMotion"),
        FinalDashVector,
        DashStrength,
        DashDuration,
        false, 
        nullptr,
        ERootMotionFinishVelocityMode::SetVelocity,
        FinalDashVector * 500.f, 
        0.1f, 
        false
    );

    if (DashTask)
    {
        DashTask->OnFinish.AddDynamic(this, &USRGA_Dash::OnDashCompleted);
        DashTask->ReadyForActivation(); 
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void USRGA_Dash::OnDashCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// =======================================================================
// ⭐️ [기력 시스템 연동] 대시가 끝날 때 상태를 체크하기 위해 추가된 함수입니다.
// =======================================================================
void USRGA_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Character && Character->GetCharacterMovement())
    {
        // 공중에서 대시가 끝났다면 다시 점프 태그를 붙여 기력 회복을 막습니다!
        if (Character->GetCharacterMovement()->IsFalling())
        {
            UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
            if (ASC)
            {
                ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump")));
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}