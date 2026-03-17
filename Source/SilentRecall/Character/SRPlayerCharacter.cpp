// Fill out your copyright notice in the Description page of Project Settings.

#include "SRPlayerCharacter.h"
#include "NiagaraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/SRCharacterMovementComponent.h"

ASRPlayerCharacter::ASRPlayerCharacter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<USRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	bUseControllerRotationYaw = true;   // 캐릭터가 마우스 좌우 회전을 따라감
	bUseControllerRotationPitch = false; // 1인칭이라도 캐릭터 몸체가 위아래로 기울어지면 안 됨
	bUseControllerRotationRoll = false;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetMesh(), TEXT("head"));
	Camera->bUsePawnControlRotation = true;
}

void ASRPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float CurrentSpeed = GetVelocity().Size();

	// 3. 목표 FOV 결정 (속도가 기준치를 넘으면 115, 아니면 90)
	float TargetFOV = (CurrentSpeed >= SpeedVFXThreshold) ? SprintFOV : BaseFOV;

	// 4. FInterpTo를 사용해 카메라 FOV를 부드럽게 줌 인/아웃 (10.0f는 전환 속도)
	float NewFOV = FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, 10.0f);
	Camera->SetFieldOfView(NewFOV);

	// 5. 스피드 라인 파티클 켜고 끄기
	if (SpeedLinesVFX)
	{
		if (CurrentSpeed >= SpeedVFXThreshold)
	{
		// 꺼져있을 때만 켭니다 (매 프레임 Activate 호출 방지)
		if (!SpeedLinesVFX->IsActive())
		{
			SpeedLinesVFX->Activate(true);
		}
	}
	else
	{
		// 켜져있을 때만 끕니다
		if (SpeedLinesVFX->IsActive())
		{
			SpeedLinesVFX->Deactivate();
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

