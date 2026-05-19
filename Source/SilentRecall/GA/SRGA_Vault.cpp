// Fill out your copyright notice in the Description page of Project Settings.

#include "SRGA_Vault.h"
#include "Character/SRPlayerCharacter.h"
#include "Character/SRCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

USRGA_Vault::USRGA_Vault()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // GA 실행 중 소유자에게 부여할 태그
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Buff.SuperArmor")));
    // 피격 중이거나 그래플링 중이면 볼팅 실행 불가
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Grappling")));
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
        // 1. 파쿠르 중 벽이나 움직이는 물체에 끼이지 않도록 물리 충돌을 무시합니다.
        PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
        PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
        
        // (선택) 다른 캐릭터와 비벼지는 것을 막으려면 Pawn도 끕니다.
        PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

        // ⭐️ 2. [핵심] 총알이 타격 판정을 내리는 채널은 무조건 Block으로 유지합니다!
        
        // 만약 히트스캔 트레이스를 Visibility 채널로 쏘고 계시다면:
        PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        
        // 만약 프로젝트 세팅에서 따로 만든 'Damage' 커스텀 트레이스 채널이 있다면:
        // (보통 ECC_GameTraceChannel1 등으로 매핑됩니다)
        // PlayerChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
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
    // 피격 당해서 몽타주가 찢겼을 때!
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void USRGA_Vault::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(GetAvatarActorFromActorInfo()))
    {
        // ⭐️ 원상 복구 로직 (정상이든 캔슬이든 무조건 실행됨)
        if (PlayerChar->GetCapsuleComponent()) PlayerChar->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        
        if (PlayerChar->GetCharacterMovement())
        {
            PlayerChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            // 캔슬되지 않고 정상 종료 시에만 앞으로 튕겨나가는 힘을 줍니다.
            if (!bWasCancelled) PlayerChar->GetCharacterMovement()->Velocity = PlayerChar->GetActorForwardVector() * 200.0f;
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