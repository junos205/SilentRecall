// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "SRAT_Play1PMontageAndWait.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSRPlay1PMontageAndWaitDelegate);

UCLASS()
class SILENTRECALL_API USRAT_Play1PMontageAndWait : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FSRPlay1PMontageAndWaitDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FSRPlay1PMontageAndWaitDelegate OnInterrupted;

	// ⭐️ SRAT_ 네이밍 컨벤션 적용 프록시 팩토리
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static USRAT_Play1PMontageAndWait* CreatePlay1PMontageAndWaitProxy(
		UGameplayAbility* OwningAbility, 
		FName TaskInstanceName, 
		UAnimMontage* MontageToPlay, 
		float PlayRate = 1.0f
	);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerDestroyed) override;

private:
	void OnMontageTimerFinished();

	UPROPERTY()
	UAnimMontage* MontageToPlay;

	float PlayRate;
	FTimerHandle TimerHandle;
};