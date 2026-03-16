// Fill out your copyright notice in the Description page of Project Settings.


#include "SRCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"


USRCharacterMovementComponent::USRCharacterMovementComponent()
{
	
}

void USRCharacterMovementComponent::DoWallJump()
{
	if (CustomMovementMode != CMOVE_WallRunning) return;

	FVector CurrentMomentum = Velocity;

	CurrentMomentum.Z = 0.0f;

	FVector JumpUpForce = FVector::UpVector * WallJumpHeight; 

	FVector BasePushOff = WallNormal * WallRepulsiveForce; 

	FVector InputDir = Acceleration.GetSafeNormal();
	FVector InputForce = FVector::ZeroVector;

	if (!InputDir.IsNearlyZero())
	{
		InputForce = InputDir * WallInputCorrection;

	}
	else
	{
		InputForce = CharacterOwner->GetActorForwardVector() * WallPropulsionForce;
	}

	Velocity = BasePushOff + JumpUpForce + InputForce + CurrentMomentum;

	WallRunCooldown = WallSeizeThreshold; 
	SetMovementMode(MOVE_Falling);

	CharacterOwner->JumpCurrentCount++;
}

void USRCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	
	if (WallRunCooldown > 0.0f)
	{
		WallRunCooldown -= DeltaSeconds;
		return; 
	}

	// 현재 공중에 떠 있고(Falling) && 이동 키(WASD)를 누르고 있을 때만 벽 타기 시도
	if (MovementMode == MOVE_Falling && !Acceleration.IsNearlyZero())
	{
		if (TryWallRun())
		{
			TimeOnWall = 0.0f; 
			SetMovementMode(MOVE_Custom, CMOVE_WallRunning);
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
	case CMOVE_WallRunning:
		PhysWallRunning(deltaTime, Iterations); // 벽 타기 전용 물리 연산 함수 호출
		break;
	default:
		break;
	}
}

void USRCharacterMovementComponent::PhysWallRunning(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	if (!TryWallRun())
    {
        WallRunCooldown = WallSeizeThreshold; 
        SetMovementMode(MOVE_Falling);
        StartNewPhysics(deltaTime, Iterations);
        return;
    }

	FVector WallRunDirection = FVector::CrossProduct(WallNormal, FVector::UpVector);
	if (FVector::DotProduct(CharacterOwner->GetActorForwardVector(), WallRunDirection) < 0.0f)
	{
		WallRunDirection = -WallRunDirection;
	}

	FVector InputDir = Acceleration.GetSafeNormal();
	float ForwardIntent = FVector::DotProduct(InputDir, WallRunDirection);

	float TargetSpeed = 0.0f;
	float TargetZ = 0.0f;

	// 조작 의도 판별
	if (ForwardIntent > 0.1f) 
	{
		TargetSpeed = MaxWallWalkSpeed; // 앞(W) 누름: 전진
		TargetZ = 0.0f;       // 높이 유지
	}
	else if (ForwardIntent < -0.1f) 
	{
		TargetSpeed = 0.0f;   // 뒤(S) 누름: 전진 멈춤
		TargetZ = -800.0f;    // 묵직하게 강하!
	}
	else 
	{
		TargetSpeed = 0.0f;   // 가만히 있음
		TargetZ = -WallStickiness;    // 젠틀하게 스르륵 미끄러짐
	}

	// [핵심 1] 속도를 확 바꾸지 않고, 목표 속도(TargetZ)를 향해 부드럽게 보간(Lerp)합니다.
	// 이렇게 하면 미끄러지다가 W를 누르면 다시 스무스하게 0으로 올라옵니다.
	Velocity.X = WallRunDirection.X * TargetSpeed;
	Velocity.Y = WallRunDirection.Y * TargetSpeed;
	Velocity.Z = FMath::FInterpTo(Velocity.Z, TargetZ, deltaTime, 8.0f);

	// [핵심 2] 이동 처리 (제자리 달리기 버그 픽스)
	// 150의 힘으로 벽에 붙이는 건 유지하되, Delta에만 적용해서 충돌을 부드럽게 뺍니다.
	FVector Delta = (Velocity - (WallNormal * 150.0f)) * deltaTime;
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, CharacterOwner->GetActorRotation(), true, Hit);

	// 만약 벽에 너무 세게 박혀서(Hit) 이동이 멈췄다면? 
	if (Hit.IsValidBlockingHit())
	{
		// 바닥에 닿은 경우 걷기 모드로
		if (Hit.Normal.Z >= GetWalkableFloorZ())
		{
			WallRunCooldown = WallSeizeThreshold;
			SetMovementMode(MOVE_Walking);
			StartNewPhysics(deltaTime, Iterations);
		}
		else
		{
			// [제자리 달리기 해결] 벽에 부딪히면 멈추지 말고 벽을 따라 미끄러지며 이동해라!
			SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}
	}
}

bool USRCharacterMovementComponent::TryWallRun()
{
	if (!CharacterOwner) return false;
	if (IsMovingOnGround()) return false;

	if (!CharacterOwner->IsPlayerControlled()) return false;

	FVector Start = CharacterOwner->GetActorLocation();
	FVector DownEnd = Start - FVector(0.0f, 0.0f, 150.0f);

	DrawDebugLine(GetWorld(), Start, DownEnd, FColor::Blue, false, 2.0f, 0, 2.0f);

	FHitResult FloorHit;
	FCollisionQueryParams FloorQueryParams;
	FloorQueryParams.AddIgnoredActor(CharacterOwner);

	bool bHitFloor = GetWorld()->LineTraceSingleByChannel(FloorHit, Start, DownEnd, ECC_Visibility, FloorQueryParams);
    
	if (bHitFloor)
	{
		return false; 
	}
	
	FVector RightVector = CharacterOwner->GetActorRightVector();
	FVector ForwardVector = CharacterOwner->GetActorForwardVector();

	// 플레이어의 좌우로 약 70 유닛 정도 레이를 쏩니다.
	float TraceLength = 70.0f; 
	FVector RightEnd = Start + (RightVector * TraceLength);
	FVector LeftEnd = Start - (RightVector * TraceLength);

	DrawDebugLine(GetWorld(), Start, RightEnd, FColor::Red, false, 2.0f, 0, 2.0f);
	DrawDebugLine(GetWorld(), Start, LeftEnd, FColor::Green, false, 2.0f, 0, 2.0f);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);

	// 1. 오른쪽 벽 검사
	bool bHitRight = GetWorld()->LineTraceSingleByChannel(HitResult, Start, RightEnd, ECC_Visibility, QueryParams);
	if (bHitRight)
	{
		bIsRightWall = true;
		WallNormal = HitResult.ImpactNormal;
		return true;
	}

	// 2. 왼쪽 벽 검사
	bool bHitLeft = GetWorld()->LineTraceSingleByChannel(HitResult, Start, LeftEnd, ECC_Visibility, QueryParams);
	if (bHitLeft)
	{
		bIsRightWall = false;
		WallNormal = HitResult.ImpactNormal;
		return true;
	}

	return false;
}


