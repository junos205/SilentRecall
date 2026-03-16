// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_Melee.generated.h"

/**
 * 
 */
UCLASS()
class SILENTRECALL_API USRGameplayAbility_Melee : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGameplayAbility_Melee();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 재생할 공격 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	UAnimMontage* AttackMontage;

	// 적에게 입힐 데미지 이펙트(GE) 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	// 히트 판정을 위한 트레이스(선 긋기) 거리 및 반경
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	float TraceDistance = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	float TraceRadius = 40.0f;

	// 몽타주가 끝났을 때 호출
	UFUNCTION()
	void OnMontageCompleted();

	// 몽타주 재생 중 '타격 지점' 노티파이가 이벤트를 보냈을 때 호출
	UFUNCTION()
	void OnHitEventReceived(FGameplayEventData Payload);
};