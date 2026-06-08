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
#include "Components/WidgetComponent.h"
#include "Game/SRGameMode.h"
#include "Gimmick/SRGrapplePoint.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UI/SRHUDWidget.h"
#include "UI/SRHUDControllerComponent.h"
#include "Game/SRGameInstance.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "Blueprint/UserWidget.h" // 🌟 [추가] CreateWidget 및 AddToViewport 사용을 위한 헤더

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

    // 🌟 [참조 보호] 다른 BP 연결을 지키기 위해 생성만 해두고 사용하지 않는 껍데기 암
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Mesh1P);
    CameraBoom->TargetArmLength = 0.0f;

    // 🌟 [최종 부착] 카메라 암 없이, Mesh1P의 "head" 본에 직접 용접
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Mesh1P, FName("head")); 
    
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
        
		if (HUDWidgetClass)
		{
			// 🌟 [수정] 앞에 'UUserWidget*'를 지워서 로컬 변수 중복 선언을 없애고 멤버 변수를 바로 사용합니다.
			MainHUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
			if (MainHUDWidget)
			{
				MainHUDWidget->AddToViewport();
				UE_LOG(LogTemp, Log, TEXT("[Character] HUD 위젯 스폰 완료. UI 바인딩은 위젯이 알아서 처리합니다."));
			}
		}
	}
	else
	{
		if (GetMesh()) GetMesh()->SetOwnerNoSee(false); 
		if (Mesh1P) Mesh1P->SetVisibility(false); 
	}
}

void ASRPlayerCharacter::TickGrappleTargetDetection()
{
    UCameraComponent* CameraComp = FindComponentByClass<UCameraComponent>();
    if (!CameraComp) return;

    float TargetRange = 2500.0f; 
    
    FVector StartLocation = CameraComp->GetComponentLocation();
    FVector ViewDir = CameraComp->GetForwardVector();
    FVector EndLocation = StartLocation + (ViewDir * TargetRange);

    TArray<FHitResult> HitResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(250.0f);

    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults, 
        StartLocation, 
        EndLocation, 
        FQuat::Identity,
        ECC_GameTraceChannel2, 
        SphereShape, 
        QueryParams
    );

    ASRGrapplePoint* BestTarget = nullptr;
    float BestDotProduct = -1.0f;

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            ASRGrapplePoint* HitPoint = Cast<ASRGrapplePoint>(Hit.GetActor());
            if (HitPoint)
            {
                FVector DirToTarget = (Hit.ImpactPoint - StartLocation).GetSafeNormal();
                float DotProduct = FVector::DotProduct(ViewDir, DirToTarget);

                if (DotProduct > 0.5f && DotProduct > BestDotProduct)
                {
                    FHitResult VisibilityHit;
                    FCollisionQueryParams VisQueryParams;
                    VisQueryParams.AddIgnoredActor(this); 
                    
                    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
                    {
                        VisQueryParams.AddIgnoredActor(InventoryComponent->GetCurrentActiveWeaponActor());
                    }

                    bool bObstructed = GetWorld()->LineTraceSingleByChannel(
                        VisibilityHit,
                        StartLocation,
                        Hit.ImpactPoint, 
                        ECC_Visibility,  
                        VisQueryParams
                    );

                    if (bObstructed && VisibilityHit.GetActor() != HitPoint)
                    {
                        continue; 
                    }

                    BestDotProduct = DotProduct; 
                    BestTarget = HitPoint;       
                }
            }
        }
    }

    if (CurrentTargetPoint.Get() != BestTarget)
    {
        if (CurrentTargetPoint.IsValid())
        {
            CurrentTargetPoint->SetWidgetActive(false);
        }

        CurrentTargetPoint = BestTarget;

        if (CurrentTargetPoint.IsValid())
        {
            CurrentTargetPoint->SetWidgetActive(true);
        }
    }
}

void ASRPlayerCharacter::AdjustHUDResolution()
{
    // 💡 [참조 유지] 헤더 파일 선언부 변경 번거로움을 방지하기 위해 빈 껍데기 스텁으로 남겨둡니다.
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

    // =======================================================================
    // 🚀 [기획 변수 연동] 하이퍼 스피드 동적 FOV 왜곡 연산
    // =======================================================================
    if (Camera)
    {
        // 1. 헤더에 정의된 변수들을 사용하여 현재 속도 비례 목표 FOV 매핑
        float TargetFOV = FMath::GetMappedRangeValueClamped(
            FVector2D(MinSpeedForFOV, MaxSpeedForFOV),
            FVector2D(BaseFOV, SprintFOV),
            CurrentSpeed
        );

        // 2. 그래플링 줄을 타고 날아가는 스윙 상태일 때는 공간 왜곡 쾌감 보너스 (+7도) 추가
        if (GrappleState != EGrappleState::Idle)
        {
            TargetFOV += 7.0f; 
        }

        // 3. [비대칭 보간 설계] 
        // 속도가 빨라지며 화면이 찢어질 때는 유저님이 디테일 창에 지정한 'FOVInterpSpeed'의 2배속으로 팍! 확장하고,
        // 감속하며 제자리로 돌아올 때는 지정하신 정속(1배속)으로 부드럽게 수축하여 멀미를 차단합니다.
        float CurrentFOV = Camera->FieldOfView;
        float DynamicInterpSpeed = (TargetFOV > CurrentFOV) ? (FOVInterpSpeed * 2.0f) : FOVInterpSpeed;

        float NewFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, DynamicInterpSpeed);
        
        // 4. 카메라 컴포넌트와 언리얼 내부 시스템 변수에 동시 주입
        Camera->SetFieldOfView(NewFOV);
        Camera->FirstPersonFieldOfView = NewFOV;
    }
    // =======================================================================

    // 카메라 쉐이크 로직 (걷기/스프린트 흔들림)
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

   if (GrappleState == EGrappleState::Idle)
   {
      TickGrappleTargetDetection();
   }
   else
   {
      if (CurrentTargetPoint.IsValid())
      {
         CurrentTargetPoint->SetWidgetActive(false);
         CurrentTargetPoint = nullptr;
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

       if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponInstance())
       {
          Cloned1PMesh->SetRelativeScale3D(InventoryComponent->GetCurrentActiveWeaponInstance()->WeaponData->WeaponScale);
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

void ASRPlayerCharacter::SaveCharacterState(USRGameInstance* GI)
{
    if (!GI) return;

    if (ASC)
    {
       const USRDefaultAttributeSet* AttrSet = Cast<USRDefaultAttributeSet>(ASC->GetAttributeSet(USRDefaultAttributeSet::StaticClass()));
       if (AttrSet)
       {
          GI->SavedHealth = AttrSet->GetHealth();
          GI->SavedAP = AttrSet->GetAP();
          GI->SavedMana = AttrSet->GetMana();
          GI->SavedXP = AttrSet->GetXP();
          GI->SavedLevel = AttrSet->GetLevel();
       }
    }

    if (InventoryComponent)
    {
       InventoryComponent->SaveToGameInstance(GI);
    }
}

void ASRPlayerCharacter::LoadCharacterState(USRGameInstance* GI)
{
	if (!GI) return;

	// 🌟 [1단계] 데이터를 강제 오버라이트 하기 전 UI를 무음 모드로 전환합니다.
	USRHUDWidget* HUDWidget = Cast<USRHUDWidget>(MainHUDWidget);
	if (HUDWidget)
	{
		HUDWidget->SetBypassAnimation(true);
	}

	if (ASC)
	{
		// 이 함수들이 한 줄씩 실행될 때마다 배후에서 변경 감지 델리게이트가 마구 요동칩니다.
		ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetHealthAttribute(), GI->SavedHealth);
		ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetAPAttribute(), GI->SavedAP);
		ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetManaAttribute(), GI->SavedMana);
		ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetXPAttribute(), GI->SavedXP);
		ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetLevelAttribute(), GI->SavedLevel);
	}

	// 🌟 [2단계] 어트리뷰트 리빌드가 완전히 종료되었으므로 연출 잠금을 해제합니다.
	if (HUDWidget)
	{
		HUDWidget->SetBypassAnimation(false);
	}

	if (InventoryComponent)
	{
		InventoryComponent->LoadFromGameInstance(GI);
	}
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

    if (CurrentTime < LastScrollTime)
    {
        LastScrollTime = 0.0f; 
    }

    UE_LOG(LogTemp, Warning, TEXT("[Character_Input] 휠 물리 입력 감지 (스크롤값: %f)"), ScrollValue);

    if (ASC)
    {
        FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.WeaponSwitch"));
        FGameplayTag MeleeTag  = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Melee"));
        FGameplayTag HitTag    = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact"));
        FGameplayTag StunTag   = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun"));

        if (ASC->HasMatchingGameplayTag(SwitchTag) || ASC->HasMatchingGameplayTag(MeleeTag) || 
            ASC->HasMatchingGameplayTag(HitTag)    || ASC->HasMatchingGameplayTag(StunTag))
        {
            return; 
        }
    }

    if (CurrentTime - LastScrollTime < ScrollThresholdDelay)
    {
        return; 
    }

    LastScrollTime = CurrentTime;
    InventoryComponent->CycleWeapon(ScrollValue > 0.0f);
}

void ASRPlayerCharacter::HandleReloadOrRespawn()
{
    if (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"))))
    {
       if (ASRGameMode* GM = Cast<ASRGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
       {
          UE_LOG(LogTemp, Warning, TEXT("[Character_Input] 죽은 상태에서 R키 입력 감지 -> 즉시 수동 부활을 집행합니다."));
          GM->ExecuteRespawnReset();
       }
       return;
    }

    GASInputPressed(static_cast<int32>(EInputAction::Reload));
}

void ASRPlayerCharacter::Jump()
{
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
    if (ASC)
    {
        FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.WeaponSwitch"));
        if (ASC->HasMatchingGameplayTag(SwitchTag))
        {
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
       FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName("Ability.Cooldown.Dash"));
       ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(CooldownTag));
    }

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
       EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::HandleReloadOrRespawn);
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

       FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName("Ability.Cooldown.Dash"));
       ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(CooldownTag));
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