// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SRBaseCharacter.h"
#include "CableComponent.h"
#include "Interface/SRCharacterInterface.h"
#include "InputActionValue.h"
#include "SRPlayerCharacter.generated.h"

UENUM(BlueprintType)
enum class EGrappleState : uint8
{
    Idle,       // 대기 중
    Deploying,  // 줄이 날아가는 중
    Swinging,   // 벽에 박혀서 스윙 중
    Retracting  // 줄을 감는 중
};

UENUM(BlueprintType)
enum class EParkourType : uint8
{
    None,
    LowVault,   // 허리춤 높이 (짚고 넘기)
    HighMantle  // 머리/가슴 높이 (매달려 오르기)
};

UCLASS()
class SILENTRECALL_API ASRPlayerCharacter : public ASRBaseCharacter, public ISRCharacterInterface
{
    GENERATED_BODY()

public:
    ASRPlayerCharacter(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    FORCEINLINE void AddInputAbility(EInputAction InputAction, TSubclassOf<class UGameplayAbility> AbilityToGrant)
    {
       if (ASC && !InputAbilities.Contains(InputAction))
       {
          InputAbilities.Add(InputAction, AbilityToGrant);
          SetupGASInputComponent(); 
       }
    }

public:
    // --- ISRCharacterInterface 구현부 ---
    virtual void AttachWeaponToHands(class AActor* WeaponActor, FName EquipSocketName) override;
    virtual void AttachWeaponToHolster(class AActor* WeaponActor, FName HolsterSocketName) override;
    virtual void PlayWeaponMontage(class UAnimMontage* MontageToPlay) override;
    
public:
    // --- 무기 애니메이션 및 판정 관련 ---
    void LinkWeaponAnimLayers(TSubclassOf<class UAnimInstance> TP_Layer, TSubclassOf<class UAnimInstance> FP_Layer);
    void UnlinkWeaponAnimLayers(TSubclassOf<class UAnimInstance> TP_Layer, TSubclassOf<class UAnimInstance> FP_Layer); 

    UFUNCTION()
    void HandleWeaponChanged(class USRWeaponDataAsset* NewWeaponData);

    // ⭐️ [추가됨] 근접 공격 노티파이에서 1P/3P를 구분하여 알맞은 무기 메쉬를 반환
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    class USkeletalMeshComponent* GetWeaponMeshForComponent(class USkeletalMeshComponent* PlayerMesh);

    // ⭐️ [추가됨] 노티파이에서 1P 메쉬인지 확인하기 위한 Getter
    FORCEINLINE class USkeletalMeshComponent* Get1PMesh() const { return Mesh1P; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
    TSubclassOf<class UAnimInstance> CurrentTPLayer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
    TSubclassOf<class UAnimInstance> CurrentFPLayer;
    
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Mesh")
    TObjectPtr<class USkeletalMeshComponent> Mesh1P;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    class USkeletalMeshComponent* Cloned1PMesh;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    class UMotionWarpingComponent* MotionWarpingComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Animation|Parkour")
    class UAnimMontage* LowVaultMontage; 

    UPROPERTY(EditDefaultsOnly, Category = "Animation|Parkour")
    class UAnimMontage* HighMantleMontage;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UCameraComponent> Camera;
    
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

    // ⭐️ [추가됨] 마우스 휠 무기 교체 액션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", Meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UInputAction> CycleWeaponAction;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    virtual void Jump() override;
    void Slide(const FInputActionValue& Value);
    void OnInteract(const FInputActionValue& Value);
    
    // ⭐️ [추가됨] 휠 굴릴 때 실행될 함수
    void Input_CycleWeapon(const FInputActionValue& Value);

    bool TryVault();
    void EndVault(class UAnimMontage* Montage, bool bInterrupted);
    EParkourType DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal);

public:
    void StartGrapple(FVector TargetLocation);
    void StopGrapple();

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour")
    bool bIsVaulting = false;

    UPROPERTY()
    FRotator InitialSocketRot;

    UPROPERTY()
    FRotator InitialControlRot;

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
};