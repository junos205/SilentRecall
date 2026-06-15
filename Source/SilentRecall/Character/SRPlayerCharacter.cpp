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
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"

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

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Mesh1P);
    CameraBoom->TargetArmLength = 0.0f;

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
         MainHUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
         if (MainHUDWidget)
         {
            MainHUDWidget->AddToViewport();
         }
      }

      if (InventoryComponent)
      {
         InventoryComponent->RefreshWeaponHUD();
      }
   }
   else
   {
      if (GetMesh()) GetMesh()->SetOwnerNoSee(false); 
      if (Mesh1P) Mesh1P->SetVisibility(false); 
   }

   if (IsLocallyControlled() && BGMPlaylist.Num() > 0)
   {
      float StartTime = 0.0f;
      int32 StartIndex = 0;

      if (USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance()))
      {
         StartTime = GI->SavedBGMPlaybackTime;
         StartIndex = GI->SavedBGMTrackIndex;
      }

      // 플레이리스트 인덱스 범위 초과 방어 가드
      if (!BGMPlaylist.IsValidIndex(StartIndex)) 
      {
         StartIndex = 0;
      }

      CurrentPlaylistIndex = StartIndex;
      PlayBGMFromPlaylist(CurrentPlaylistIndex, StartTime);
   }
}

void ASRPlayerCharacter::TickGrappleTargetDetection()
{
    UCameraComponent* CameraComp = FindComponentByClass<UCameraComponent>();
    if (!CameraComp) return;

    float TargetRange = 3000.0f; 
    FVector StartLocation = CameraComp->GetComponentLocation();
    FVector ViewDir = CameraComp->GetForwardVector();
    FVector EndLocation = StartLocation + (ViewDir * TargetRange);

    TArray<FHitResult> HitResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(400.0f);

    bool bHit = GetWorld()->SweepMultiByChannel(
        HitResults, StartLocation, EndLocation, FQuat::Identity, ECC_GameTraceChannel2, SphereShape, QueryParams
    );

    ASRGrapplePoint* BestTarget = nullptr;
    float BestDotProduct = 0.5f;   // 최소 조준 각도 마크 (에임 가이드라인)
    float BestDistance = 999999.f; // 각도가 동률일 때 비교할 최단 거리 저장소

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            ASRGrapplePoint* HitPoint = Cast<ASRGrapplePoint>(Hit.GetActor());
            if (HitPoint)
            {
                // 1. 초기 겹침 상태에서도 안전한 액터 고유의 정중앙 월드 좌표 확보
                FVector TargetCenterLoc = HitPoint->GetActorLocation();
                FVector DirToTarget = (TargetCenterLoc - StartLocation).GetSafeNormal();
                
                // 2. 에임 중심점(크로스헤어) 정렬도 및 실시간 거리 연산
                float DotProduct = FVector::DotProduct(ViewDir, DirToTarget);
                float DistanceToTarget = FVector::Distance(StartLocation, TargetCenterLoc);

                // 3. [시선 중심 정렬 최우선 필터링]
                // 먼저 겹치거나 가까운 게 주도권을 뺏지 못하도록 화면 중앙 정렬도를 최우선으로 검사합니다.
                bool bIsBetterTarget = false;
                
                if (DotProduct > BestDotProduct + 0.005f)
                {
                    // [우선순위 1] 화면 정중앙(크로스헤어)에 더 가깝게 조준하고 있다면 무조건 가동
                    bIsBetterTarget = true;
                }
                else if (FMath::IsNearlyEqual(DotProduct, BestDotProduct, 0.005f))
                {
                    // [우선순위 2] 만약 시선 각도가 거의 완벽하게 똑같다면, 그 중 더 가까운 타겟을 선택
                    if (DistanceToTarget < BestDistance)
                    {
                        bIsBetterTarget = true;
                    }
                }

                // 조건 검증을 통과한 유력 후보만 최종 시야 검사(장애물 체크)에 진입시킵니다.
                if (bIsBetterTarget)
                {
                    FHitResult VisibilityHit;
                    FCollisionQueryParams VisQueryParams;
                    VisQueryParams.AddIgnoredActor(this);
                    
                    // 🛡️ [자가 충돌 방어] 타겟 본인의 콜리전 박스 때문에 시야가 막혔다고 오판하는 것을 원천 차단
                    VisQueryParams.AddIgnoredActor(HitPoint); 
                    
                    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
                    {
                        VisQueryParams.AddIgnoredActor(InventoryComponent->GetCurrentActiveWeaponActor());
                    }

                    // 타겟의 정확한 센터 좌표로 깨끗하게 시야 검사 레이저 격발
                    bool bObstructed = GetWorld()->LineTraceSingleByChannel(
                        VisibilityHit, StartLocation, TargetCenterLoc, ECC_Visibility, VisQueryParams
                    );

                    // =======================================================================
                    // 🛡️ [배경 벽면 씹힘 완치 오차 마진 공식]
                    // 무언가에 레이저가 부딪혔더라도, 충돌 지점이 타겟의 중심점과 거의 일치한다면
                    // (타겟보다 최소 15cm 이상 앞에서 가로막은 게 아니라면) 
                    // 그것은 가로막은 장애물이 아니라 "배경 벽"이므로 시야가 확보된 것으로 인정합니다!
                    // =======================================================================
                    if (bObstructed)
                    {
                        if (VisibilityHit.Distance < DistanceToTarget - 15.0f)
                        {
                            continue; // 타겟보다 확실히 앞에서 가로막고 있는 진짜 장애물이므로 기각 패스
                        }
                    }
                    // =======================================================================

                    // 모든 가드를 통과한 최종 왕좌 갱신
                    BestDotProduct = DotProduct;
                    BestDistance = DistanceToTarget;
                    BestTarget = HitPoint;       
                }
            }
        }
    }

    // HUD 위젯 상태 인계 정산 (기존 연출 연동 흐름 보존)
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
   
    if (Camera)
    {
       float TargetFOV = BaseFOV;
       float CustomFOVInterpSpeed = FOVInterpSpeed;

       if (bIsAiming)
       {
          float WeaponAimFOV = 65.0f; 
          if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponInstance())
          {
             if (auto* WD = InventoryComponent->GetCurrentActiveWeaponInstance()->WeaponData)
             {
                WeaponAimFOV = WD->AimFOV; 
             }
          }
          TargetFOV = WeaponAimFOV;
          CustomFOVInterpSpeed = FOVInterpSpeed; 
       }
       else
       {
          TargetFOV = FMath::GetMappedRangeValueClamped(
             FVector2D(MinSpeedForFOV, MaxSpeedForFOV),
             FVector2D(BaseFOV, SprintFOV),
             CurrentSpeed
          );

          if (GrappleState != EGrappleState::Idle)
          {
             TargetFOV += 7.0f; 
          }

          CustomFOVInterpSpeed = (TargetFOV > Camera->FieldOfView) ? (FOVInterpSpeed * 2.0f) : FOVInterpSpeed;
       }

       float NewFOV = FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, CustomFOVInterpSpeed);
       Camera->SetFieldOfView(NewFOV);
       Camera->FirstPersonFieldOfView = NewFOV;
    }

   FGameplayTag AimTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Aiming"));
   bIsAiming = (ASC && ASC->HasMatchingGameplayTag(AimTag));

   if (bIsAiming)
   {
      CurrentADSOffset = FMath::VInterpTo(CurrentADSOffset, CalculateADSOffset(), DeltaTime, 15.0f);
      USAimAlpha = FMath::FInterpTo(USAimAlpha, 1.0f, DeltaTime, 15.0f);
   }
   else
   {
      CurrentADSOffset = FMath::VInterpTo(CurrentADSOffset, FVector::ZeroVector, DeltaTime, 15.0f);
      USAimAlpha = FMath::FInterpTo(USAimAlpha, 0.0f, DeltaTime, 15.0f);
   }

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
   if (IsLocallyControlled() && BGMAudioComponent && BGMPlaylist.IsValidIndex(CurrentPlaylistIndex) && BGMPlaylist[CurrentPlaylistIndex])
   {
      if (BGMAudioComponent->IsPlaying())
      {
         // 실시간 프레임 시간 적립 (Pitch 배율을 곱해야 슬로우 모션 시 느려지는 속도까지 완벽 동기화됨)
         CurrentBGMTimelineSeconds += DeltaTime * BGMAudioComponent->PitchMultiplier;

         // 🔥 [무한 누적 차단선] 현재 틀고 있는 곡의 순수 원본 러닝타임 길이를 측정합니다.
         float MaxSongDuration = BGMPlaylist[CurrentPlaylistIndex]->GetDuration();

         // 곡이 완전히 끝났거나 끝나기 직전이라면?
         if (CurrentBGMTimelineSeconds >= MaxSongDuration)
         {
            // 다음 트랙으로 번호 인계 (플레이리스트가 2개라면: 0 ➔ 1 ➔ 0 ➔ 1 순환 구조)
            CurrentPlaylistIndex = (CurrentPlaylistIndex + 1) % BGMPlaylist.Num();
                
            // 🌟 새 노래가 시작되므로 타이머가 0.0f인 상태로 깨끗하게 다음 트랙을 연타 실행!
            PlayBGMFromPlaylist(CurrentPlaylistIndex, 0.0f);
                
            UE_LOG(LogTemp, Warning, TEXT("[BGM 플레이리스트] 곡 종료 완료. 다음 순번인 %d번 트랙 연달아 재생 가동!"), CurrentPlaylistIndex);
         }
      }

      // --- 기존 슬로우 모션 및 사망 시 먹먹해지는 피치 믹싱 제어 메커니즘 유지 ---
      FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"));
      bool bIsDead = ASC && ASC->HasMatchingGameplayTag(DeadTag);
      bool bIsSlowMo = UGameplayStatics::GetGlobalTimeDilation(GetWorld()) < 0.99f;

      float TargetVolume = 1.0f;
      float TargetPitch = 1.0f;

      if (bIsDead || bIsSlowMo)
      {
         TargetVolume = 0.35f; 
         TargetPitch = 0.65f;  
      }

      float NewVolume = FMath::FInterpTo(BGMAudioComponent->VolumeMultiplier, TargetVolume, DeltaTime, 6.0f);
      float NewPitch = FMath::FInterpTo(BGMAudioComponent->PitchMultiplier, TargetPitch, DeltaTime, 6.0f);

      BGMAudioComponent->SetVolumeMultiplier(NewVolume);
      BGMAudioComponent->SetPitchMultiplier(NewPitch);
   }
}

void ASRPlayerCharacter::PlayBGMFromPlaylist(int32 TrackIndex, float StartTime)
{
   if (!BGMPlaylist.IsValidIndex(TrackIndex) || !BGMPlaylist[TrackIndex]) return;

   // 이미 다른 노래가 재생 중이라면 완전히 숨통을 끊고 교체합니다.
   if (BGMAudioComponent)
   {
      BGMAudioComponent->Stop();
   }

   // 현재 타이머를 시작 지점으로 초기화 (새 곡이면 0.0f, 이어 틀기면 세이브된 시간)
   CurrentBGMTimelineSeconds = StartTime;
    
   // 월드에 새로운 오디오 컴포넌트를 스폰
   BGMAudioComponent = UGameplayStatics::SpawnSound2D(GetWorld(), BGMPlaylist[TrackIndex], 1.0f, 1.0f, StartTime);
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
    if (!WeaponActor) return;
    
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

    USRHUDWidget* HUDWidget = Cast<USRHUDWidget>(MainHUDWidget);
    if (HUDWidget)
    {
       HUDWidget->SetBypassAnimation(true);
    }

    if (ASC)
    {
       ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetHealthAttribute(), GI->SavedHealth);
       ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetAPAttribute(), GI->SavedAP);
       ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetManaAttribute(), GI->SavedMana);
       ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetXPAttribute(), GI->SavedXP);
       ASC->SetNumericAttributeBase(USRDefaultAttributeSet::GetLevelAttribute(), GI->SavedLevel);
    }

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
}

void ASRPlayerCharacter::ApplyWeaponAnimLayer()
{
   if (InventoryComponent)
   {
      if (USRWeaponInstance* ActiveInst = InventoryComponent->GetCurrentActiveWeaponInstance())
      {
         if (ActiveInst->WeaponData && ActiveInst->WeaponData->FP_AnimLayerClass && Mesh1P)
         {
            Mesh1P->LinkAnimClassLayers(ActiveInst->WeaponData->FP_AnimLayerClass);
            CurrentFPLayer = ActiveInst->WeaponData->FP_AnimLayerClass; 
         }
      }
   }
}

FVector ASRPlayerCharacter::CalculateADSOffset() const
{
   FVector CustomTuning = FVector::ZeroVector;
   if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponInstance())
   {
      if (auto* WD = InventoryComponent->GetCurrentActiveWeaponInstance()->WeaponData)
      {
         CustomTuning = WD->AimOffsetTuning; 
      }
   }
   return CustomTuning;
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

// =======================================================================
// 🔄 [복구 및 정밀 검증] R키 부활 집행 및 재장전 연쇄 제어기
// =======================================================================
void ASRPlayerCharacter::HandleReloadOrRespawn()
{
    // 캐릭터가 사망 태그를 가지고 있다면 무기 재장전 대신 게임모드의 부활 시스템 집행 가동!
    if (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"))))
    {
       if (ASRGameMode* GM = Cast<ASRGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
       {
          GM->ExecuteRespawnReset();
       }
       return;
    }

    // 살아있는 상태라면 정상적으로 GAS 입력(Reload ID) 발사
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

   // =======================================================================
   // 🔊 [신규 추가] 일반 공중 점프 중 추가 점프 시 더블 점프 사운드 연출
   // =======================================================================
   if (JumpCurrentCount > 0 && JumpCurrentCount < JumpMaxCount)
   {
      PlayDoubleJumpSound(); // 내부적으로 랜덤 사운드가 터집니다.
   }

   JumpMaxCount = 2;
   Super::Jump();
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

   if (GrappleStartSound)
   {
      UGameplayStatics::SpawnSoundAttached(
          GrappleStartSound,
          GetMesh(),
          FName("hand_r_Socket"),
          FVector::ZeroVector,
          FRotator::ZeroRotator, 
          EAttachLocation::KeepRelativeOffset,
          true
      );
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

// =======================================================================
// 📡 [복구 및 정밀 검증] 벽타기(Wall Run) 진입/해제 시점 무기 시각 제어부
// =======================================================================
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
       EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ASRPlayerCharacter::GASInputPressed, static_cast<int32>(EInputAction::Aim));
       EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ASRPlayerCharacter::GASInputReleased, static_cast<int32>(EInputAction::Aim));
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

void ASRPlayerCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);

    if (ASC)
    {
       FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName("Ability.Cooldown.Dash"));
       ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(CooldownTag));
       
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
       float FinalShakeScale = FMath::GetMappedRangeValueClamped(FVector2D(400.0f, 1500.0f), FVector2D(0.2f, 3.0f), ImpactSpeed);

       UCameraShakeBase* SpawnedShake = PC->PlayerCameraManager->StartCameraShake(LandShakeClass, FinalShakeScale);
       if (ULegacyCameraShake* LegacyShake = Cast<ULegacyCameraShake>(SpawnedShake))
       {
          float DynamicBlendOut = FMath::GetMappedRangeValueClamped(FVector2D(400.0f, 1500.0f), FVector2D(0.2f, 1.2f), ImpactSpeed);
          LegacyShake->OscillationBlendOutTime = DynamicBlendOut;
          LegacyShake->OscillationDuration = 0.1f + DynamicBlendOut; 
       }
    }
}

void ASRPlayerCharacter::SetupGASInputComponent()
{
    if (IsValid(ASC) && IsValid(InputComponent))
    {
       UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
       if (EnhancedInputComponent)
       {
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
}

void ASRPlayerCharacter::GASInputPressed(int32 InputId)
{
    if (!ASC) return;
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
    if (!ASC) return;
    FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromInputID(InputId);
    if (Spec)
    {
       Spec->InputPressed = false;
       if (Spec->IsActive()) ASC->AbilitySpecInputReleased(*Spec);
    }
}

void ASRPlayerCharacter::PlayRandomSoundFromPool(const TArray<class USoundBase*>& SoundPool)
{
   if (SoundPool.Num() > 0)
   {
      int32 Index = FMath::RandRange(0, SoundPool.Num() - 1);
      if (SoundPool[Index])
      {
         UGameplayStatics::PlaySoundAtLocation(GetWorld(), SoundPool[Index], GetActorLocation());
      }
   }
}
