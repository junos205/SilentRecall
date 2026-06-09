#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_AimDownSights.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_AimDownSights : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_AimDownSights();

	// 🌟 [신규 추가] 능력이 발동하기 전, "지금 조준을 켤 수 있는 상황인가?"를 판별하는 가드 스크립트
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
};