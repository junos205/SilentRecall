// Fill out your copyright notice in the Description page of Project Settings.

#include "SRGA_Vault.h"
#include "Character/SRPlayerCharacter.h"
#include "Character/SRCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h" // ⭐️ 태그 제어용 헤더 추가
#include "GameplayTagContainer.h"   // ⭐️ 태그 제어용 헤더 추가

USRGA_Vault::USRGA_Vault()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // GA 실행 중 소유자에게 부여할 태그
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Buff.SuperArmor")));


    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Attack.Melee")));
    
    // 피격 중이거나 그래플링 중이면 볼팅 실행 불가
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Grappling")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Melee")));
    
    // 대시 중일 때도 절대 볼팅(파쿠르) 실행 불가!
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Dash")));
}

bool USRGA_Vault::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;

    ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
    if (!PlayerChar) return false;

    FVector LedgeLoc, WallNormal;
    // 난간이 없으면 GA 실행을 거부합니다. (일반 점프가 나가게 됨)
    if (PlayerChar->DetectLedge(LedgeLoc, WallNormal) == EParkourType::None) return false;

    return true;
}

void USRGA_Vault::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!PlayerChar) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

    // =======================================================================
    // ⭐️ [기력 시스템 연동] 파쿠르 시작! 공중 판정(Jump 태그)을 지워서 파쿠르 도중엔 기력이 차오르게 합니다.
    // =======================================================================
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC)
    {
        ASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump")), 0);
    }

    FVector LedgeLocation;
    FVector WallNormal;
    EParkourType ParkourType = PlayerChar->DetectLedge(LedgeLocation, WallNormal);

    FVector ForwardDir = (-WallNormal).GetSafeNormal();
    FRotator TargetRotation = ForwardDir.Rotation();
    FVector Target1Loc = FVector::ZeroVector;
    FVector Target2Loc = FVector::ZeroVector;
    UAnimMontage* SelectedMontage = nullptr;

    if (ParkourType == EParkourType::LowVault)
    {
        Target1Loc = LedgeLocation + (WallNormal * 30.0f); Target1Loc.Z = LedgeLocation.Z;
        Target2Loc = LedgeLocation + (ForwardDir * 120.0f); Target2Loc.Z = PlayerChar->GetActorLocation().Z;
        SelectedMontage = PlayerChar->LowVaultMontage;
    }
    else if (ParkourType == EParkourType::HighMantle)
    {
        Target1Loc = LedgeLocation + (WallNormal * 50.0f); Target1Loc.Z = LedgeLocation.Z - 200.0f;
        Target2Loc = LedgeLocation + (ForwardDir * 100.0f); Target2Loc.Z = LedgeLocation.Z;
        SelectedMontage = PlayerChar->HighMantleMontage;
    }

    if (!SelectedMontage) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

    // 1. 모션 워핑 적용
    if (UMotionWarpingComponent* WarpingComp = PlayerChar->FindComponentByClass<UMotionWarpingComponent>())
    {
        WarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(FName("VaultHandTarget"), Target1Loc, TargetRotation);
        WarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(FName("VaultLandTarget"), Target2Loc, TargetRotation);
    }

    // 2. 캐릭터 상태 제어
    if (PlayerChar->GetCharacterMovement()) PlayerChar->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    
    // 무적 콜리전 분리! (총알은 맞고, 벽에는 안 끼이게)
    if (PlayerChar->GetCapsuleComponent())
    {
        PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
        PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
        PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }

    // 카메라 워핑 연출을 위한 초기 각도 세팅
    PlayerChar->bUseControllerRotationYaw = false;
    if (APlayerController* PC = Cast<APlayerController>(PlayerChar->GetController()))
    {
        PlayerChar->InitialSocketRot = PlayerChar->GetMesh()->GetSocketRotation(TEXT("CameraSocket"));
        PlayerChar->InitialControlRot = PC->GetControlRotation();
        PC->SetIgnoreLookInput(true);
    }

    // 3. 몽타주 실행 태스크
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SelectedMontage);
    MontageTask->OnCompleted.AddDynamic(this, &USRGA_Vault::OnMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &USRGA_Vault::OnMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &USRGA_Vault::OnMontageInterrupted);
    MontageTask->ReadyForActivation();
}

void USRGA_Vault::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_Vault::OnMontageInterrupted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void USRGA_Vault::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(GetAvatarActorFromActorInfo()))
    {
        if (PlayerChar->GetCapsuleComponent()) PlayerChar->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        
        if (PlayerChar->GetCharacterMovement())
        {
            PlayerChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            if (!bWasCancelled) PlayerChar->GetCharacterMovement()->Velocity = PlayerChar->GetActorForwardVector() * 200.0f;

            // =======================================================================
            // ⭐️ [기력 시스템 연동] 파쿠르가 끝났는데 공중이라면? 다시 점프 태그를 붙여서 기력 회복 정지!
            // =======================================================================
            if (PlayerChar->GetCharacterMovement()->IsFalling())
            {
                UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
                if (ASC)
                {
                    ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump")));
                }
            }
        }

        PlayerChar->bUseControllerRotationYaw = true;
        
        if (APlayerController* PC = Cast<APlayerController>(PlayerChar->GetController()))
        {
            FRotator ResetRot = PC->GetControlRotation();
            ResetRot.Roll = 0.0f; 
            PC->SetControlRotation(ResetRot);
            PC->ResetIgnoreLookInput();
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}