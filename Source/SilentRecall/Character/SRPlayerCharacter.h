// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SRBaseCharacter.h"
#include "CableComponent.h"
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
class SILENTRECALL_API ASRPlayerCharacter : public ASRBaseCharacter
{
	GENERATED_BODY()

public:
	ASRPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	
	// 매 프레임 속도를 체크하기 위해 Tick 함수 오버라이드
	virtual void Tick(float DeltaTime) override;

	FORCEINLINE void AddInputAbility(EInputAction InputAction, TSubclassOf<UGameplayAbility> AbilityToGrant)
	{
		if (ASC && !InputAbilities.Contains(InputAction))
		{
			InputAbilities.Add(InputAction, AbilityToGrant);
			SetupGASInputComponent(); // 입력 컴포넌트 재설정
		}
	}

public:
	// 무기 애니메이션 레이어를 연결(입기)하는 함수
	void LinkWeaponAnimLayers(TSubclassOf<UAnimInstance> TP_Layer, TSubclassOf<UAnimInstance> FP_Layer);

	// 무기 애니메이션 레이어를 해제(벗기)하는 함수
	void UnlinkWeaponAnimLayers(TSubclassOf<UAnimInstance> TP_Layer, TSubclassOf<UAnimInstance> FP_Layer); 

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<class USkeletalMeshComponent> Mesh1P;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	class UMotionWarpingComponent* MotionWarpingComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Animation|Parkour")
	class UAnimMontage* LowVaultMontage; // 허리용 (예: 60~130cm)

	UPROPERTY(EditDefaultsOnly, Category = "Animation|Parkour")
	class UAnimMontage* HighMantleMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<class UInputMappingContext> InputMappingContext;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCameraComponent> Camera;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputAbilities)
	TMap<EInputAction, TSubclassOf<UGameplayAbility>> InputAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TObjectPtr<class UInputAction> DashAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TObjectPtr<class UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> SlideAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> GrappleAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> AttackAction;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	virtual void Jump() override;
	
	void Slide(const FInputActionValue& Value);

	bool TryVault();
	void EndVault(UAnimMontage* Montage, bool bInterrupted);

	EParkourType DetectLedge(FVector& OutLedgeLocation, FVector& OutWallNormal);

	void OnInteract(const FInputActionValue& Value);

public:
	void StartGrapple(FVector TargetLocation);
	void StopGrapple();

protected:
	EGrappleState GrappleState = EGrappleState::Idle;
    
	FVector GrappleTargetLocation;    // 훅이 박힐 최종 목적지
	FVector CurrentCableEndLocation;  // 현재 줄의 끝점 (날아가는 중인 좌표)

	UPROPERTY(EditDefaultsOnly, Category = "Grapple")
	float DeploySpeed = 10000.0f; // 줄이 날아가는 속도 (cm/s)

	UPROPERTY(EditDefaultsOnly, Category = "Grapple")
	float RetractSpeed = 15000.0f; // 줄이 감기는 속도 (cm/s)
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple")
	class UCableComponent* GrappleCable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractTraceRadius = 25.0f; 

	// 상호작용 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractDistance = 250.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour")
	bool bIsVaulting = false;

protected:
	// ⭐️ 파쿠르 시작 시점의 각도를 기억할 변수 두 개
	UPROPERTY()
	FRotator InitialSocketRot;

	UPROPERTY()
	FRotator InitialControlRot;

public:
	virtual void PossessedBy(AController* NewController) override;

	// GAS관련 입력 바인딩 함수
	void SetupGASInputComponent();

	// GAS관련 입력 핸들 함수
	void GASInputPressed(int32 InputId);
	void GASInputReleased(int32 InputId);

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// 스피드 라인 VFX 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX", meta = (AllowPrivateAccess = "true"))
	class UNiagaraComponent* SpeedLinesVFX;

	// FOV 및 속도 연출 설정값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float SpeedVFXThreshold = 800.0f; // 이 속도를 넘으면 연출 시작

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float BaseFOV = 90.0f; // 기본 시야각

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float SprintFOV = 115.0f; // 질주 시 시야각 (넓어질수록 속도감 극대화)

};
