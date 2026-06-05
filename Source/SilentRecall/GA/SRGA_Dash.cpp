#include "GA/SRGA_Dash.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "AbilitySystemComponent.h" 
#include "GameplayTagContainer.h"   
#include "NiagaraFunctionLibrary.h" // 🎯 나이아가라 스폰용 헤더 추가
#include "NiagaraSystem.h"          // 🎯 나이아가라 시스템 헤더 추가

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

    // 대시 벡터와 캐릭터의 각 로컬 방향 축 벡터를 내적(Dot) 연산
    float ForwardDot = FVector::DotProduct(FinalDashVector, Character->GetActorForwardVector());
    float RightDot = FVector::DotProduct(FinalDashVector, Character->GetActorRightVector());
    float UpDot = FVector::DotProduct(FinalDashVector, Character->GetActorUpVector());

    // 1. 위/아래 수직 대시 체크 (Z축 값이 임계값 이상인 경우)
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
        // 전후방 성분이 좌우 성분보다 크거나 같을 때
        if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
        {
            SelectedFX = (ForwardDot >= 0.f) ? ForwardDashFX : BackwardDashFX;
        }
        // 좌우 성분이 더 클 때
        else
        {
            SelectedFX = (RightDot >= 0.f) ? RightDashFX : LeftDashFX;
        }
    }

    // 예외 처리: 특정 방향 에셋이 안 비어있으면 재생, 비어있으면 전방 기본 이펙트로 대체
    if (!SelectedFX) SelectedFX = ForwardDashFX;

    if (SelectedFX)
    {
        // 캐릭터 루트(메시)에 이펙트를 부착하여 재생합니다. 
        // 방향은 대시 진행 방향(FinalDashVector.Rotation())을 바라보게 정렬합니다.
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