// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_Dash.generated.h"

/**
 * 
 */
UCLASS()
class SILENTRECALL_API USRGA_Dash : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_Dash();

	// 어빌리티가 발동될 때 호출되는 메인 함수
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
protected:
	// 루트 모션(대시)이 끝났을 때 어빌리티를 종료시켜줄 함수
	UFUNCTION()
	void OnDashCompleted();

	// 기획적으로 조절할 대시 세팅값들
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	float DashStrength = 3000.f; // 대시 속도

	UPROPERTY(EditDefaultsOnly, Category="Dash")
	float DashDuration = 0.2f;   // 대시 지속 시간
};