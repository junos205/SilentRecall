// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_GloryKill.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_GloryKill : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_GloryKill();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 처형 가능한 적을 탐색하는 레이더 함수
	AActor* FindExecutionTarget();
    
	// 처형을 실행(동기화 및 몽타주 재생)하는 함수
	void PlayExecution(AActor* TargetActor);

	UFUNCTION()
	void OnMontageCompleted();

public:
	// ⭐️ 내 캐릭터가 재생할 멋진 처형 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GloryKill")
	UAnimMontage* AttackerMontage;

	// ⭐️ 적이 멱살 잡히거나 맞는 리액션을 할 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GloryKill")
	UAnimMontage* VictimMontage;

	// ⭐️ 처형이 끝나는 시점에 적을 확실히 죽일 즉사 데미지 이펙트 (GE)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GloryKill")
	TSubclassOf<class UGameplayEffect> ExecutionDamageEffect;

private:
	UPROPERTY()
	AActor* CurrentVictim;
};