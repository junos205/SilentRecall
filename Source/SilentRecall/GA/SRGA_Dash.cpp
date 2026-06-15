#include "GA/SRGA_Dash.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h" 
#include "GameplayTagContainer.h"   
#include "NiagaraFunctionLibrary.h" 
#include "NiagaraSystem.h"          
#include "Kismet/GameplayStatics.h" // 🔊 사운드 재생을 위한 헤더 추가

USRGA_Dash::USRGA_Dash()
{
    FGameplayTagContainer TempTags;
    TempTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Movement.Dash"))); 
    SetAssetTags(TempTags);

    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Dash")));
    
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Exhausted")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Melee")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Attack.Melee")));
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
    
    // =======================================================================
    // 🎯 [방향 판별 및 나이아가라 이펙트 재생]
    // =======================================================================
    UNiagaraSystem* SelectedFX = nullptr;

    float ForwardDot = FVector::DotProduct(FinalDashVector, Character->GetActorForwardVector());
    float RightDot = FVector::DotProduct(FinalDashVector, Character->GetActorRightVector());
    float UpDot = FVector::DotProduct(FinalDashVector, Character->GetActorUpVector());

    // 1. 위/아래 수직 대시 체크
    if (UpDot > 0.55f)
    {
        SelectedFX = UpwardDashFX;
    }
    else if (UpDot < -0.55f)
    {
        SelectedFX = DownwardDashFX;
    }
    // 2. 수평 평면(앞/뒤/좌/우) 대시 체크
    else
    {
        if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
        {
            SelectedFX = (ForwardDot >= 0.f) ? ForwardDashFX : BackwardDashFX;
        }
        else
        {
            SelectedFX = (RightDot >= 0.f) ? RightDashFX : LeftDashFX;
        }
    }

    if (!SelectedFX) SelectedFX = ForwardDashFX;

    if (SelectedFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            SelectedFX,
            Character->GetMesh(),
            NAME_None,
            FVector::ZeroVector,
            FinalDashVector.Rotation(),
            EAttachLocation::KeepRelativeOffset,
            true
        );
    }

    // =======================================================================
    // 🔊 [신규 추가] 대시 사운드 재생 (캐릭터의 현재 위치에서 실행)
    // =======================================================================
    if (DashSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), DashSound, Character->GetActorLocation());
    }
    // =======================================================================

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

void USRGA_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Character && Character->GetCharacterMovement())
    {
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