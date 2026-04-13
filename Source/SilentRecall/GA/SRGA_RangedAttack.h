// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_RangedAttack.generated.h"

/**
 * 
 */
UCLASS()
class SILENTRECALL_API USRGA_RangedAttack : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	USRGA_RangedAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// ⭐️ 애니메이션 노티파이에서 보낸 '격발' 신호를 받을 함수
	UFUNCTION()
	void OnFireEventReceived(FGameplayEventData Payload);

	// 몽타주 종료 시 GA 종료
	UFUNCTION()
	void OnMontageCompleted();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Damage")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Stats")
	float AttackRange = 10000.0f; // 기본 사거리 100m
};
