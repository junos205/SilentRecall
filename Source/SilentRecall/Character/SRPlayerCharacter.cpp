// Fill out your copyright notice in the Description page of Project Settings.

#include "SRPlayerCharacter.h"
#include "NiagaraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/SRCharacterMovementComponent.h"
#include "MotionWarpingComponent.h"

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

bool ASRPlayerCharacter::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
	// 1. [가슴팍 앞으로 쏘기] : 눈앞에 벽이 있는지 확인합니다.
    FVector StartLocation = GetActorLocation(); // 캐릭터 골반/배 위치
    FVector ForwardVector = GetActorForwardVector();
    FVector ForwardEnd = StartLocation + (ForwardVector * 150.0f); // 1.5미터 앞까지 검사

    FHitResult ForwardHit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    // (무기 액터 무시 코드도 필요하다면 여기에 추가)

    // 스피어 대신 약간 얇은 캡슐이나 구체를 써도 좋습니다.
    FCollisionShape ForwardShape = FCollisionShape::MakeSphere(30.0f); 

    bool bHitWall = GetWorld()->SweepSingleByChannel(ForwardHit, StartLocation, ForwardEnd, FQuat::Identity, ECC_Visibility, ForwardShape, QueryParams);

    // 디버그 (빨간색: 벽 확인용)
    // DrawDebugLine(GetWorld(), StartLocation, ForwardEnd, FColor::Red, false, 2.0f, 0, 2.0f);

    if (bHitWall)
    {
        // ⭐️ 벽을 찾았다! 벽의 법선(Normal)을 저장해둡니다. (나중에 캐릭터가 벽을 바라보게 회전할 때 씀)
        OutWallNormal = ForwardHit.Normal;

        // 2. [위에서 아래로 쏘기] : 유저님 아이디어의 핵심! 모서리(Ledge)의 윗면을 찾습니다.
        // 벽 부딪힌 곳에서 살짝 앞(벽 안쪽) & 내 머리 위(Z축)로 훅 올라간 위치에서 시작합니다.
        FVector DownStart = ForwardHit.Location + (ForwardVector * 15.0f) + (FVector::UpVector * 200.0f); 
        FVector DownEnd = DownStart - (FVector::UpVector * 200.0f); // 거기서 다시 아래로 2미터 쏩니다.

        FHitResult DownHit;
        FCollisionShape DownShape = FCollisionShape::MakeSphere(15.0f); // 모서리를 찾을 땐 얇은 구체를 씁니다.

        bool bHitLedge = GetWorld()->SweepSingleByChannel(DownHit, DownStart, DownEnd, FQuat::Identity, ECC_Visibility, DownShape, QueryParams);

        // 디버그 (파란색: 옥상 바닥 확인용)
        // DrawDebugLine(GetWorld(), DownStart, DownEnd, FColor::Blue, false, 2.0f, 0, 2.0f);

        if (bHitLedge)
        {
            // ⭐️ 모서리 윗면도 찾았다! (DownHit.Location)
            
            // 3. [최종 검사 - 걸림돌 확인] : 유저님이 말씀하신 "4번 쏴서 걸림돌 없나 확인"하는 부분입니다.
            // 4번 쏠 필요 없이, 내가 올라갈 자리에 '캐릭터만 한 투명 캡슐'을 놔보고 안 겹치는지 딱 1번만 물어보면 됩니다!
            
            FVector StandLocation = DownHit.Location + (FVector::UpVector * 90.0f); // 바닥 + 내 캐릭터 반 높이
            FCollisionShape CharacterCapsule = FCollisionShape::MakeCapsule(40.0f, 90.0f); // 내 캐릭터 사이즈

            // Overlap 검사: 이 자리에 캡슐을 놨을 때 천장이나 다른 장애물에 겹치나요?
            bool bIsBlocked = GetWorld()->OverlapAnyTestByChannel(StandLocation, FQuat::Identity, ECC_Visibility, CharacterCapsule, QueryParams);

            if (!bIsBlocked)
            {
                // 걸림돌도 없고 완벽하게 텅 비어있다! Ledge 감지 최종 성공!
                // ⭐️ 매달릴 손의 위치 = 벽의 Z축 높이 + 내가 부딪힌 벽의 XY 좌표
                OutLedgeLocation = FVector(ForwardHit.Location.X, ForwardHit.Location.Y, DownHit.Location.Z);
                
                // 디버그 (초록색 공: 최종 매달릴 위치!)
                // DrawDebugSphere(GetWorld(), OutLedgeLocation, 10.0f, 12, FColor::Green, false, 2.0f);
                
                return true; 
            }
        }
    }

    // 조건 중 하나라도 실패하면 레지 아님!
    return false;
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

