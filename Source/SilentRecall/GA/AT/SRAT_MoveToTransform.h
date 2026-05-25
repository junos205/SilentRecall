#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "SRAT_MoveToTransform.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSRMoveToTransformDelegate);

UCLASS()
class SILENTRECALL_API USRAT_MoveToTransform : public UAbilityTask
{
	GENERATED_BODY()

public:
	USRAT_MoveToTransform(const FObjectInitializer& ObjectInitializer);

	// ⭐️ 적 액터(TargetActor)를 인자로 받도록 구조 확장
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static USRAT_MoveToTransform* SRMoveToTransform(
		UGameplayAbility* OwningAbility, 
		FVector TargetLocation, 
		FRotator TargetRotation, 
		AActor* TargetActor, // ⭐️ 여기에 적 액터를 추가합니다.
		float AbsoluteGroundZ, 
		float Duration);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

	UPROPERTY(BlueprintAssignable)
	FSRMoveToTransformDelegate OnTargetLocationReached;

private:
	FVector StartLocation;
	FRotator StartRotation;

	FVector GoalLocation;
	FRotator GoalRotation;

	// ⭐️ 실시간 추적을 위해 적 액터 포인터 보관
	UPROPERTY()
	AActor* ExecutionTargetActor;

	float GroundZ;
	float MoveDuration;
	float TimeElapsed;
	bool bIsMoving;
};