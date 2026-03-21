#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_Grappling.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_Grappling : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_Grappling();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	
protected:
	// 키보드/마우스에서 손을 뗐을 때 실행될 콜백 함수
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grapple")
	float GrappleRange = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Grapple")
	bool bDrawDebug = true;
};