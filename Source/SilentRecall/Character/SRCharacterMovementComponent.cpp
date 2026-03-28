// Fill out your copyright notice in the Description page of Project Settings.


#include "SRCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"


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
	
    if (MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_WallRunning)
    {
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

    if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::CMOVE_WallRunning)
    {

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

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::CMOVE_Sliding)
	{
		// 현재 모드가 슬라이딩이 확실하게 아닐 때만 초기화 실행
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
			// 언리얼 캐릭터 메쉬의 기본 로컬 회전값 (Pitch 0, Yaw -90, Roll 0)
			// 주의: 만약 블루프린트에서 메쉬의 기본 회전값을 다르게 설정하셨다면 그 값을 넣어야 합니다.
			FRotator DefaultRotation(0.0f, -90.0f, 0.0f);
            
			// 메쉬의 상대 회전을 캡슐 기준 올바른 정렬 상태로 즉시 되돌립니다.
			Mesh->SetRelativeRotation(DefaultRotation);
		}
	}
}

void USRCharacterMovementComponent::DoSlideJump()
{
	// 슬라이딩 중이 아니면 무시
	if (CustomMovementMode != ECustomMovementMode::CMOVE_Sliding) return;

	// 1. 슬라이딩 강제 해제 (캡슐 크기 원래대로 복구)
	ExitSlide();

	// 2. 현재 미끄러지던 속도 보존 (X, Y축 관성)
	FVector CurrentMomentum = Velocity;
	// (선택 사항) 점프 시 기존 수직 속도(떨어지던 속도 등)는 무시하고 싶다면 0으로 초기화
	CurrentMomentum.Z = 0.0f; 

	// 3. 위로 솟구치는 기본 점프력 (엔진 기본 JumpZVelocity 활용)
	FVector JumpUpForce = FVector::UpVector * JumpZVelocity;

	// 4. 앞으로 강하게 튕겨 나가는 추가 슬라이드 추진력!
	// 미끄러지던 방향(Normal)을 구해서 커스텀 힘(SlideJumpForce)만큼 밀어줍니다.
	FVector ForwardBoost = CurrentMomentum.GetSafeNormal2D() * SlideJumpForce;

	// 5. 최종 속도 덮어쓰기 = 기존 관성 + 점프력 + 슬라이드 부스트
	Velocity = CurrentMomentum + JumpUpForce + ForwardBoost;

	// 6. 엔진에 "나 지금 허공에 떴어!" 라고 수동으로 상태 보고 (월 점프와 동일!)
	SetMovementMode(MOVE_Falling);

	// 7. 점프 카운트 수동 증가 (더블 점프를 위해)
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

	// 현재 공중에 떠 있고(Falling) && 이동 키(WASD)를 누르고 있을 때만 벽 타기 시도
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
		break; // 슬라이딩 전용 물리 함수 호출
	case ECustomMovementMode::CMOVE_WallRunning:
		PhysWallRunning(deltaTime, Iterations); // 벽 타기 전용 물리 연산 함수 호출
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

	WallDir = WallRunDirection;

	FVector InputDir = Acceleration.GetSafeNormal();
	float ForwardIntent = FVector::DotProduct(InputDir, CharacterOwner->GetActorForwardVector());
	
	FVector LookDir = CharacterOwner->GetControlRotation().Vector();

	float TargetSpeed = 0.0f;
	float TargetZ = 0.0f;

	// 조작 의도 판별
	if (ForwardIntent > 0.1f) 
	{
		TargetSpeed = MaxWallWalkSpeed; // 앞(W) 누름: 전진
		TargetZ = LookDir.Z * MaxWallWalkSpeed;       // 높이 유지
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
	float TraceLength = 70.0f; // 필요시 100.0f 등으로 늘려보세요
    
	FVector TraceDirRight;
	FVector TraceDirLeft;

	// [핵심] 이미 벽을 타는 중이라면, 내 몸의 회전과 무관하게 '벽이 있는 방향(-WallNormal)'으로 레이를 쏩니다.
	if (MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_WallRunning)
	{
		// bIsRightWall 상태에 따라 방향을 맞춰줍니다.
		TraceDirRight = bIsRightWall ? -WallNormal : RightVector;
		TraceDirLeft = !bIsRightWall ? -WallNormal : -RightVector;
	}
	else
	{
		// 벽을 타기 전이라면 내 몸의 좌우로 쏩니다.
		TraceDirRight = RightVector;
		TraceDirLeft = -RightVector;
	}

	FVector RightEnd = Start + (TraceDirRight * TraceLength);
	FVector LeftEnd = Start + (TraceDirLeft * TraceLength);

	DrawDebugLine(GetWorld(), Start, RightEnd, FColor::Red, false, 2.0f, 0, 2.0f);
	DrawDebugLine(GetWorld(), Start, LeftEnd, FColor::Green, false, 2.0f, 0, 2.0f);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);

	// 1. 오른쪽 벽 검사
	bool bHitRight = GetWorld()->LineTraceSingleByChannel(HitResult, Start, RightEnd, ECC_GameTraceChannel3, QueryParams);
	if (bHitRight)
	{
		bIsRightWall = true;
		WallNormal = HitResult.ImpactNormal;
		WallHitLocationtion = HitResult.Location;
		return true;
		
	}

	// 2. 왼쪽 벽 검사
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
	// 현재 바닥의 기울기
	FVector FloorNormal = CurrentFloor.HitResult.Normal;

	// ⭐️ 핵심 1: 내 원래 속도를 바닥 표면에 완벽하게 눕힙니다! (땅 파고들기 원천 차단)
	Velocity = FVector::VectorPlaneProject(Velocity, FloorNormal);

	// 가속도 계산 및 속도 업데이트
	FVector GravityForce = FVector::DownVector * FMath::Abs(GetGravityZ());
	FVector SlopeAcceleration = FVector::VectorPlaneProject(GravityForce, FloorNormal);
    
	Velocity += SlopeAcceleration * deltaTime;
	Velocity -= Velocity * SlideFriction * deltaTime;

	// 이동 실행
	FVector Delta = Velocity * deltaTime;
	FHitResult Hit;
    
	// 1차 이동: 이제 Delta가 바닥과 완벽히 평행하므로 부딪히지 않고 빙판처럼 미끄러집니다.
	SafeMoveUpdatedComponent(Delta, CharacterOwner->GetActorRotation(), true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	// 이동을 마친 후 바닥 정보 새로고침
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false, NULL);

	// ⭐️ 핵심 2: 자석 스냅 (Magnetic Snap)
	// 경사가 꺾여서 바닥에서 발이 미세하게(50 미만으로) 떴다면? 강제로 끌어내려서 바닥에 붙입니다!
	if (CurrentFloor.IsWalkableFloor() && CurrentFloor.FloorDist > 0.0f && CurrentFloor.FloorDist < 50.0f)
	{
		FHitResult SnapHit;
		// 남은 거리만큼 밑으로 꽂아버림
		SafeMoveUpdatedComponent(FVector(0.0f, 0.0f, -CurrentFloor.FloorDist), CharacterOwner->GetActorRotation(), true, SnapHit);
	}

	if (USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh())
	{
		// 1. 슬라이딩 방향 구하기 (속도가 0에 가까우면 액터의 정면을 기준)
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
	// ❌ 중력 적용 끄기! (고스트러너는 날아갈 때 밑으로 안 떨어집니다)
	// Velocity += FVector(0.f, 0.f, GetGravityZ()) * deltaTime; (삭제)

	// 1. 목적지를 향하는 방향과 현재 남은 거리 계산
	FVector ToTarget = HookLocation - UpdatedComponent->GetComponentLocation();
	float DistToTarget = ToTarget.Size();
	FVector DashDirection = ToTarget.GetSafeNormal();

	// ⭐️ 2. 도착 판정 (목표 지점에 1미터 이내로 근접했는가?)
	// 너무 0까지 딱 붙으려 하면 벽에 박혀서 버그가 생기니, 적당히 도착하면 줄을 놓게 합니다.
	if (DistToTarget < 100.0f) 
	{
		ExitGraple(); // 훅 도착! 스킬 종료!
		return;
	}

	// ⭐️ 3. 고스트러너의 쾌감: 초고속 직선 속도 덮어쓰기!
	// 기존 속도를 무시하고 목적지를 향해 무조건 고정 속도로 날아갑니다.
	float GrappleDashSpeed = 3300.0f; // 💡 엄청 빠릅니다! 게임 템포에 맞게 조절하세요.
	Velocity = DashDirection * GrappleDashSpeed;

	// 4. 실제 이동 적용
	FVector Delta = Velocity * deltaTime;
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, CharacterOwner->GetActorRotation(), true, Hit);

	// 5. 날아가다가 다른 벽이나 장애물에 부딪혔다면?
	if (Hit.IsValidBlockingHit())
	{
		// 미끄러지게 두거나, 아니면 부딪힌 즉시 스킬을 취소할 수도 있습니다.
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
        
		// 만약 중간에 장애물에 박았을 때 줄을 놓게 하고 싶다면 아래 주석을 푸세요.
		// ExitGraple(); 
	}
}

void USRCharacterMovementComponent::EnterGraple(FVector InHookLocation)
{
	// 바닥 마찰력 무시 (기존과 동일)
	CharacterOwner->SetBase(nullptr);

	// 커스텀 무브먼트 상태 진입
	SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_Grapling);
    
	// 목적지 저장
	HookLocation = InHookLocation;
    
	// (더 이상 PhysicsRopeLength 같은 복잡한 변수도 필요 없습니다!)
}

void USRCharacterMovementComponent::ExitGraple()
{
	if (MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_Grapling)
	{
		// 다시 중력을 받는 일반 낙하 상태로 복귀
		SetMovementMode(MOVE_Falling);
        
		// 💡 팁: 날아가던 관성을 살짝 줄여주면 도착 지점에서 컨트롤하기 편합니다.
		Velocity *= 0.3f; 
		
	}
}
