// Fill out your copyright notice in the Description page of Project Settings.


#include "SRGA_Dash.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"

USRGameplayAbility_Dash::USRGameplayAbility_Dash()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void USRGameplayAbility_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    FVector DashDirection = Character->GetActorForwardVector();
    if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
    {
        FVector InputDir = MoveComp->GetCurrentAcceleration().GetSafeNormal();
        if (!InputDir.IsNearlyZero())
        {
            DashDirection = InputDir; 
        }
    }

    DashDirection.Z = 0.0f;
    DashDirection.Normalize();
    
    UAbilityTask_ApplyRootMotionConstantForce* DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
        this,
        TEXT("DashRootMotion"),
        DashDirection,
        DashStrength,
        DashDuration,
        false, 
        nullptr,
        ERootMotionFinishVelocityMode::SetVelocity,
        DashDirection * 500.f, 
        0.1f, 
        false
    );

    if (DashTask)
    {
        DashTask->OnFinish.AddDynamic(this, &USRGameplayAbility_Dash::OnDashCompleted);
        DashTask->ReadyForActivation(); 
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void USRGameplayAbility_Dash::OnDashCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}