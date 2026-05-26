// Fill out your copyright notice in the Description page of Project Settings.

#include "SRPlayerCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "NiagaraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "KismetTraceUtils.h"
#include "Camera/CameraComponent.h"
#include "Character/SRCharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "Components/CapsuleComponent.h"
#include "SRInventoryComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Interface/InteractableInterface.h"
#include "Data/SRWeaponDataAsset.h"
#include "Camera/CameraShakeBase.h"
#include "LegacyCameraShake.h"

ASRPlayerCharacter::ASRPlayerCharacter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<USRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    GetMesh()->SetOwnerNoSee(true); 
    GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f)); 
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    GetMesh()->bCastHiddenShadow = true;
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    
    Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
    Mesh1P->SetupAttachment(GetMesh()); 
    Mesh1P->SetOnlyOwnerSee(true);
    Mesh1P->SetRelativeLocation(FVector::ZeroVector); 
    Mesh1P->SetRelativeRotation(FRotator::ZeroRotator);
    Mesh1P->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    Mesh1P->CastShadow = false;

	GetMesh()->PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Mesh1P->PrimaryComponentTick.TickGroup = TG_PrePhysics;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Mesh1P, TEXT("head")); 
    Camera->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
    Camera->SetRelativeLocation(FVector::ZeroVector);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    Camera->bUsePawnControlRotation = true;
    Camera->bEnableFirstPersonFieldOfView = true;
    Camera->FirstPersonFieldOfView = 90.0f;

    GrappleCable = CreateDefaultSubobject<UCableComponent>(TEXT("GrappleCable"));
    GrappleCable->SetupAttachment(GetRootComponent());
    GrappleCable->SetVisibility(false);
    GrappleCable->CableLength = 0.0f;
    GrappleCable->NumSegments = 10;
    GrappleCable->SolverIterations = 4;
    GrappleCable->CableWidth = 5.0f;
    GrappleCable->EndLocation = FVector::ZeroVector;

    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> CharacterMeshRef(TEXT("/Script/Engine.SkeletalMesh'/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple'"));
    if (CharacterMeshRef.Object)
    {
       Mesh1P->SetSkeletalMesh(CharacterMeshRef.Object);
    }
}

void ASRPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    if (IsLocallyControlled())
    {
       if (GetMesh()) GetMesh()->SetOwnerNoSee(true); 
       if (Mesh1P) Mesh1P->SetVisibility(true);
    }
    else
    {
       if (GetMesh()) GetMesh()->SetOwnerNoSee(false); 
       if (Mesh1P) Mesh1P->SetVisibility(false); 
    }
}

void ASRPlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ⭐️ 1. Tick 맨 위에서 한 번만 캐스팅하여 캐싱
    USRCharacterMovementComponent* SRMovement = Cast<USRCharacterMovementComponent>(GetCharacterMovement());
    APlayerController* PC = Cast<APlayerController>(GetController());

    if (SRMovement && SRMovement->IsFalling())
    {
       LastFallingVelocity = GetVelocity().Z;
    }
    
    float CurrentSpeed = GetVelocity().Size2D();

    // =====================================
    // 🎥 카메라 쉐이크 로직
    // =====================================
    if (PC && PC->PlayerCameraManager && SRMovement)
    {
        bool bIsWalkingOnGround = SRMovement->IsMovingOnGround();
        bool bIsNotGrappling = (GrappleState == EGrappleState::Idle);
        float AbsoluteMaxSprintSpeed = 1000.f; 

        if (CurrentSpeed > 10.f && bIsWalkingOnGround && bIsNotGrappling)
        {
           if (!ActiveMovementShake && MovementShakeClass)
           {
              ActiveMovementShake = Cast<ULegacyCameraShake>(PC->PlayerCameraManager->StartCameraShake(MovementShakeClass, 0.0f));
           }

           float TargetScale = FMath::GetMappedRangeValueClamped(FVector2D(100.0f, AbsoluteMaxSprintSpeed), FVector2D(0.2f, 1.2f), CurrentSpeed);
           float TargetPlayRate = FMath::GetMappedRangeValueClamped(FVector2D(100.0f, AbsoluteMaxSprintSpeed), FVector2D(0.8f, 1.3f), CurrentSpeed);

           CurrentShakeScale = FMath::FInterpTo(CurrentShakeScale, TargetScale, DeltaTime, 5.0f);

           if (ActiveMovementShake)
           {
              ActiveMovementShake->ShakeScale = CurrentShakeScale;
              if (ULegacyCameraShake* DefaultShake = MovementShakeClass->GetDefaultObject<ULegacyCameraShake>())
              {
                 ActiveMovementShake->LocOscillation.Z.Frequency = DefaultShake->LocOscillation.Z.Frequency * TargetPlayRate * 0.1f;
                 ActiveMovementShake->RotOscillation.Pitch.Frequency = DefaultShake->RotOscillation.Pitch.Frequency * TargetPlayRate;
                 ActiveMovementShake->RotOscillation.Roll.Frequency = DefaultShake->RotOscillation.Roll.Frequency * TargetPlayRate;
              }
           }
        }
        else
        {
           if (ActiveMovementShake)
           {
              bool bStopImmediately = !bIsWalkingOnGround || !bIsNotGrappling;
              PC->PlayerCameraManager->StopCameraShake(ActiveMovementShake, bStopImmediately);
              ActiveMovementShake = nullptr; 
              CurrentShakeScale = 0.0f; 
           }
        }
    }

    // =====================================
    // 🏃‍♂️ 커스텀 이동 (슬라이딩 & 벽 타기 카메라 롤)
    // =====================================
    if (SRMovement)
    {
       if (SRMovement->CustomMovementMode == ECustomMovementMode::CMOVE_Sliding)
       {
          // 슬라이딩 처리 로직
       }

    	if (PC)
    	{
    		// 언리얼 엔진의 깐깐한 카메라 Roll 잠금을 완전히 해제합니다!
    		PC->PlayerCameraManager->ViewRollMin = -179.9f;
    		PC->PlayerCameraManager->ViewRollMax = 179.9f;

    		FRotator CurrentControlRot = PC->GetControlRotation();
    		float TargetRoll = SRMovement->TargetWallRunRoll;

    		// ⭐️ [핵심 버그 해결] 현재 회전값에 타겟 롤만 쏙 집어넣은 '목표 회전값(TargetRot)'을 만듭니다.
    		FRotator TargetRot = CurrentControlRot;
    		TargetRot.Roll = TargetRoll;

    		float InterpSpeed = 12.0f;
           
    		// ⭐️ FMath::FInterpTo (단순 숫자 계산) 대신,
    		// ⭐️ FMath::RInterpTo (회전 전용 계산)를 사용하여 360도 회전 버그를 완벽히 차단합니다!
    		FRotator NewRot = FMath::RInterpTo(CurrentControlRot, TargetRot, DeltaTime, InterpSpeed);

    		// 0도 근처로 오면 미세한 떨림 방지를 위해 완전히 0으로 딱 잡아줍니다.
    		if (FMath::IsNearlyZero(TargetRoll) && FMath::Abs(NewRot.Roll) < 0.1f)
    		{
    			NewRot.Roll = 0.0f;
    		}

    		// 각도가 미세하게라도 변했을 때만 덮어씌웁니다.
    		if (!CurrentControlRot.Equals(NewRot, 0.01f))
    		{
    			PC->SetControlRotation(NewRot);
    		}
    	}
    }

    // =====================================
    // 🪝 그래플링 처리 로직
    // =====================================
    if (GrappleState != EGrappleState::Idle && GrappleCable)
    {
       FVector HandLocation = GetMesh()->GetSocketLocation(FName("hand_r_Socket"));
       GrappleCable->SetWorldLocation(HandLocation);

       switch (GrappleState)
       {
       case EGrappleState::Deploying:
          {
             CurrentCableEndLocation = FMath::VInterpConstantTo(CurrentCableEndLocation, GrappleTargetLocation, DeltaTime, DeploySpeed);
             GrappleCable->EndLocation = GrappleCable->GetComponentTransform().InverseTransformPosition(CurrentCableEndLocation);
             GrappleCable->CableLength = FVector::Distance(HandLocation, CurrentCableEndLocation);

             if (FVector::DistSquared(CurrentCableEndLocation, GrappleTargetLocation) < 10.0f)
             {
                if (SRMovement) SRMovement->EnterGraple(GrappleTargetLocation);
                GrappleState = EGrappleState::Swinging;
             }
             break;
          }
       case EGrappleState::Swinging:
          {
             GrappleCable->EndLocation = GrappleCable->GetComponentTransform().InverseTransformPosition(GrappleTargetLocation);
             GrappleCable->CableLength = FVector::Distance(HandLocation, GrappleTargetLocation);
             break;
          }
       case EGrappleState::Retracting:
          {
             CurrentCableEndLocation = FMath::VInterpConstantTo(CurrentCableEndLocation, HandLocation, DeltaTime, RetractSpeed);
             GrappleCable->EndLocation = GrappleCable->GetComponentTransform().InverseTransformPosition(CurrentCableEndLocation);
             GrappleCable->CableLength = FVector::Distance(HandLocation, CurrentCableEndLocation);

             if (FVector::DistSquared(CurrentCableEndLocation, HandLocation) < 100.0f)
             {
                GrappleState = EGrappleState::Idle;
                GrappleCable->SetVisibility(false);
                GrappleCable->EndLocation = FVector::ZeroVector; 
             }
             break;
          }
       }
    }

    // =====================================
    // 🧱 볼팅 (파쿠르) 로직
    // =====================================
	bool bIsCurrentlyVaulting = false;
	if (ASC)
	{
		// 내 몸에 '볼팅 중'이라는 태그가 있는지 확인
		bIsCurrentlyVaulting = ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
	}

	if (bIsCurrentlyVaulting && Controller)
	{
		FRotator CurrentSocketRot = GetMesh()->GetSocketRotation(TEXT("CameraSocket")); 
		FRotator SocketDelta = (CurrentSocketRot - InitialSocketRot).GetNormalized();
		FRotator TargetRot = (InitialControlRot + SocketDelta).GetNormalized();
		FRotator CurrentRot = Controller->GetControlRotation();
		FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 15.0f);
		Controller->SetControlRotation(NewRot);
	}
	// ❌ 기존에 틱 하단에 있던 EndVault 검사 로직(IsAnyMontagePlaying)은 지웁니다. GA가 알아서 캔슬시킵니다.
}
// =====================================================================
// 1. 손에 무기 장착 (1P 메쉬 동적 생성 & 3P 가시성 완벽 세팅)
// =====================================================================
void ASRPlayerCharacter::AttachWeaponToHolster(AActor* WeaponActor, FName HolsterSocketName)
{
	if (!WeaponActor) return;
    
	// ⭐️ [핵심 해결책] 지금 내 손에 들고 있는 무기(CurrentActiveWeapon)를 집어넣을 때만 1P 메쉬를 부숩니다!
	// 바닥에서 주운 새 무기를 등에 매달 때는 1P 메쉬를 건드리지 않고 무사히 넘어갑니다.
	if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor() == WeaponActor)
	{
		if (Cloned1PMesh) 
		{ 
			Cloned1PMesh->DestroyComponent(); 
			Cloned1PMesh = nullptr; 
		}
	}
    
	// 새 무기를 등에 붙이고 투명하게 숨김
	WeaponActor->SetOwner(this);
	WeaponActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocketName);
	WeaponActor->SetActorHiddenInGame(true);

	// 등에 있는 동안은 그림자도 안 보이게 처리
	TArray<UMeshComponent*> Meshes;
	WeaponActor->GetComponents<UMeshComponent>(Meshes);
	for (auto* M : Meshes) { M->bCastHiddenShadow = false; }
}

void ASRPlayerCharacter::PlayWeaponMontage(class UAnimMontage* MontageToPlay, bool bFirstPersonOnly)
{
	if (!MontageToPlay) return;

	// 1. 1인칭은 무조건 재생 (어떤 상황이든 내 눈엔 보여야 하니까요)
	if (Mesh1P && Mesh1P->GetAnimInstance())
	{
		Mesh1P->GetAnimInstance()->Montage_Play(MontageToPlay);
	}

	// 2. 3인칭은 옵션에 따라 결정
	if (!bFirstPersonOnly)
	{
		if (GetMesh() && GetMesh()->GetAnimInstance())
		{
			GetMesh()->GetAnimInstance()->Montage_Play(MontageToPlay);
		}
	}
}

void ASRPlayerCharacter::ApplyRecoil(float PitchAmount, float YawAmount)
{
	// 캐릭터에 빙의된 컨트롤러(플레이어 마우스)가 있는지 확인합니다.
	if (Controller != nullptr)
	{
		// ⭐️ [매우 중요] 언리얼 엔진의 기본 마우스 로직은 Pitch가 음수(-)일 때 카메라가 위로 올라갑니다!
		// 데이터 애셋에는 양수(예: 0.5 ~ 1.2)로 편하게 적으시고, 여기서 적용할 때만 빼기(-)를 붙여줍니다.
		AddControllerPitchInput(-PitchAmount); 

		// Yaw는 양수면 우측, 음수면 좌측으로 돌아가므로 그대로 꽂아줍니다.
		AddControllerYawInput(YawAmount);
	}
}

USkeletalMeshComponent* ASRPlayerCharacter::Get1PMesh() const
{
	return Mesh1P;
}

FVector ASRPlayerCharacter::GetActiveWeaponMuzzleLocation() const
{
	// 1. 내가 현재 조종 중인 로컬 플레이어라면? -> 1P 복제 메쉬(Cloned1PMesh)에서 총구를 찾는다!
	if (IsLocallyControlled() && Cloned1PMesh)
	{
		return Cloned1PMesh->GetSocketLocation(FName("Muzzle"));
	}
    
	// 2. 다른 플레이어(멀티)이거나 적(AI)이라면? -> 인벤토리에 있는 3P 무기 액터에서 총구를 찾는다!
	if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
	{
		if (USkeletalMeshComponent* Mesh3P = InventoryComponent->GetCurrentActiveWeaponActor()->FindComponentByClass<USkeletalMeshComponent>())
		{
			return Mesh3P->GetSocketLocation(FName("Muzzle"));
		}
	}

	// (보험) 메쉬를 못 찾았다면 액터의 중심점 반환
	return GetActorLocation();
}

void ASRPlayerCharacter::AttachWeaponToHands(AActor* WeaponActor, FName EquipSocketName)
{
	// ⭐️ 진입 확인 로그 추가!
	UE_LOG(LogTemp, Warning, TEXT("[Character] AttachWeaponToHands Called!"));

	if (!WeaponActor) 
	{
		UE_LOG(LogTemp, Error, TEXT("[Character] AttachWeaponToHands Failed: WeaponActor is NULL!"));
		return;
	}
	
    // 1. 3P 원본 무기
    WeaponActor->SetOwner(this);
    WeaponActor->SetActorHiddenInGame(false); 
    WeaponActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, EquipSocketName);
    
    TArray<UMeshComponent*> Meshes;
    WeaponActor->GetComponents<UMeshComponent>(Meshes);
    for (auto* M : Meshes) 
    { 
        M->SetOwnerNoSee(true);      
        M->bCastHiddenShadow = true; // 꺼냈으니 그림자 다시 켜기
        M->SetVisibility(true);      
    }

	// 2. 1P 무기 (런타임 생성 및 부착)
	if (Cloned1PMesh) 
	{
		Cloned1PMesh->DestroyComponent(); 
		Cloned1PMesh = nullptr;
	}

	USkeletalMeshComponent* OriginalMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>();
	if (OriginalMesh)
	{
		Cloned1PMesh = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
        
		// ⭐️ [버그 해결] 런타임에는 무조건 RegisterComponent와 AttachToComponent를 써야 합니다!
		Cloned1PMesh->RegisterComponent(); 
		Cloned1PMesh->AttachToComponent(Mesh1P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, EquipSocketName);
        
		// ⭐️ 손바닥에 붙이고 나서 위치를 강제로 (0,0,0)으로 딱 맞춤!
		Cloned1PMesh->SetRelativeTransform(FTransform::Identity); 

		Cloned1PMesh->SetSkeletalMeshAsset(OriginalMesh->GetSkeletalMeshAsset());
		Cloned1PMesh->SetAnimInstanceClass(OriginalMesh->GetAnimClass());
		for (int32 i = 0; i < OriginalMesh->GetNumMaterials(); ++i)
		{
			Cloned1PMesh->SetMaterial(i, OriginalMesh->GetMaterial(i));
		}

		Cloned1PMesh->SetOwnerNoSee(false);         
		Cloned1PMesh->SetOnlyOwnerSee(true);        
		Cloned1PMesh->CastShadow = false;           
		Cloned1PMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
		Cloned1PMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson); 
	}
}

// =====================================================================
// 3. 근접 타격 판정 노티파이에서 호출하는 함수 (더 간결해짐!)
// =====================================================================
USkeletalMeshComponent* ASRPlayerCharacter::GetWeaponMeshForComponent(USkeletalMeshComponent* PlayerMesh)
{
    // 이제 1P 무기는 액터가 아니라 컴포넌트 그 자체이므로 바로 반환하면 됩니다!
    if (PlayerMesh == Mesh1P && Cloned1PMesh)
    {
        return Cloned1PMesh; 
    }
    
    if (PlayerMesh == GetMesh() && InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
    {
        return InventoryComponent->GetCurrentActiveWeaponActor()->FindComponentByClass<USkeletalMeshComponent>();
    }
    return nullptr;
}

// =========================================================

void ASRPlayerCharacter::LinkWeaponAnimLayers(TSubclassOf<UAnimInstance> TP_Layer, TSubclassOf<UAnimInstance> FP_Layer)
{
    if (GetMesh() && TP_Layer) GetMesh()->LinkAnimClassLayers(TP_Layer);
    if (Mesh1P && FP_Layer) Mesh1P->LinkAnimClassLayers(FP_Layer);
}

void ASRPlayerCharacter::UnlinkWeaponAnimLayers(TSubclassOf<UAnimInstance> TP_Layer, TSubclassOf<UAnimInstance> FP_Layer)
{
    if (GetMesh() && TP_Layer) GetMesh()->UnlinkAnimClassLayers(TP_Layer);
    if (Mesh1P && FP_Layer) Mesh1P->UnlinkAnimClassLayers(FP_Layer);
}

void ASRPlayerCharacter::HandleWeaponChanged(USRWeaponDataAsset* NewWeaponData)
{
	// ⭐️ 부모(ASRBaseCharacter)의 함수를 호출하여 3P 메쉬 처리를 완벽하게 끝냅니다.
	Super::HandleWeaponChanged(NewWeaponData);

	// ==========================================
	// 아래부터는 1P (Mesh1P) 전용 처리 로직입니다.
	// ==========================================
	if (CurrentFPLayer && Mesh1P) 
	{
		Mesh1P->UnlinkAnimClassLayers(CurrentFPLayer);
		CurrentFPLayer = nullptr;
	}

	if (NewWeaponData && NewWeaponData->FP_AnimLayerClass && Mesh1P)
	{
		Mesh1P->LinkAnimClassLayers(NewWeaponData->FP_AnimLayerClass);
		CurrentFPLayer = NewWeaponData->FP_AnimLayerClass; 
	}
}

// =========================================================
// 입력 처리부
// =========================================================

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

// ⭐️ [추가됨] 마우스 휠 바인딩 실행 함수
void ASRPlayerCharacter::Input_CycleWeapon(const FInputActionValue& Value)
{
    float ScrollValue = Value.Get<float>();
    if (ScrollValue != 0.0f && InventoryComponent)
    {
        InventoryComponent->CycleWeapon(ScrollValue > 0.0f);
    }
}

void ASRPlayerCharacter::Jump()
{
	UE_LOG(LogTemp, Error, TEXT("[Character] Jump() called! Attempting to vault..."));
	
	// ⭐️ 1. 스페이스바를 누르면 가장 먼저 "Vault GA" 발동을 시도합니다.
	// (Vault GA의 AbilityTags에 이 태그를 등록해야 합니다)
	FGameplayTag VaultTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.Vault"));
    
	if (ASC && ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(VaultTag)))
	{
		// 파쿠르 난간이 있어 GA 실행에 성공했다면, 일반 점프는 무시하고 종료!
		return; 
	}

	// 2. 파쿠르가 안 나갔다면, 기존처럼 벽 점프나 일반 점프를 실행합니다.
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

	if (ASC)
	{
		FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump"));
        
		// ⭐️ [버그 해결] 이미 점프 태그가 있다면(더블점프 시) 스택을 추가하지 않도록 방어합니다!
		if (!ASC->HasMatchingGameplayTag(JumpTag))
		{
			ASC->AddLooseGameplayTag(JumpTag);
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

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->PlayerCameraManager && SlideShakeClass)
	{
		// ⭐️ 슬라이딩 시작 시점에 단발성 쉐이크 1회 재생 (예: 카메라가 살짝 바닥으로 깔리며 흔들림)
		PC->PlayerCameraManager->StartCameraShake(SlideShakeClass, 1.0f);
	}
}

EParkourType ASRPlayerCharacter::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
    FVector StartLocation = GetActorLocation();
    
    // ⭐️ 1. 캐릭터의 단순 수평 앞방향 (변수명 ForwardVector로 통일)
    FVector ForwardVector = GetActorForwardVector(); 
    FVector TraceDirection = ForwardVector;

    // ⭐️ 2. [근본 해결책] 내가 딛고 있는 바닥의 노멀(기울기)을 가져옵니다.
    if (UCharacterMovementComponent* MoveComp = Cast<UCharacterMovementComponent>(GetCharacterMovement()))
    {
       if (MoveComp->CurrentFloor.bBlockingHit)
       {
          FVector FloorNormal = MoveComp->CurrentFloor.HitResult.Normal;
            
          // ⭐️ 3. 수평 앞방향 벡터를 '바닥의 기울기(면)'에 맞춰 투영(구부림)시킵니다!
          // 이렇게 하면 경사로를 오를 때는 레이저도 대각선 위를 향해 쏘게 됩니다.
          TraceDirection = FVector::VectorPlaneProject(ForwardVector, FloorNormal).GetSafeNormal();
       }
    }

    float TraceDistance = 300.0f; 
    // 이제 레이저는 무조건 바닥과 평행하게 나아갑니다! 바닥에 꽂힐 일이 없습니다.
    FVector EndLocation = StartLocation + (TraceDirection * TraceDistance); 

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(30.0f);

    FHitResult ForwardHit;
    bool bWallHit = GetWorld()->SweepSingleByChannel(ForwardHit, StartLocation, EndLocation, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);
    
    // 디버그 라인 (원하신다면 켜두셔도 좋습니다)
    // FColor WallResultColor = bWallHit ? FColor::Red : FColor::Green;
    // DrawDebugCapsule(GetWorld(), StartLocation, ((EndLocation - StartLocation) * 0.5f).Length(), SphereShape.GetCapsuleRadius(), FQuat::Identity, WallResultColor);

    if (bWallHit)
    {
       // ⭐️ [유저님 아이디어 적용] 맞은 표면의 노멀과 월드 Up 벡터를 내적합니다.
       // 절댓값이 0에 가까울수록 완벽한 수직 벽, 1에 가까울수록 평면 바닥입니다.
       float WallSteepness = FMath::Abs(FVector::DotProduct(ForwardHit.Normal, FVector::UpVector));
       
       // 내적 값이 0.3 초과라면 (약 72도보다 완만한 경사로라면) 파쿠르 취소!
       if (WallSteepness > 0.3f)
       {
           return EParkourType::None;
       }

       OutWallNormal = ForwardHit.Normal;
       // 💡 여기서 ForwardVector가 정상적으로 사용되어 난간 안쪽으로 구체를 쏩니다!
       FVector DownStart = ForwardHit.Location + (ForwardVector * 30.0f) + (FVector::UpVector * 250.0f); 
       FVector DownEnd = DownStart - (FVector::UpVector * 250.0f);

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

             if (LedgeHeight > 60.0f && LedgeHeight <= 130.0f) return EParkourType::LowVault;
             else if (LedgeHeight > 130.0f && LedgeHeight <= 250.0f) return EParkourType::HighMantle;
          }
       }
    }
    
    return EParkourType::None;
}

void ASRPlayerCharacter::OnInteract(const FInputActionValue& Value)
{
    UCameraComponent* CameraComp = FindComponentByClass<UCameraComponent>();
    if (!CameraComp) return;

    FVector StartLoc = CameraComp->GetComponentLocation();
    FVector LookDir = CameraComp->GetForwardVector();
    FVector EndLoc = StartLoc + (LookDir * InteractDistance);

    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(InteractTraceRadius); 
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); 

    bool bHit = GetWorld()->SweepMultiByChannel(HitResults, StartLoc, EndLoc, FQuat::Identity, ECC_Visibility, SphereShape, Params);
    
    FVector CapsuleCenter = StartLoc + (LookDir * InteractDistance * 0.5f);
    float HalfHeight = InteractDistance * 0.5f;
    FQuat CapsuleRot = FRotationMatrix::MakeFromZ(LookDir).ToQuat();
    FColor CapsuleColor = FColor::Red;
    
    if (bHit)
    {
        AActor* BestTarget = nullptr;
        float MinAngle = 180.0f; 

        for (const FHitResult& Hit : HitResults)
        {
            AActor* PotentialActor = Hit.GetActor();
            if (PotentialActor && PotentialActor->Implements<UInteractableInterface>())
            {
                FVector ToActorDir = (PotentialActor->GetActorLocation() - StartLoc).GetSafeNormal();
                float Dot = FVector::DotProduct(LookDir, ToActorDir);
                float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));

                CapsuleColor = FColor::Green;

                if (Angle < MinAngle)
                {
                    MinAngle = Angle;
                    BestTarget = PotentialActor;
                }
            }
        }
        
        if (BestTarget)
        {
            IInteractableInterface::Execute_Interact(BestTarget, this);
            DrawDebugSolidBox(GetWorld(), BestTarget->GetActorLocation(), FVector(10.0f), FColor::Blue, false, 2.0f);
        }
    }
    DrawDebugCapsule(GetWorld(), CapsuleCenter, HalfHeight, InteractTraceRadius, CapsuleRot, CapsuleColor, false, 2.0f, 0, 1.0f);
}

void ASRPlayerCharacter::StartGrapple(FVector TargetLocation)
{
    GrappleTargetLocation = TargetLocation;
    if (!GrappleCable) GrappleCable = FindComponentByClass<UCableComponent>();

    if (GrappleCable)
    {
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
       if (USRCharacterMovementComponent* SRMovement = Cast<USRCharacterMovementComponent>(GetCharacterMovement()))
       {
          SRMovement->ExitGraple();
       }
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

void ASRPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (ASC)
	{
		FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump"));
        
		// ⭐️ [버그 해결] Remove(-1) 대신 SetCount(0)을 써서, 
		// 꼬여있는 스택이 몇 개든 상관없이 착지하는 순간 무조건 0으로 싹 밀어버립니다!
		ASC->SetLooseGameplayTagCount(JumpTag, 0);
	}
	
	if (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Dash")))) return;
    
	if (USRCharacterMovementComponent* MoveComp = Cast<USRCharacterMovementComponent>(GetCharacterMovement()))
	{
		if (MoveComp->CustomMovementMode == ECustomMovementMode::CMOVE_Sliding) return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->PlayerCameraManager && LandShakeClass)
	{
		// 떨어지던 수직 속도(충격량) 절대값 변환
		float ImpactSpeed = FMath::Abs(LastFallingVelocity);

		// 1. 쉐이크 세기 (Scale) 가변화
		float FinalShakeScale = FMath::GetMappedRangeValueClamped(
			FVector2D(400.0f, 1500.0f),
			FVector2D(0.2f, 3.0f), 
			ImpactSpeed
		);

		// 2. 일단 계산된 세기로 쉐이크를 재생시킵니다.
		UCameraShakeBase* SpawnedShake = PC->PlayerCameraManager->StartCameraShake(LandShakeClass, FinalShakeScale);
        
		// ⭐️ 3. 방금 재생된 쉐이크를 Legacy로 캐스팅해서 내부 시간 값을 덮어씌웁니다!
		// 3. 방금 재생된 쉐이크를 Legacy로 캐스팅해서 내부 시간 값을 덮어씌웁니다!
		if (ULegacyCameraShake* LegacyShake = Cast<ULegacyCameraShake>(SpawnedShake))
		{
			float DynamicBlendOut = FMath::GetMappedRangeValueClamped(
				FVector2D(400.0f, 1500.0f),
				FVector2D(0.2f, 1.2f),
				ImpactSpeed
			);

			LegacyShake->OscillationBlendOutTime = DynamicBlendOut;
			LegacyShake->OscillationDuration = 0.1f + DynamicBlendOut; 

			// ⭐️ 해결: UE_LOG를 if문 안쪽으로 가져왔습니다! (삼항 연산자도 뺐습니다)
			UE_LOG(LogTemp, Warning, TEXT("[Landed] Speed: %f | Scale: %f | BlendOutTime: %f"), ImpactSpeed, FinalShakeScale, LegacyShake->OscillationBlendOutTime);
		}
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
    	EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::GASInputPressed, static_cast<int32>(EInputAction::Reload));
    	EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Completed, this, &ASRPlayerCharacter::GASInputReleased, static_cast<int32>(EInputAction::Reload));
    }
}

void ASRPlayerCharacter::GASInputPressed(int32 InputId)
{
    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);
    if (Spec)
    {
       Spec->InputPressed = true;
       if (Spec->IsActive()) ASC->AbilitySpecInputPressed(*Spec);
       else ASC->TryActivateAbility(Spec->Handle);
    }
}

void ASRPlayerCharacter::GASInputReleased(int32 InputId)
{
    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);
    if (Spec)
    {
       Spec->InputPressed = false;
       if (Spec->IsActive()) ASC->AbilitySpecInputReleased(*Spec);
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
       
       // ⭐️ [추가됨] 마우스 휠 바인딩
       EnhancedInputComponent->BindAction(CycleWeaponAction, ETriggerEvent::Triggered, this, &ASRPlayerCharacter::Input_CycleWeapon);
    }

    SetupGASInputComponent();
}

void ASRPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// ⭐️ 무브먼트 컴포넌트의 방송국(Delegate)에 내 함수들을 연결합니다.
	if (USRCharacterMovementComponent* CustomMC = Cast<USRCharacterMovementComponent>(GetCharacterMovement()))
	{
		CustomMC->OnWallRunStartedDelegate.AddDynamic(this, &ASRPlayerCharacter::OnWallRunStarted);
		CustomMC->OnWallRunEndedDelegate.AddDynamic(this, &ASRPlayerCharacter::OnWallRunEnded);
	}
}

void ASRPlayerCharacter::OnWallRunStarted()
{
	if (ASC)
	{
		// 벽을 타기 시작하면 공중(Jump) 판정을 지우고 벽타기 태그를 붙여 기력을 회복시킵니다.
		FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump"));
		FGameplayTag WallRunTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Movement.WallRunning"));

		ASC->SetLooseGameplayTagCount(JumpTag, 0);
		ASC->AddLooseGameplayTag(WallRunTag);
	}
}

void ASRPlayerCharacter::OnWallRunEnded()
{
	if (ASC)
	{
		// 벽에서 떨어지면 벽타기 태그를 지웁니다.
		FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump"));
		FGameplayTag WallRunTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Movement.WallRunning"));

		ASC->SetLooseGameplayTagCount(WallRunTag, 0);

		// 바닥에 닿지 않고 떨어지는 중(Falling)이라면 다시 Jump 태그를 붙여 기력 회복을 막습니다.
		if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
		{
			ASC->AddLooseGameplayTag(JumpTag);
		}
	}
}