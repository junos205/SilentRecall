// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SRBaseCharacter.h"
#include "CableComponent.h"
#include "Interface/SRCharacterInterface.h"
#include "InputActionValue.h"
#include "Data/SRCharacterData.h"
#include "SRPlayerCharacter.generated.h"

class ULegacyCameraShake;
class ASRGrapplePoint;

UENUM(BlueprintType)
enum class EGrappleState : uint8
{
    Idle,       
    Deploying,  
    Swinging,   
    Retracting  
};

UENUM(BlueprintType)
enum class EParkourType : uint8
{
    None,
    LowVault,   
    HighMantle  
};

UCLASS()
class SILENTRECALL_API ASRPlayerCharacter : public ASRBaseCharacter
{
    GENERATED_BODY()

public:
    ASRPlayerCharacter(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void PostInitializeComponents() override;

    UFUNCTION()
    void OnWallRunStarted();

    UFUNCTION()
    void OnWallRunEnded();
    
    FORCEINLINE void AddInputAbility(EInputAction InputAction, TSubclassOf<class UGameplayAbility> AbilityToGrant)
    {
       if (ASC && !InputAbilities.Contains(InputAction))
       {
          InputAbilities.Add(InputAction, AbilityToGrant);
          SetupGASInputComponent(); 
       }
    }

    FORCEINLINE UUserWidget* GetMainHUDWidget() const { return MainHUDWidget; }
    
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    FVector GetActiveWeaponMuzzleLocation() const;

    ASRGrapplePoint* GetCurrentGrappleTarget() const { return CurrentTargetPoint.Get(); }

    // 🌟 [격상] 애님인스턴스가 안전하게 수치를 낚아챌 수 있도록 public 구역에 배치
    UFUNCTION(BlueprintPure, Category = "Character|ADS")
    FVector CalculateADSOffset() const;

    // 애님인스턴스용 인라인 게터 탑재
    FORCEINLINE bool IsAiming() const { return bIsAiming; }
    FORCEINLINE FVector GetCurrentADSOffset() const { return CurrentADSOffset; }

public:
    // --- ISRCharacterInterface 구현부 ---
    virtual void AttachWeaponToHands(class AActor* WeaponActor, FName EquipSocketName) override;
    virtual void AttachWeaponToHolster(class AActor* WeaponActor, FName HolsterSocketName) override;
    virtual void PlayWeaponMontage(class UAnimMontage* MontageToPlay, bool bFirstPersonOnly) override;
    virtual void ApplyRecoil(float PitchAmount, float YawAmount) override;
    virtual USkeletalMeshComponent* Get1PMesh() const override;
    virtual void ApplyWeaponAnimLayer() override;
    
public:
    // --- 무기 애니메이션 및 판정 관련 ---
    void LinkWeaponAnimLayers(TSubclassOf<class UAnimInstance> TP_Layer, TSubclassOf<class UAnimInstance> FP_Layer);
    void UnlinkWeaponAnimLayers(TSubclassOf<class UAnimInstance> TP_Layer, TSubclassOf<class UAnimInstance> FP_Layer); 

    virtual void HandleWeaponChanged(class USRWeaponDataAsset* NewWeaponData) override;

 
    
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    class USkeletalMeshComponent* GetWeaponMeshForComponent(class USkeletalMeshComponent* PlayerMesh);

    void SaveCharacterState(class USRGameInstance* GI);
    void LoadCharacterState(class USRGameInstance* GI);

    EParkourType DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal);

    UPROPERTY(EditDefaultsOnly, Category = "Animation|Parkour")
    class UAnimMontage* LowVaultMontage; 

    UPROPERTY(EditDefaultsOnly, Category = "Animation|Parkour")
    class UAnimMontage* HighMantleMontage;

    FRotator InitialSocketRot;
    FRotator InitialControlRot;

    void StartGrapple(FVector TargetLocation);
    void StopGrapple();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
    TSubclassOf<class UAnimInstance> CurrentFPLayer;
    
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Mesh")
    TObjectPtr<class USkeletalMeshComponent> Mesh1P;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    class USkeletalMeshComponent* Cloned1PMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Movement")
    class UMotionWarpingComponent* MotionWarpingComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class USpringArmComponent> CameraBoom;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UCameraComponent> Camera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UUserWidget> MainHUDWidget;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = UI, Meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UUserWidget> HUDWidgetClass;
    
    // --- 입력(Input) 관련 ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<class UInputMappingContext> InputMappingContext;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputAbilities")
    TMap<EInputAction, TSubclassOf<class UGameplayAbility>> InputAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<class UInputAction> DashAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<class UInputAction> SprintAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> JumpAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> SlideAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> GrappleAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> AttackAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> ReloadAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> CycleWeaponAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> AimAction;
    
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    virtual void Jump() override;
    void Slide(const FInputActionValue& Value);
    void OnInteract(const FInputActionValue& Value);
    void Input_CycleWeapon(const FInputActionValue& Value);

    void HandleReloadOrRespawn();

protected:
    EGrappleState GrappleState = EGrappleState::Idle;
    FVector GrappleTargetLocation;
    FVector CurrentCableEndLocation;

    UPROPERTY(EditDefaultsOnly, Category = "Grapple")
    float DeploySpeed = 10000.0f; 

    UPROPERTY(EditDefaultsOnly, Category = "Grapple")
    float RetractSpeed = 15000.0f; 
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple")
    class UCableComponent* GrappleCable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float InteractTraceRadius = 25.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float InteractDistance = 250.0f;

    float LastFallingVelocity = 0.0f;
    virtual void Landed(const FHitResult& Hit) override;

    UPROPERTY(EditDefaultsOnly, Category = "Camera")
    TSubclassOf<ULegacyCameraShake> MovementShakeClass;
    
    UPROPERTY(EditDefaultsOnly, Category = "Camera")
    TSubclassOf<ULegacyCameraShake> LandShakeClass;

    UPROPERTY(EditDefaultsOnly, Category = "Camera")
    TSubclassOf<ULegacyCameraShake> SlideShakeClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    float HUDLocationInterpSpeed = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    float HUDRotationInterpSpeed = 18.0f;

    UPROPERTY()
    class ULegacyCameraShake* ActiveMovementShake;

    float CurrentShakeScale = 0.0f;

    // 🌟 [수리] 직관적인 리딩 및 리플렉션을 위한 명밀한 캡슐화 상태 변수 유지
    UPROPERTY(BlueprintReadOnly, Category = "Character State")
    bool bIsAiming = false;

    UPROPERTY(BlueprintReadOnly, Category = "ADS")
    FVector CurrentADSOffset = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "ADS")
    FVector TargetADSOffset = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Character|Aim")
    FRotator CurrentADSRotationOffset;

    FRotator TargetADSRotationOffset;

    TWeakObjectPtr<ASRGrapplePoint> CurrentTargetPoint;

    void TickGrappleTargetDetection();
    void AdjustHUDResolution();

public:
    void SetupGASInputComponent();
    void GASInputPressed(int32 InputId);
    void GASInputReleased(int32 InputId);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX", meta = (AllowPrivateAccess = "true"))
    class UNiagaraComponent* SpeedLinesVFX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    float SpeedVFXThreshold = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    float BaseFOV = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    float SprintFOV = 115.0f; 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    float MinSpeedForFOV = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    float MaxSpeedForFOV = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    float FOVInterpSpeed = 8.0f;
};