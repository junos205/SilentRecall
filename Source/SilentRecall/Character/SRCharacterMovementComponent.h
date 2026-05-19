// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SRCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum ECustomMovementMode : uint8
{
	CMOVE_None          UMETA(Hidden),
	CMOVE_WallRunning   UMETA(DisplayName = "Wall Running"),
	CMOVE_Sliding       UMETA(DisplayName = "Sliding"),
	CMOVE_Grapling		UMETA(DisplayName = "Grapling")
};

UENUM(BlueprintType)
enum class EWallDirection : uint8
{
	Left,
	Right
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SILENTRECALL_API USRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USRCharacterMovementComponent();

	void DoWallJump();

	float TargetWallRunRoll = 0.0f;
protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

public:
	void EnterSlide();
	void ExitSlide();

	void DoSlideJump();

public:
	void EnterGraple(FVector InHookLocation);
	void ExitGraple();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	float MaxWallRunRollAngle = 15.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	float MaxWallWalkSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	float WallStickiness = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	float WallSeizeThreshold = 0.3f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	float WallJumpHeight = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	float WallRepulsiveForce = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	float WallPropulsionForce = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	float WallInputCorrection = 150.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	bool bIsRightWall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	FVector WallNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	FVector WallHitLocationtion = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WallMovement", meta=(AllowPrivateAccess=true))
	FVector WallDir = FVector::ZeroVector;
protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
    
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	void PhysWallRunning(float deltaTime, int32 Iterations);

	void PhysSliding(float deltaTime, int32 Iterations);

	void PhysGrapling(float deltaTime, int32 Iterations);

protected:

	float TimeOnWall = 0.0f;

	float WallRunCooldown = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "WallRun")
	float WallRunIdleGracePeriod = 0.15f;

private:
	// 벽 감지 함수
	bool TryWallRun();

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sliding", meta=(AllowPrivateAccess=true))
	float SlideForce = 150.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Sliding")
	float MinSlideSpeed = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Sliding")
	float MaxSlideSpeed = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Sliding")
	float SlideFriction = 0.5f; 

	UPROPERTY(EditDefaultsOnly, Category = "Sliding")
	float SlideJumpForce = 600.0f;
	
	UPROPERTY(Transient)
	float EntrySlideSpeed = 0.0f;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Grapling")
	FVector HookLocation;

	UPROPERTY(EditDefaultsOnly, Category = "Grapling")
	float CableLength = 1000;
};
