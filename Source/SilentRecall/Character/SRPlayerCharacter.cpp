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
	Camera->SetupAttachment(GetMesh(), TEXT("head"));
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
    FVector LedgeLocation;
    FVector WallNormal;
    EParkourType ParkourType = DetectLedge(LedgeLocation, WallNormal);

    if (ParkourType == EParkourType::None) return false;

    FVector ForwardDir = (-WallNormal).GetSafeNormal(); 
    FRotator TargetRotation = ForwardDir.Rotation();
    float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius(); 

    // 타겟을 저장할 변수들
    FVector Target1Location; // 손 짚는 곳
    FVector Target2Location; // 착지하는 곳
    UAnimMontage* SelectedMontage = nullptr;

    switch (ParkourType)
    {
    case EParkourType::LowVault:
       // ⭐️ 낮은 벽 (훌쩍 넘기)
       // 가슴을 벽에 대지 않고 위로 넘어가므로, 타겟을 벽 바깥으로 빼지 않습니다!
       // 오히려 손을 옥상 안쪽에 짚도록 모서리에서 안쪽으로 15cm 넣어줍니다.
       Target1Location = LedgeLocation + (ForwardDir * 15.0f);
       
       // 착지 지점도 훌쩍 넘어가니까 훨씬 더 멀리(100cm) 찍어줍니다.
       Target2Location = LedgeLocation + (ForwardDir * 100.0f) + (FVector::UpVector * 10.0f);
       SelectedMontage = LowVaultMontage;
       break;

    case EParkourType::HighMantle:
       // ⭐️ 높은 벽 (가슴 대고 영차 오르기)
       // 가슴이 벽돌을 뚫지 않게 캡슐 반지름 + 여유 공간(35)만큼 밖으로 뺍니다.
       Target1Location = LedgeLocation + (WallNormal * (CapsuleRadius + 35.0f));
       
       // 옥상 끝자락에 안전하게 올라서도록 70cm 안쪽으로 세팅
       Target2Location = LedgeLocation + (ForwardDir * 70.0f) + (FVector::UpVector * 10.0f);
       SelectedMontage = HighMantleMontage;
       break;

    default:
       break;
    }

    if (!SelectedMontage) return false;

    // 🎯 1단계: 모션 워핑 컴포넌트에 타겟 입력
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
       FName("VaultHandTarget"), Target1Location, TargetRotation
    );

    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
       FName("VaultLandTarget"), Target2Location, TargetRotation
    );

    // 🎯 2단계: 1인칭 전용 물리 세팅 (변함 없음)
    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    if (GetCapsuleComponent()) GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 🎯 3단계: 몽타주 실행 및 복구 델리게이트 연결 (변함 없음)
    if (SelectedMontage)
    {
       PlayAnimMontage(SelectedMontage);

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
	if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (GetCapsuleComponent()) GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
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

	if (bWallHit)
	{
		OutWallNormal = ForwardHit.Normal;
       
		// 위에서 아래로 쏘는 위치도 살짝 수정 (벽 안쪽으로 30cm만 들어가서 쏩니다)
		FVector DownStart = ForwardHit.Location + (ForwardVector * 30.0f) + (FVector::UpVector * 200.0f); 
		FVector DownEnd = DownStart - (FVector::UpVector * 200.0f);

		// ... (이하 DownHit, LedgeHeight 검사 및 분류 로직은 기존과 100% 동일)
		FHitResult DownHit;
		FCollisionShape DownShape = FCollisionShape::MakeSphere(15.0f);

		bool bHitLedge = GetWorld()->SweepSingleByChannel(DownHit, DownStart, DownEnd, FQuat::Identity, ECC_Visibility, DownShape, QueryParams);

		if (bHitLedge)
		{
			// ⭐️ 높이 검사: 발바닥부터 옥상 바닥까지의 높이 계산
			float CharacterFeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			float LedgeHeight = DownHit.Location.Z - CharacterFeetZ;

			FVector StandLocation = DownHit.Location + (FVector::UpVector * 90.0f); 
			FCollisionShape CharacterCapsule = FCollisionShape::MakeCapsule(40.0f, 90.0f); 

			// 겹침 검사
			bool bIsBlocked = GetWorld()->OverlapAnyTestByChannel(StandLocation, FQuat::Identity, ECC_Visibility, CharacterCapsule, QueryParams);

			if (!bIsBlocked)
			{
				OutLedgeLocation = FVector(ForwardHit.Location.X, ForwardHit.Location.Y, DownHit.Location.Z);
				OutWallNormal = ForwardHit.Normal;

				// ⭐️ 높이에 따른 파쿠르 타입 분류! (유저님 프로젝트 애니메이션에 맞춰 조절하세요)
				// 예: 60cm ~ 130cm 사이는 '허리용 Vault'
				if (LedgeHeight > 60.0f && LedgeHeight <= 130.0f)
				{
					return EParkourType::LowVault;
				}
				// 예: 130cm ~ 200cm 사이는 '머리/가슴용 Mantle/Climb'
				else if (LedgeHeight > 130.0f && LedgeHeight <= 200.0f)
				{
					return EParkourType::HighMantle;
				}
			}
		}
		return EParkourType::None; // 60cm 미만이거나 블로킹된 경우 파쿠르 안 함
	}
	return EParkourType::None;
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
	}

	SetupGASInputComponent();
}

