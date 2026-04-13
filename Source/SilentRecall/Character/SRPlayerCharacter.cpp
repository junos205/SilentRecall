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

ASRPlayerCharacter::ASRPlayerCharacter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<USRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    SetActorTickEnabled(false);
    
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

    // 부모 클래스(ASRBaseCharacter)에 있는 InventoryComponent 사용!
    if (InventoryComponent)
    {
       InventoryComponent->OnWeaponChanged.AddDynamic(this, &ASRPlayerCharacter::HandleWeaponChanged);
    }
}

void ASRPlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

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
                SetActorTickEnabled(false);
             }
             break;
          }
       }
    }

    if (bIsVaulting && Controller)
    {
       FRotator CurrentSocketRot = GetMesh()->GetSocketRotation(TEXT("CameraSocket")); 
       FRotator SocketDelta = (CurrentSocketRot - InitialSocketRot).GetNormalized();
       FRotator TargetRot = (InitialControlRot + SocketDelta).GetNormalized();
       FRotator CurrentRot = Controller->GetControlRotation();
       FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 15.0f);
       Controller->SetControlRotation(NewRot);
    }

    if (bIsVaulting)
    {
       if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
       {
          if (!AnimInstance->IsAnyMontagePlaying()) EndVault(nullptr, true); 
       }
    }
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

void ASRPlayerCharacter::AttachWeaponToHands(AActor* WeaponActor, FName EquipSocketName)
{
    if (!WeaponActor) return;

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
    if (!NewWeaponData)
    {
       if (GetMesh() && CurrentTPLayer) GetMesh()->UnlinkAnimClassLayers(CurrentTPLayer);
       if (Mesh1P && CurrentFPLayer) Mesh1P->UnlinkAnimClassLayers(CurrentFPLayer);
       
       CurrentTPLayer = nullptr;
       CurrentFPLayer = nullptr;
       return;
    }

    if (CurrentTPLayer || CurrentFPLayer) UnlinkWeaponAnimLayers(CurrentTPLayer, CurrentFPLayer);

    if (GetMesh() && NewWeaponData->TP_AnimLayerClass)
    {
       GetMesh()->LinkAnimClassLayers(NewWeaponData->TP_AnimLayerClass);
       CurrentTPLayer = NewWeaponData->TP_AnimLayerClass; 
    }

    if (Mesh1P && NewWeaponData->FP_AnimLayerClass)
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
    if (TryVault()) return;

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

    FVector Target1Location = FVector::ZeroVector; 
    FVector Target2Location = FVector::ZeroVector; 
    UAnimMontage* SelectedMontage = nullptr;

    switch (ParkourType)
    {
    case EParkourType::LowVault:
       {
          Target1Location = LedgeLocation + (WallNormal * 30.0f); 
          Target1Location.Z = LedgeLocation.Z; 
          Target2Location = LedgeLocation + (ForwardDir * 120.0f); 
          Target2Location.Z = GetActorLocation().Z; 
          SelectedMontage = LowVaultMontage;
          break;
       }
    case EParkourType::HighMantle:
       {
          Target1Location = LedgeLocation + (WallNormal * 50.0f);
          float VaultHandHeightOffset = 200.0f; 
          Target1Location.Z = LedgeLocation.Z - VaultHandHeightOffset;
          Target2Location = LedgeLocation + (ForwardDir * 100.0f); 
          Target2Location.Z = LedgeLocation.Z; 
          DrawDebugSphere(GetWorld(), Target1Location, 10.0f, 16, FColor::Red, false, 5.0f);  
          DrawDebugSphere(GetWorld(), Target2Location, 10.0f, 16, FColor::Blue, false, 5.0f); 
          SelectedMontage = HighMantleMontage;
          break;
       }
    default: break;
    }
    
    if (!SelectedMontage) return false;

    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(FName("VaultHandTarget"), Target1Location, TargetRotation);
    MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(FName("VaultLandTarget"), Target2Location, TargetRotation);

    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    if (GetCapsuleComponent()) GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    float AnimDuration = PlayAnimMontage(SelectedMontage);
    if (AnimDuration > 0.0f)
    {
       bIsVaulting = true;
       if (GetCharacterMovement()) GetCharacterMovement()->StopMovementImmediately();
       bUseControllerRotationYaw = false;

       if (APlayerController* PC = Cast<APlayerController>(Controller))
       {
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
    bUseControllerRotationYaw = true; 

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
       FRotator ResetRot = PC->GetControlRotation();
       ResetRot.Roll = 0.0f; 
       PC->SetControlRotation(ResetRot);
       PC->ResetIgnoreLookInput();
    }

    if (GrappleState == EGrappleState::Idle) SetActorTickEnabled(false);
}

EParkourType ASRPlayerCharacter::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
    FVector StartLocation = GetActorLocation();
    FVector ForwardVector = GetActorForwardVector();
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