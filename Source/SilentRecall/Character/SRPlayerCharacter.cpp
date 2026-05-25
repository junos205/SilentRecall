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
   if (InventoryComponent == nullptr)
   {
      InventoryComponent = FindComponentByClass<USRInventoryComponent>();
   }
    
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

    USRCharacterMovementComponent* SRMovement = Cast<USRCharacterMovementComponent>(GetCharacterMovement());
    APlayerController* PC = Cast<APlayerController>(GetController());

    if (SRMovement && SRMovement->IsFalling())
    {
       LastFallingVelocity = GetVelocity().Z;
    }
    
    float CurrentSpeed = GetVelocity().Size2D();

    // 카메라 쉐이크 로직
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

    // 커스텀 이동 (슬라이딩 & 벽 타기 카메라 롤)
    if (SRMovement)
    {
        if (PC)
        {
           PC->PlayerCameraManager->ViewRollMin = -179.9f;
           PC->PlayerCameraManager->ViewRollMax = 179.9f;

           FRotator CurrentControlRot = PC->GetControlRotation();
           float TargetRoll = SRMovement->TargetWallRunRoll;

           FRotator TargetRot = CurrentControlRot;
           TargetRot.Roll = TargetRoll;

           float InterpSpeed = 12.0f;
           FRotator NewRot = FMath::RInterpTo(CurrentControlRot, TargetRot, DeltaTime, InterpSpeed);

           if (FMath::IsNearlyZero(TargetRoll) && FMath::Abs(NewRot.Roll) < 0.1f)
           {
              NewRot.Roll = 0.0f;
           }

           if (!CurrentControlRot.Equals(NewRot, 0.01f))
           {
              PC->SetControlRotation(NewRot);
           }
        }
    }

    // 그래플링 처리 로직
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

    // 볼팅 (파쿠르) 로직
    bool bIsCurrentlyVaulting = false;
    if (ASC)
    {
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
}

void ASRPlayerCharacter::AttachWeaponToHolster(AActor* WeaponActor, FName HolsterSocketName)
{
    if (!WeaponActor) return;
    
    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor() == WeaponActor)
    {
       if (Cloned1PMesh) 
       { 
          Cloned1PMesh->DestroyComponent(); 
          Cloned1PMesh = nullptr; 
       }
    }
    
    WeaponActor->SetOwner(this);
    WeaponActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocketName);
    WeaponActor->SetActorHiddenInGame(true);

    TArray<UMeshComponent*> Meshes;
    WeaponActor->GetComponents<UMeshComponent>(Meshes);
    for (auto* M : Meshes) { M->bCastHiddenShadow = false; }
}

void ASRPlayerCharacter::PlayWeaponMontage(class UAnimMontage* MontageToPlay, bool bFirstPersonOnly)
{
    if (!MontageToPlay) return;

    if (Mesh1P && Mesh1P->GetAnimInstance())
    {
       Mesh1P->GetAnimInstance()->Montage_Play(MontageToPlay);
    }

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
    if (Controller != nullptr)
    {
       AddControllerPitchInput(-PitchAmount); 
       AddControllerYawInput(YawAmount);
    }
}

USkeletalMeshComponent* ASRPlayerCharacter::Get1PMesh() const
{
    return Mesh1P;
}

FVector ASRPlayerCharacter::GetActiveWeaponMuzzleLocation() const
{
    if (IsLocallyControlled() && Cloned1PMesh)
    {
       return Cloned1PMesh->GetSocketLocation(FName("Muzzle"));
    }
    
    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
    {
       if (USkeletalMeshComponent* Mesh3P = InventoryComponent->GetCurrentActiveWeaponActor()->FindComponentByClass<USkeletalMeshComponent>())
       {
          return Mesh3P->GetSocketLocation(FName("Muzzle"));
       }
    }

    return GetActorLocation();
}

void ASRPlayerCharacter::AttachWeaponToHands(AActor* WeaponActor, FName EquipSocketName)
{
    UE_LOG(LogTemp, Warning, TEXT("[Character] AttachWeaponToHands Called!"));

    if (!WeaponActor) 
    {
       UE_LOG(LogTemp, Error, TEXT("[Character] AttachWeaponToHands Failed: WeaponActor is NULL!"));
       return;
    }
    
    WeaponActor->SetOwner(this);
    WeaponActor->SetActorHiddenInGame(false); 
    WeaponActor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, EquipSocketName);
    
    TArray<UMeshComponent*> Meshes;
    WeaponActor->GetComponents<UMeshComponent>(Meshes);
    for (auto* M : Meshes) 
    { 
        M->SetOwnerNoSee(true);      
        M->bCastHiddenShadow = true; 
        M->SetVisibility(true);      
    }

    if (Cloned1PMesh) 
    {
       Cloned1PMesh->DestroyComponent(); 
       Cloned1PMesh = nullptr;
    }

    USkeletalMeshComponent* OriginalMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>();
    if (OriginalMesh)
    {
       Cloned1PMesh = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
       Cloned1PMesh->RegisterComponent(); 
       Cloned1PMesh->AttachToComponent(Mesh1P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, EquipSocketName);
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

USkeletalMeshComponent* ASRPlayerCharacter::GetWeaponMeshForComponent(USkeletalMeshComponent* PlayerMesh)
{
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
    Super::HandleWeaponChanged(NewWeaponData);

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

void ASRPlayerCharacter::Input_CycleWeapon(const FInputActionValue& Value)
{
    float ScrollValue = Value.Get<float>();
    if (ScrollValue == 0.0f || !InventoryComponent) return;

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    static float LastScrollTime = 0.0f;
    const float ScrollThresholdDelay = 0.15f; 

    // ==========================================================
    // 🚨 [마스터 키] 언리얼 에디터 PIE 재시작 고질병(시간 왜곡) 방어선
    // ==========================================================
    // 에디터를 완전히 껐다 켜지 않아 전 판의 static 시간대(예: 150초)가 메모리에 남아있고,
    // 새 게임이 시작되어 CurrentTime이 리셋(예: 3초)되었을 때 대소관계가 뒤집히는 것을 감지합니다.
    if (CurrentTime < LastScrollTime)
    {
        // 판이 바뀌었음을 확신하고 과거의 유령 시간대를 깨끗하게 세탁(Reset)합니다.
        LastScrollTime = 0.0f; 
    }

    // 하드웨어 축 입력 생사 확인을 위해 디버그용 출력 로그 가동
    UE_LOG(LogTemp, Warning, TEXT("[Character_Input] 휠 물리 입력 감지 (스크롤값: %f)"), ScrollValue);

    // ==========================================================
    // 🛡️ 1단계: GAS 상태 태그 검사 (피격/액션 중 최우선 잠금)
    // ==========================================================
    if (ASC)
    {
        FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.WeaponSwitch"));
        FGameplayTag MeleeTag  = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Melee"));
        FGameplayTag HitTag    = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact"));
        FGameplayTag StunTag   = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun"));

        if (ASC->HasMatchingGameplayTag(SwitchTag) || ASC->HasMatchingGameplayTag(MeleeTag) || 
            ASC->HasMatchingGameplayTag(HitTag)    || ASC->HasMatchingGameplayTag(StunTag))
        {
            UE_LOG(LogTemp, Log, TEXT("[Character_Input] 🔒 캐릭터가 스왑/공격/피격 액션 중이므로 휠 입력을 잠급니다."));
            return; 
        }
    }

    // ==========================================================
    // ⏱️ 2단계: 하드웨어 OS 휠 버스트 틱 디바운싱 필터링
    // ==========================================================
    if (CurrentTime - LastScrollTime < ScrollThresholdDelay)
    {
        UE_LOG(LogTemp, Log, TEXT("[Character_Input] ⏳ 한 칸 스크롤 시 발생하는 Windows OS의 중복 유령 틱이 정상 차단되었습니다."));
        return; 
    }

    // 모든 지뢰밭 검문소를 완벽히 통과한 단 1번의 순수한 격발 타임스탬프만 기록합니다.
    LastScrollTime = CurrentTime;

    UE_LOG(LogTemp, Error, TEXT("[Character_Input] 🟢 [최종 승인] 인벤토리 컴포넌트에 진짜 무기 교체 신호 전송 완료!"));
    InventoryComponent->CycleWeapon(ScrollValue > 0.0f);
}

void ASRPlayerCharacter::Jump()
{
    UE_LOG(LogTemp, Error, TEXT("[Character] Jump() called! Attempting to vault..."));
    
    FGameplayTag VaultTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.Vault"));
    if (ASC && ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(VaultTag)))
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

    if (ASC)
    {
       FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump"));
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
       PC->PlayerCameraManager->StartCameraShake(SlideShakeClass, 1.0f);
    }
}

EParkourType ASRPlayerCharacter::DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal)
{
    FVector StartLocation = GetActorLocation();
    FVector ForwardVector = GetActorForwardVector(); 
    FVector TraceDirection = ForwardVector;

    if (UCharacterMovementComponent* MoveComp = Cast<UCharacterMovementComponent>(GetCharacterMovement()))
    {
       if (MoveComp->CurrentFloor.bBlockingHit)
       {
          FVector FloorNormal = MoveComp->CurrentFloor.HitResult.Normal;
          TraceDirection = FVector::VectorPlaneProject(ForwardVector, FloorNormal).GetSafeNormal();
       }
    }

    float TraceDistance = 300.0f; 
    FVector EndLocation = StartLocation + (TraceDirection * TraceDistance); 

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(30.0f);

    FHitResult ForwardHit;
    bool bWallHit = GetWorld()->SweepSingleByChannel(ForwardHit, StartLocation, EndLocation, FQuat::Identity, ECC_Visibility, SphereShape, QueryParams);

    if (bWallHit)
    {
       float WallSteepness = FMath::Abs(FVector::DotProduct(ForwardHit.Normal, FVector::UpVector));
       if (WallSteepness > 0.3f)
       {
           return EParkourType::None;
       }

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
    // ==========================================================
    // 🛡️ ⭐️ [해결 1] 무기 장착/스왑 중 중복 루팅 원천 차단 마스터 브레이크
    // ==========================================================
    if (ASC)
    {
        FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.WeaponSwitch"));
        if (ASC->HasMatchingGameplayTag(SwitchTag))
        {
            UE_LOG(LogTemp, Warning, TEXT("[Character_Interact] ❌ 무기 교체 애니메이션이 진행 중입니다. 상호작용을 강제 거부합니다."));
            return; 
        }
    }

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
        }
    }
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
       float ImpactSpeed = FMath::Abs(LastFallingVelocity);

       float FinalShakeScale = FMath::GetMappedRangeValueClamped(
          FVector2D(400.0f, 1500.0f),
          FVector2D(0.2f, 3.0f), 
          ImpactSpeed
       );

       UCameraShakeBase* SpawnedShake = PC->PlayerCameraManager->StartCameraShake(LandShakeClass, FinalShakeScale);
        
       if (ULegacyCameraShake* LegacyShake = Cast<ULegacyCameraShake>(SpawnedShake))
       {
          float DynamicBlendOut = FMath::GetMappedRangeValueClamped(
             FVector2D(400.0f, 1500.0f),
             FVector2D(0.2f, 1.2f),
             ImpactSpeed
          );

          LegacyShake->OscillationBlendOutTime = DynamicBlendOut;
          LegacyShake->OscillationDuration = 0.1f + DynamicBlendOut; 

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
       EnhancedInputComponent->BindAction(CycleWeaponAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::Input_CycleWeapon);
    }

    SetupGASInputComponent();
}

void ASRPlayerCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

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
       FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump"));
       FGameplayTag WallRunTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Movement.WallRunning"));

       ASC->SetLooseGameplayTagCount(JumpTag, 0);
       ASC->AddLooseGameplayTag(WallRunTag);
    }

    if (InventoryComponent)
    {
       InventoryComponent->SetCurrentActiveWeaponVisibility(false);
    }
    
    if (Cloned1PMesh)
    {
       Cloned1PMesh->SetVisibility(false);
    }
}

void ASRPlayerCharacter::OnWallRunEnded()
{
    if (ASC)
    {
       FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Jump"));
       FGameplayTag WallRunTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Movement.WallRunning"));

       ASC->SetLooseGameplayTagCount(WallRunTag, 0);

       if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
       {
          ASC->AddLooseGameplayTag(JumpTag);
       }
    }

    if (InventoryComponent)
    {
       InventoryComponent->SetCurrentActiveWeaponVisibility(true);
    }
    
    if (Cloned1PMesh)
    {
       Cloned1PMesh->SetVisibility(true);
    }
}