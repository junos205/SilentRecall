// Fill out your copyright notice in the Description page of Project Settings.

#include "SRCharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "GameplayTagContainer.h"

USRCharacterMovementComponent::USRCharacterMovementComponent()
{
}

void USRCharacterMovementComponent::DoWallJump()
{
    if (CustomMovementMode != ECustomMovementMode::CMOVE_WallRunning) return;

    FVector CurrentMomentum = Velocity;
    CurrentMomentum.Z = 0.0f;

    float CurrentSpeed = CurrentMomentum.Size();

    FVector JumpUpForce = FVector::UpVector * WallJumpHeight;
    FVector BasePushOff = WallNormal * WallRepulsiveForce;

    FVector LookDir = CharacterOwner->GetControlRotation().Vector();
    LookDir.Z = 0.0f;
    LookDir.Normalize();

    FVector LookForce = LookDir * WallPropulsionForce;

    FVector RedirectedMomentum = LookDir * CurrentSpeed;

    Velocity = BasePushOff + JumpUpForce + LookForce + RedirectedMomentum;

    WallRunCooldown = WallSeizeThreshold;
    SetMovementMode(MOVE_Falling);

    CharacterOwner->JumpCurrentCount++;
}

void USRCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

    if (!CharacterOwner) return;
    
    // ==========================================================
    // 1. [월런 진입] 기존 코드 유지
    // ==========================================================
    if (MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_WallRunning)
    {
        OnWallRunStartedDelegate.Broadcast();

        CharacterOwner->bUseControllerRotationYaw = false;
        bOrientRotationToMovement = false;

        FVector WallRunDirection = FVector::CrossProduct(WallNormal, FVector::UpVector);
        if (FVector::DotProduct(CharacterOwner->GetActorForwardVector(), WallRunDirection) < 0.0f)
        {
            WallRunDirection = -WallRunDirection;
        }

        CharacterOwner->SetActorRotation(WallRunDirection.Rotation());

        if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
        {
            if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
            {
                float RunYaw = WallRunDirection.Rotation().Yaw;
                CameraManager->ViewYawMin = RunYaw - 70.0f;
                CameraManager->ViewYawMax = RunYaw + 70.0f;
            }
        }
    }

    // ==========================================================
    // 2. [슬라이딩 진입] ⭐️ 신규 추가: 좌우 시선 락온
    // ==========================================================
    if (MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_Sliding)
    {
        // 컨트롤러 회전과 캐릭터 몸체 회전 분리 (마우스 움직임에 메쉬가 뒤틀리는 것 방지)
        CharacterOwner->bUseControllerRotationYaw = false;
        bOrientRotationToMovement = false;

        if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
        {
            if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
            {
                // 슬라이딩 진입 순간의 플레이어 실제 수평 각도(Yaw) 획득
                float SlideInitYaw = CharacterOwner->GetControlRotation().Yaw;
                
                // 최소값과 최대값을 똑같은 값으로 묶어버려서 좌우 회전을 물리적으로 원천 차단합니다!
                CameraManager->ViewYawMin = SlideInitYaw;
                CameraManager->ViewYawMax = SlideInitYaw;
            }
        }
    }

    // ==========================================================
    // 3. [월런 탈출] 기존 코드 유지
    // ==========================================================
    if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::CMOVE_WallRunning)
    {
        OnWallRunEndedDelegate.Broadcast();

        TargetWallRunRoll = 0.0f;
        
        if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
        {
            if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
            {
                CameraManager->ViewYawMin = 0.0f;
                CameraManager->ViewYawMax = 359.999f;
            }
        }

        CharacterOwner->bUseControllerRotationYaw = true; 
        bOrientRotationToMovement = false; 
    }

    // ==========================================================
    // 4. [슬라이딩 탈출] ⭐️ 수정: 카메라 잠금 해제 및 기존 에디터 복구 통합
    // ==========================================================
    if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::CMOVE_Sliding)
    {
        if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
        {
            if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
            {
                // 제한되었던 카메라 Yaw 각도를 다시 360도 자유 시점으로 복구합니다.
                CameraManager->ViewYawMin = 0.0f;
                CameraManager->ViewYawMax = 359.999f;
            }
        }

        // 마우스 턴에 맞춰 다시 캐릭터가 회전하도록 주도권 복구
        CharacterOwner->bUseControllerRotationYaw = true;

        if (CustomMovementMode != ECustomMovementMode::CMOVE_Sliding)
        {
            ExitSlide();
        }
    }
}

void USRCharacterMovementComponent::EnterSlide()
{
    if (MovementMode == MOVE_Walking && Velocity.Size2D() > MinSlideSpeed)
    {
       bWantsToCrouch = true; 
       SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_Sliding);
    }
}

void USRCharacterMovementComponent::ExitSlide()
{
    bWantsToCrouch = false;
    SetMovementMode(MOVE_Walking);

    if (CharacterOwner)
    {
       if (USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh())
       {
          FRotator DefaultRotation(0.0f, -90.0f, 0.0f);
          Mesh->SetRelativeRotation(DefaultRotation);
       }
    }
}

void USRCharacterMovementComponent::DoSlideJump()
{
    if (CustomMovementMode != ECustomMovementMode::CMOVE_Sliding) return;

    ExitSlide();

    FVector CurrentMomentum = Velocity;
    CurrentMomentum.Z = 0.0f; 

    FVector JumpUpForce = FVector::UpVector * JumpZVelocity;
    FVector ForwardBoost = CurrentMomentum.GetSafeNormal2D() * SlideJumpForce;

    Velocity = CurrentMomentum + JumpUpForce + ForwardBoost;
    SetMovementMode(MOVE_Falling);

    if (CharacterOwner)
    {
       CharacterOwner->JumpCurrentCount++;
    }
}

void USRCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
    
    if (WallRunCooldown > 0.0f)
    {
       WallRunCooldown -= DeltaSeconds;
       return; 
    }

    if (MovementMode == MOVE_Falling && !Acceleration.IsNearlyZero())
    {
       if (TryWallRun())
       {
          TimeOnWall = 0.0f; 
          SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_WallRunning);
       }
    }
}

void USRCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation,
                                                      const FVector& OldVelocity)
{
    Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

void USRCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
    Super::PhysCustom(deltaTime, Iterations);

    switch (CustomMovementMode)
    {
    case ECustomMovementMode::CMOVE_Sliding:
       PhysSliding(deltaTime, Iterations);
       break; 
    case ECustomMovementMode::CMOVE_WallRunning:
       PhysWallRunning(deltaTime, Iterations); 
       break;
    case ECustomMovementMode::CMOVE_Grapling:
       PhysGrapling(deltaTime, Iterations);
       break;
    default:
       break;
    }
}

void USRCharacterMovementComponent::PhysWallRunning(float deltaTime, int32 Iterations)
{
    if (deltaTime < MIN_TICK_TIME) return;

    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CharacterOwner))
    {
       FGameplayTag HitTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact"));
       FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun"));

       if (ASC->HasMatchingGameplayTag(HitTag) || ASC->HasMatchingGameplayTag(StunTag))
       {
          WallRunCooldown = WallSeizeThreshold; 
          SetMovementMode(MOVE_Falling);
          StartNewPhysics(deltaTime, Iterations);
          return;
       }
    }

    FFindFloorResult FloorResult;
    FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
    
    if (FloorResult.IsWalkableFloor() && FloorResult.FloorDist <= MAX_FLOOR_DIST)
    {
       WallRunCooldown = WallSeizeThreshold;
       SetMovementMode(MOVE_Walking); 
       StartNewPhysics(deltaTime, Iterations);
       return;
    }

    if (!TryWallRun())
    {
       WallRunCooldown = WallSeizeThreshold; 
       SetMovementMode(MOVE_Falling);
       StartNewPhysics(deltaTime, Iterations);
       return;
    }

    TargetWallRunRoll = bIsRightWall ? -MaxWallRunRollAngle : MaxWallRunRollAngle;

    FVector WallRunDirection = FVector::CrossProduct(WallNormal, FVector::UpVector);
    if (FVector::DotProduct(CharacterOwner->GetActorForwardVector(), WallRunDirection) < 0.0f)
    {
       WallRunDirection = -WallRunDirection;
    }

    WallDir = WallRunDirection;

    FVector InputDir = Acceleration.GetSafeNormal();
    float ForwardIntent = FVector::DotProduct(InputDir, CharacterOwner->GetActorForwardVector());
    
    FVector LookDir = CharacterOwner->GetControlRotation().Vector();

    float TargetSpeed = 0.0f;
    float TargetZ = 0.0f;

    if (ForwardIntent > 0.1f) 
    {
       TargetSpeed = MaxWallWalkSpeed; 
    }
    else if (ForwardIntent < -0.1f) 
    {
       TargetSpeed = 0.0f;   
       TargetZ = -800.0f;    
    }
    else 
    {
       TargetSpeed = 0.0f;   
       TargetZ = -WallStickiness;    
    }

    Velocity.X = WallRunDirection.X * TargetSpeed;
    Velocity.Y = WallRunDirection.Y * TargetSpeed;
    Velocity.Z = FMath::FInterpTo(Velocity.Z, TargetZ, deltaTime, 8.0f);

    FVector Delta = (Velocity - (WallNormal * 150.0f)) * deltaTime;
    FHitResult Hit;
    SafeMoveUpdatedComponent(Delta, CharacterOwner->GetActorRotation(), true, Hit);

    if (Hit.IsValidBlockingHit())
    {
       if (Hit.Normal.Z >= GetWalkableFloorZ())
       {
          WallRunCooldown = WallSeizeThreshold;
          SetMovementMode(MOVE_Walking);
          StartNewPhysics(deltaTime, Iterations);
       }
       else
       {
          SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
       }
    }
}

bool USRCharacterMovementComponent::TryWallRun()
{
    if (!CharacterOwner) return false;
    if (IsMovingOnGround()) return false;
    if (!CharacterOwner->IsPlayerControlled()) return false;

    float CapsuleHalfHeight = CharacterOwner->GetSimpleCollisionHalfHeight();
    
    FVector Start = CharacterOwner->GetActorLocation();
    FVector DownEnd = Start - FVector(0.0f, 0.0f, CapsuleHalfHeight + 15.0f);

    FHitResult FloorHit;
    FCollisionQueryParams FloorQueryParams;
    FloorQueryParams.AddIgnoredActor(CharacterOwner);

    bool bHitFloor = GetWorld()->LineTraceSingleByChannel(FloorHit, Start, DownEnd, ECC_Visibility, FloorQueryParams);
    
    if (bHitFloor && FloorHit.Normal.Z >= GetWalkableFloorZ())
    {
       return false;
    }
    
    FVector RightVector = CharacterOwner->GetActorRightVector();
    float TraceLength = 70.0f; 
    
    FVector TraceDirRight;
    FVector TraceDirLeft;

    if (MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_WallRunning)
    {
       TraceDirRight = bIsRightWall ? -WallNormal : RightVector;
       TraceDirLeft = !bIsRightWall ? -WallNormal : -RightVector;
    }
    else
    {
       TraceDirRight = RightVector;
       TraceDirLeft = -RightVector;
    }

    FVector RightEnd = Start + (TraceDirRight * TraceLength);
    FVector LeftEnd = Start + (TraceDirLeft * TraceLength);

    // DrawDebugLine(GetWorld(), Start, RightEnd, FColor::Red, false, 2.0f, 0, 2.0f);
    // DrawDebugLine(GetWorld(), Start, LeftEnd, FColor::Green, false, 2.0f, 0, 2.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(CharacterOwner);

    bool bHitRight = GetWorld()->LineTraceSingleByChannel(HitResult, Start, RightEnd, ECC_GameTraceChannel3, QueryParams);
    if (bHitRight)
    {
       bIsRightWall = true;
       WallNormal = HitResult.ImpactNormal;
       WallHitLocationtion = HitResult.Location;
       return true;
    }

    bool bHitLeft = GetWorld()->LineTraceSingleByChannel(HitResult, Start, LeftEnd, ECC_GameTraceChannel3, QueryParams);
    if (bHitLeft)
    {
       bIsRightWall = false;
       WallNormal = HitResult.ImpactNormal;
       WallHitLocationtion = HitResult.Location;
       return true;
    }

    return false;
}

void USRCharacterMovementComponent::PhysSliding(float deltaTime, int32 Iterations)
{
    FVector FloorNormal = CurrentFloor.HitResult.Normal;
    Velocity = FVector::VectorPlaneProject(Velocity, FloorNormal);

    FVector GravityForce = FVector::DownVector * FMath::Abs(GetGravityZ());
    FVector SlopeAcceleration = FVector::VectorPlaneProject(GravityForce, FloorNormal);
    
    Velocity += SlopeAcceleration * deltaTime * SlideForce;
    Velocity -= Velocity * SlideFriction * deltaTime;

    FVector Delta = Velocity * deltaTime;
    FHitResult Hit;
    
    SafeMoveUpdatedComponent(Delta, CharacterOwner->GetActorRotation(), true, Hit);

    if (Hit.IsValidBlockingHit())
    {
       SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
    }

    FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false, NULL);

    if (CurrentFloor.IsWalkableFloor() && CurrentFloor.FloorDist > 0.0f && CurrentFloor.FloorDist < 50.0f)
    {
       FHitResult SnapHit;
       SafeMoveUpdatedComponent(FVector(0.0f, 0.0f, -CurrentFloor.FloorDist), CharacterOwner->GetActorRotation(), true, SnapHit);
    }

    if (USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh())
    {
       FVector SlideDirection = Velocity.GetSafeNormal();
       if (SlideDirection.IsNearlyZero())
       {
          SlideDirection = CharacterOwner->GetActorForwardVector();
       }
       
       FQuat TargetSlopeQuat = FRotationMatrix::MakeFromXZ(SlideDirection, CurrentFloor.HitResult.Normal).ToQuat();
       FQuat DefaultMeshLocalQuat = FRotator(0.0f, -90.0f, 0.0f).Quaternion(); 
       FQuat TargetMeshWorldQuat = TargetSlopeQuat * DefaultMeshLocalQuat;
       FQuat NewMeshQuat = FMath::QInterpTo(Mesh->GetComponentQuat(), TargetMeshWorldQuat, deltaTime, 10.0f);
        
       Mesh->SetWorldRotation(NewMeshQuat);
    }

    if (Velocity.SizeSquared2D() < FMath::Square(MinSlideSpeed))
    {
       ExitSlide();
    }
    else if (!CurrentFloor.IsWalkableFloor()) 
    {
       ExitSlide();
       SetMovementMode(MOVE_Falling);
    }
}

void USRCharacterMovementComponent::PhysGrapling(float deltaTime, int32 Iterations)
{
    FVector ToTarget = HookLocation - UpdatedComponent->GetComponentLocation();
    float DistToTarget = ToTarget.Size();
    FVector DashDirection = ToTarget.GetSafeNormal();

    if (DistToTarget < 100.0f) 
    {
       ExitGraple(); 
       return;
    }

    float GrappleDashSpeed = 3300.0f; 
    Velocity = DashDirection * GrappleDashSpeed;

    FVector Delta = Velocity * deltaTime;
    FHitResult Hit;
    SafeMoveUpdatedComponent(Delta, CharacterOwner->GetActorRotation(), true, Hit);

    if (Hit.IsValidBlockingHit())
    {
       SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
    }
}

void USRCharacterMovementComponent::EnterGraple(FVector InHookLocation)
{
    CharacterOwner->SetBase(nullptr);
    SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_Grapling);
    HookLocation = InHookLocation;
}

void USRCharacterMovementComponent::ExitGraple()
{
    if (MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_Grapling)
    {
       SetMovementMode(MOVE_Falling);
       Velocity *= 0.3f; 
    }
}
