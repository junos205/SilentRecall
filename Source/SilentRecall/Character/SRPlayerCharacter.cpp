// Fill out your copyright notice in the Description page of Project Settings.

#include "SRPlayerCharacter.h"
#include "NiagaraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "KismetTraceUtils.h"
#include "Camera/CameraComponent.h"
#include "Character/SRCharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Interface/InteractableInterface.h"

ASRPlayerCharacter::ASRPlayerCharacter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<USRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    
	// ⭐️ 평소에는 캐릭터의 Tick을 아예 꺼버립니다! (성능 최적화 100%)
	SetActorTickEnabled(false);
	
	bUseControllerRotationYaw = true;   // 캐릭터가 마우스 좌우 회전을 따라감
	bUseControllerRotationPitch = false; // 1인칭이라도 캐릭터 몸체가 위아래로 기울어지면 안 됨
	bUseControllerRotationRoll = false;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetMesh(), TEXT("neck_01"));
	Camera->bUsePawnControlRotation = true;

	GrappleCable = CreateDefaultSubobject<UCableComponent>(TEXT("GrappleCable"));
	
	// 💡 팁: 특정 손목 뼈(Socket)에서 나가게 하고 싶다면 이렇게 씁니다.
	GrappleCable->SetupAttachment(GetRootComponent());
	// 3. 초기 기본값 세팅 (평소엔 안 보이고, 길이는 0이어야 함)
	GrappleCable->SetVisibility(false);
	GrappleCable->CableLength = 0.0f;
    
	// 4. 물리/시각적 퀄리티 세팅 (에디터에서도 수정 가능)
	GrappleCable->NumSegments = 10; // 관절 수를 줄여서 빳빳하게 만듦 (기본 20 -> 10)
	GrappleCable->SolverIterations = 4;  // 밧줄이 꺾이는 관절 수 (부드러움)
	GrappleCable->CableWidth = 5.0f;   // 밧줄의 두께
	GrappleCable->EndLocation = FVector::ZeroVector; // 끝점 로컬 좌표 초기화

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void ASRPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocallyControlled())
	{
		GetMesh()->HideBoneByName(TEXT("head"), PBO_None);
	}
}

void ASRPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GrappleState != EGrappleState::Idle && GrappleCable)
	{
		// ⭐️ 핵심: 케이블은 캡슐에 달려있지만, '시작점'은 매 프레임 내 손목 뼈 위치로 강제 이동시킵니다!
		FVector HandLocation = GetMesh()->GetSocketLocation(FName("hand_r_Socket"));
		GrappleCable->SetWorldLocation(HandLocation);

		switch (GrappleState)
		{
		case EGrappleState::Deploying:
			{
				// 목표를 향해 월드 좌표 이동
				CurrentCableEndLocation = FMath::VInterpConstantTo(CurrentCableEndLocation, GrappleTargetLocation, DeltaTime, DeploySpeed);

				// 이제 트랜스폼이 완벽하게 깨끗하므로 이 공식이 기가 막히게 작동합니다.
				GrappleCable->EndLocation = GrappleCable->GetComponentTransform().InverseTransformPosition(CurrentCableEndLocation);
                
				GrappleCable->CableLength = FVector::Distance(HandLocation, CurrentCableEndLocation);

				if (FVector::DistSquared(CurrentCableEndLocation, GrappleTargetLocation) < 10.0f)
				{
					if (USRCharacterMovementComponent* SRMovement = Cast<USRCharacterMovementComponent>(GetCharacterMovement()))
					{
						SRMovement->EnterGraple(GrappleTargetLocation);
					}
					GrappleState = EGrappleState::Swinging;
				}
				break;
			}
		case EGrappleState::Swinging:
			{
				// 스윙 중 밧줄 끝 고정
				GrappleCable->EndLocation = GrappleCable->GetComponentTransform().InverseTransformPosition(GrappleTargetLocation);
				GrappleCable->CableLength = FVector::Distance(HandLocation, GrappleTargetLocation);
				break;
			}
		case EGrappleState::Retracting:
			{
				// 회수 중
				CurrentCableEndLocation = FMath::VInterpConstantTo(CurrentCableEndLocation, HandLocation, DeltaTime, RetractSpeed);
                
				GrappleCable->EndLocation = GrappleCable->GetComponentTransform().InverseTransformPosition(CurrentCableEndLocation);
				GrappleCable->CableLength = FVector::Distance(HandLocation, CurrentCableEndLocation);

				if (FVector::DistSquared(CurrentCableEndLocation, HandLocation) < 100.0f)
				{
					GrappleState = EGrappleState::Idle;
					GrappleCable->SetVisibility(false);
					GrappleCable->EndLocation = FVector::ZeroVector; 
					SetActorTickEnabled(false);
				}
				break;
			}
		}
	}

	if (bIsVaulting && Controller)
	{
		// 1. 현재 소켓의 실시간 각도 가져오기
		FRotator CurrentSocketRot = GetMesh()->GetSocketRotation(TEXT("CameraSocket")); 
        
		// ⭐️ 2. [핵심] 시작할 때보다 소켓이 얼마나 움직였는가(변동폭)?
		FRotator SocketDelta = (CurrentSocketRot - InitialSocketRot).GetNormalized();
        
		// ⭐️ 3. 내가 원래 쳐다보던 카메라 방향에 그 변동폭만큼만 똑같이 더해줍니다!
		FRotator TargetRot = (InitialControlRot + SocketDelta).GetNormalized();
        
		FRotator CurrentRot = Controller->GetControlRotation();
		FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 15.0f);

		Controller->SetControlRotation(NewRot);
	}

	// 마우스 먹통 방지 안전장치
	if (bIsVaulting)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			if (!AnimInstance->IsAnyMontagePlaying()) EndVault(nullptr, true); 
		}
	}
}

void ASRPlayerCharacter::Move(const FInputActionValue& Value)
{
	FVector2d AxisValue = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator MoveRotation = GetControlRotation();
		const FRotator YawRotation = FRotator(0.0f, MoveRotation.Yaw, 0.0f);
		const FVector ForwardVector = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightVector = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		
		AddMovementInput(ForwardVector, AxisValue.Y);
		AddMovementInput(RightVector, AxisValue.X);
	}
	
}

void ASRPlayerCharacter::Look(const FInputActionValue& Value)
{
	FVector2d AxisValue = Value.Get<FVector2D>();
	
	if (Controller != nullptr)
	{
		AddControllerYawInput(AxisValue.X);

		AddControllerPitchInput(-AxisValue.Y);
	}
}

void ASRPlayerCharacter::Jump()
{
	if (TryVault())
	{
		return;
	}

	if (USRCharacterMovementComponent* SRMovement = Cast<USRCharacterMovementComponent>(GetCharacterMovement()))
	{
		if (SRMovement->MovementMode == MOVE_Custom && SRMovement->CustomMovementMode == CMOVE_WallRunning)
		{
			SRMovement->DoWallJump();
			return;
		}
		else if (SRMovement->MovementMode == MOVE_Custom && SRMovement->CustomMovementMode == CMOVE_Sliding)
		{
			SRMovement->DoSlideJump();
			return;
		}
	}

	JumpMaxCount = 2;
	
	Super::Jump();
}

void ASRPlayerCharacter::Slide(const FInputActionValue& Value)
{
	if (USRCharacterMovementComponent* SRMovement = Cast<USRCharacterMovementComponent>(GetCharacterMovement()))
	{
		SRMovement->EnterSlide();
	}
}

bool ASRPlayerCharacter::TryVault()
{
    if (bIsVaulting) return false;

    FVector LedgeLocation;
    FVector WallNormal;
    EParkourType ParkourType = DetectLedge(LedgeLocation, WallNormal);

    if (ParkourType == EParkourType::None) return false;

    FVector ForwardDir = (-WallNormal).GetSafeNormal(); 
    FRotator TargetRotation = ForwardDir.Rotation();
    float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius(); 

    FVector Target1Location; 
    FVector Target2Location; 
    UAnimMontage* SelectedMontage = nullptr;

	switch (ParkourType)
	{
	case EParkourType::LowVault:
		{
			// ⭐️ 타겟 1 (손 짚을 때): XY 평면은 벽면에서 30cm 앞, Z(높이)는 장애물 옥상 높이!
			Target1Location = LedgeLocation + (WallNormal * 30.0f); 
			Target1Location.Z = LedgeLocation.Z; // <-- 내 발바닥이 아니라 옥상 높이로 수정!

			// ⭐️ 타겟 2 (착지할 때): 장애물을 완전히 넘어간 앞쪽 바닥
			Target2Location = LedgeLocation + (ForwardDir * 120.0f); 
			Target2Location.Z = GetActorLocation().Z; 
        
			SelectedMontage = LowVaultMontage;
			break;
		}

	case EParkourType::HighMantle:
		{
			// ⭐️ 타겟 1 (손 짚을 때): XY 좌표는 옥상 모서리 살짝 앞
			Target1Location = LedgeLocation + (WallNormal * 50.0f);
          
			// [핵심 튜닝 포인트] 손을 짚을 때, '발바닥'이 옥상 기준 몇 cm 아래에 있어야 자연스러울까요?
			// (보통 사람 키 기준으로 가슴 높이인 100~120cm를 빼면 손이 예쁘게 모서리에 걸립니다!)
			// ⭐️ [핵심 튜닝 포인트] 캐릭터를 더 아래로 끌어내리기 위해 값을 확 키웁니다!
			// 기존 110.0f에서 140.0f ~ 160.0f 정도로 늘리면 캐릭터의 허리(Root)가 훅 내려갑니다.
			float VaultHandHeightOffset = 200.0f; // ⬅️ 값을 '키울수록' 캐릭터는 더 '아래로' 내려갑니다.
			Target1Location.Z = LedgeLocation.Z - VaultHandHeightOffset;

			// ⭐️ 타겟 2 (착지할 때): 옥상 위쪽으로 1미터 전진
			Target2Location = LedgeLocation + (ForwardDir * 100.0f); 
			// 착지할 땐 발바닥이 옥상 표면에 정확히 닿아야 하므로 옥상 높이 그대로!
			Target2Location.Z = LedgeLocation.Z; 
        
			// 🔴🔵 [초강력 디버그 툴] 모션 워핑이 도대체 내 캐릭터 발바닥을 어디로 당기고 있는지 눈으로 직접 봅니다!
			DrawDebugSphere(GetWorld(), Target1Location, 10.0f, 16, FColor::Red, false, 5.0f);  // 빨간공: 손 짚을 때 내 발의 위치
			DrawDebugSphere(GetWorld(), Target2Location, 10.0f, 16, FColor::Blue, false, 5.0f); // 파란공: 착지할 때 내 발의 위치

			SelectedMontage = HighMantleMontage;
			break;
		}

	default:
		break;
	}
    if (!SelectedMontage) return false;

    // ⭐️ [복구 완료!] 대망의 모션 워핑 타겟 입력부 
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
       FName("VaultHandTarget"), Target1Location, TargetRotation
    );

    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
       FName("VaultLandTarget"), Target2Location, TargetRotation
    );

    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    if (GetCapsuleComponent()) GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    float AnimDuration = PlayAnimMontage(SelectedMontage);
    
    if (AnimDuration > 0.0f)
    {
       bIsVaulting = true;
       
       // 1. 미끄러짐 완벽 방지: 달려오던 가속도를 즉시 0으로 만듭니다!
       if (GetCharacterMovement())
       {
          GetCharacterMovement()->StopMovementImmediately();
       }

       // ⭐️ 2. [복구 완료!] 헬기 버그 방지: 파쿠르 중엔 카메라가 몸통을 못 돌리게 끊어줍니다.
       bUseControllerRotationYaw = false;

       if (APlayerController* PC = Cast<APlayerController>(Controller))
       {
          // 3. 시작 시점의 소켓 각도와 카메라 각도를 '찰칵' 찍어 기억해둡니다.
          InitialSocketRot = GetMesh()->GetSocketRotation(TEXT("CameraSocket"));
          InitialControlRot = PC->GetControlRotation();

          PC->SetIgnoreLookInput(true); 
       }
       
       SetActorTickEnabled(true); 

       if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
       {
          FOnMontageEnded EndDelegate;
          EndDelegate.BindUObject(this, &ASRPlayerCharacter::EndVault);
          AnimInstance->Montage_SetEndDelegate(EndDelegate, SelectedMontage);
       }
       return true;
    }
    return false;
}

void ASRPlayerCharacter::EndVault(UAnimMontage* Montage, bool bInterrupted)
{
	if (GetCapsuleComponent()) GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		GetCharacterMovement()->Velocity = GetActorForwardVector() * 200.0f;
	}
	
	bIsVaulting = false;
    
	// ⭐️ [버그 해결 코드] 파쿠르가 끝났으니 다시 마우스가 몸통을 돌릴 수 있게 연동을 켜줍니다!
	bUseControllerRotationYaw = true; 

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		// 카메라 삐딱해짐 방지 (Roll 초기화)
		FRotator ResetRot = PC->GetControlRotation();
		ResetRot.Roll = 0.0f; 
		PC->SetControlRotation(ResetRot);

		PC->ResetIgnoreLookInput();
	}

	if (GrappleState == EGrappleState::Idle)
	{
		SetActorTickEnabled(false);
	}
}

EParkourType ASRPlayerCharacter::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
	FVector StartLocation = GetActorLocation();
	FVector ForwardVector = GetActorForwardVector();
    
	// ⭐️ 탐색 거리 증가: 이제 1.5미터 앞(150.0f)에서 스페이스바를 눌러도 파쿠르가 발동합니다!
	float TraceDistance = 300.0f; 
	FVector EndLocation = StartLocation + (ForwardVector * TraceDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(30.0f);

	FHitResult ForwardHit;
	bool bWallHit = GetWorld()->SweepSingleByChannel(ForwardHit, StartLocation, EndLocation, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);

	FColor WallResultColor = bWallHit ? FColor::Red : FColor::Green;
	
	DrawDebugCapsule(GetWorld(), StartLocation, ((EndLocation - StartLocation) * 0.5f).Length(), SphereShape.GetCapsuleRadius(), FQuat::Identity, WallResultColor);
	
	if (bWallHit)
	{
		OutWallNormal = ForwardHit.Normal;
       
		// 위에서 아래로 쏘는 위치도 살짝 수정 (벽 안쪽으로 30cm만 들어가서 쏩니다)
		FVector DownStart = ForwardHit.Location + (ForwardVector * 30.0f) + (FVector::UpVector * 250.0f); 
		FVector DownEnd = DownStart - (FVector::UpVector * 250.0f);

		// ... (이하 DownHit, LedgeHeight 검사 및 분류 로직은 기존과 100% 동일)
		FHitResult DownHit;
		FCollisionShape DownShape = FCollisionShape::MakeSphere(5.0f);

		bool bHitLedge = GetWorld()->SweepSingleByChannel(DownHit, DownStart, DownEnd, FQuat::Identity, ECC_Visibility, DownShape, QueryParams);

		if (bHitLedge)
		{
			float CharacterFeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			float LedgeHeight = DownHit.Location.Z - CharacterFeetZ;

			FVector StandLocation = DownHit.Location + (FVector::UpVector * 90.0f); 
			FCollisionShape CharacterCapsule = FCollisionShape::MakeCapsule(40.0f, 90.0f); 

			bool bIsBlocked = GetWorld()->OverlapAnyTestByChannel(StandLocation, FQuat::Identity, ECC_Visibility, CharacterCapsule, QueryParams);

			if (!bIsBlocked)
			{
				OutLedgeLocation = FVector(ForwardHit.Location.X, ForwardHit.Location.Y, DownHit.Location.Z);
				OutWallNormal = ForwardHit.Normal;

				if (LedgeHeight > 60.0f && LedgeHeight <= 130.0f)
				{
					return EParkourType::LowVault;
				}
				else if (LedgeHeight > 130.0f && LedgeHeight <= 250.0f)
				{
					return EParkourType::HighMantle;
				}
			}
		}
		return EParkourType::None; // 60cm 미만이거나 블로킹된 경우 파쿠르 안 함
	}
	return EParkourType::None;
}

void ASRPlayerCharacter::OnInteract(const FInputActionValue& Value)
{
	UCameraComponent* CameraComp = FindComponentByClass<UCameraComponent>();
    if (!CameraComp) return;

    // 1. 카메라 정보 계산
    FVector StartLoc = CameraComp->GetComponentLocation();
    FVector LookDir = CameraComp->GetForwardVector();
    FVector EndLoc = StartLoc + (LookDir * InteractDistance);

    // ⭐️ 2. 멀티 스피어 트레이스 세팅
    TArray<FHitResult> HitResults; // 여러 녀석이 맞을 예정
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(InteractTraceRadius); // 두툼한 두께!

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); // 나 무시

    // ⭐️ 3. 멀티 스피어 트레이스 발사! (LineTraceSingle ➡️ SweepMulti로 변경)
    // 눈에는 안 보이지만 두툼한 원기둥을 쏘는 것과 같습니다.
    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults, 
        StartLoc, 
        EndLoc, 
        FQuat::Identity, // 회전 없음
        ECC_Visibility, 
        SphereShape, 
        Params
    );
	FVector CapsuleCenter = StartLoc + (LookDir * InteractDistance * 0.5f);
	float HalfHeight = InteractDistance * 0.5f;
    
	// 마법의 회전 공식: 캡슐의 위아래(Z축)를 내 시선(LookDir) 방향으로 눕혀줍니다!
	FQuat CapsuleRot = FRotationMatrix::MakeFromZ(LookDir).ToQuat();

	FColor CapsuleColor = FColor::Red;
	
    
    // ⭐️ 5. (유저 요청) 시선과 가장 가까운 아이템 고르기 로직
    if (bHit)
    {
        AActor* BestTarget = nullptr;
        float MinAngle = 180.0f; // 초기값은 최대 각도

        // 맞은 모든 녀석을 순회합니다!
        for (const FHitResult& Hit : HitResults)
        {
            AActor* PotentialActor = Hit.GetActor();
            
            // 상호작용 인터페이스를 달고 있는 녀석만 검사! (가장 중요)
            if (PotentialActor && PotentialActor->Implements<UInteractableInterface>())
            {
                // 🎯 플레이어 시선(LookDir)과 플레이어➡️아이템 방향 벡터 사이의 각도를 구합니다.
                FVector ToActorDir = (PotentialActor->GetActorLocation() - StartLoc).GetSafeNormal();
                
                // 내적(Dot Product)을 이용해 코사인 각도를 구하고, 이를 도(Degree) 단위로 바꿉니다.
                float Dot = FVector::DotProduct(LookDir, ToActorDir);
                float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));

            	CapsuleColor = FColor::Green;

                // 여태까지 찾은 각도 중 최소 각도(가장 정면에 가까운)를 갱신합니다.
                if (Angle < MinAngle)
                {
                    MinAngle = Angle;
                    BestTarget = PotentialActor;
                }
            }
        }
    	
        // ⭐️ 6. 최후의 승자(가장 각도가 가까운)에게만 상호작용 실행!
        if (BestTarget)
        {
            IInteractableInterface::Execute_Interact(BestTarget, this);
            
            // (디버그) 당첨된 녀석은 파란색 다이아몬드로 표시
            DrawDebugSolidBox(GetWorld(), BestTarget->GetActorLocation(), FVector(10.0f), FColor::Blue, false, 2.0f);
        }
    }
	DrawDebugCapsule(GetWorld(), CapsuleCenter, HalfHeight, InteractTraceRadius, CapsuleRot, CapsuleColor, false, 2.0f, 0, 1.0f);

}

void ASRPlayerCharacter::StartGrapple(FVector TargetLocation)
{
	GrappleTargetLocation = TargetLocation;

	// 케이블 컴포넌트를 찾아서 캐싱해 둡니다. (매 틱마다 찾으면 성능 낭비!)
	if (!GrappleCable)
	{
		GrappleCable = FindComponentByClass<UCableComponent>();
	}

	if (GrappleCable)
	{
		// ⭐️ 발사 준비! 내 손(케이블 시작점)에서부터 줄이 출발합니다.
		GrappleCable->SetAttachEndTo(nullptr, NAME_None);
		CurrentCableEndLocation = GrappleCable->GetComponentLocation();
		GrappleCable->SetVisibility(true);
		GrappleCable->CableLength = 1.0f;

		GrappleState = EGrappleState::Deploying;
	}

	SetActorTickEnabled(true);
}

void ASRPlayerCharacter::StopGrapple()
{
	if (GrappleState == EGrappleState::Swinging || GrappleState == EGrappleState::Deploying)
	{
		// ⭐️ 가장 먼저 실제 물리 엔진 스윙을 꺼서 관성으로 날아가게 만듭니다!
		if (USRCharacterMovementComponent* SRMovement = Cast<USRCharacterMovementComponent>(GetCharacterMovement()))
		{
			SRMovement->ExitGraple();
		}

		// 상태를 회수로 변경 (이제 Tick 함수가 줄을 감기 시작합니다)
		GrappleState = EGrappleState::Retracting;
	}
}

void ASRPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	for (auto& InputAbility : InputAbilities)
	{
		FGameplayAbilitySpec AbilitySpec(InputAbility.Value);
		AbilitySpec.InputID = static_cast<int32>(InputAbility.Key);
		ASC->GiveAbility(AbilitySpec);
	}

}

void ASRPlayerCharacter::SetupGASInputComponent()
{
	
	if (IsValid(ASC) && IsValid(InputComponent))
	{
		UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);

		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::GASInputPressed, static_cast<int32>(EInputAction::Dash));
		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Completed, this, &ASRPlayerCharacter::GASInputReleased, static_cast<int32>(EInputAction::Dash));
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::GASInputPressed, static_cast<int32>(EInputAction::Sprint));
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ASRPlayerCharacter::GASInputReleased, static_cast<int32>(EInputAction::Sprint));
		EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::Slide);
		EnhancedInputComponent->BindAction(GrappleAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::GASInputPressed, static_cast<int32>(EInputAction::Grapple));
		EnhancedInputComponent->BindAction(GrappleAction, ETriggerEvent::Completed, this, &ASRPlayerCharacter::GASInputReleased, static_cast<int32>(EInputAction::Grapple));
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::GASInputPressed, static_cast<int32>(EInputAction::Attack));
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &ASRPlayerCharacter::GASInputReleased, static_cast<int32>(EInputAction::Attack));
	}
}

void ASRPlayerCharacter::GASInputPressed(int32 InputId)
{
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);
	if (Spec)
	{
		Spec->InputPressed = true;
		if (Spec->IsActive())
		{
			ASC->AbilitySpecInputPressed(*Spec);
		}
		else
		{
			ASC->TryActivateAbility(Spec->Handle);
		}
	}
}

void ASRPlayerCharacter::GASInputReleased(int32 InputId)
{
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);
	if (Spec)
	{
		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			ASC->AbilitySpecInputReleased(*Spec);
		}
	}
}

void ASRPlayerCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetWorld()->GetFirstLocalPlayerFromController()))
	{
		if (InputMappingContext)
		{
			Subsystem->AddMappingContext(InputMappingContext, 1);
			UE_LOG(LogTemp, Warning, TEXT("[SRPlayerCharacter] Setup InputMappingContext Successfully linked"));
	
		}
	}
	
	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	
	if (EnhancedInputComponent)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASRPlayerCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASRPlayerCharacter::Look);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::OnInteract);
	}

	SetupGASInputComponent();
}

