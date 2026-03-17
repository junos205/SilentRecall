// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SRCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None          UMETA(Hidden),
	CMOVE_WallRunning   UMETA(DisplayName = "Wall Running"),
	CMOVE_Sliding       UMETA(DisplayName = "Sliding")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SILENTRECALL_API USRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USRCharacterMovementComponent();

	void DoWallJump();

protected:
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
	float SlideForce = 150.f;
protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
    
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	void PhysWallRunning(float deltaTime, int32 Iterations);

	float TimeOnWall = 0.0f;

	float WallRunCooldown = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "WallRun")
	float WallRunIdleGracePeriod = 0.15f;

private:
	// 벽 감지 함수
	bool TryWallRun();
    
	// 현재 타고 있는 벽의 정보 (오른쪽 벽인지 왼쪽 벽인지 판별)
	bool bIsRightWall;
	FVector WallNormal;

public:
	void EnterSlide();
	void ExitSlide();

	void DoSlideJump();

protected:
	void PhysSliding(float deltaTime, int32 Iterations);

	UPROPERTY(EditDefaultsOnly, Category = "Sliding")
	float MinSlideSpeed = 400.0f; 

	UPROPERTY(EditDefaultsOnly, Category = "Sliding")
	float SlideFriction = 0.5f; 

	UPROPERTY(EditDefaultsOnly, Category = "Sliding")
	float SlideJumpForce = 600.0f; 
};
