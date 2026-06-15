// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"// 기존 상위 클래스에 맞게 유지 (UGameplayAbility)
#include "Abilities/GameplayAbility.h"
#include "SRGA_Dash.generated.h"

// 나이아가라 전방 선언
class UNiagaraSystem;

UCLASS()
class SILENTRECALL_API USRGA_Dash : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_Dash();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnDashCompleted();

	UPROPERTY(EditDefaultsOnly, Category="Dash")
	float DashStrength = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category="Dash")
	float DashDuration = 0.2f;

	// 🎯 에디터 기획에서 방향별로 에셋을 등록할 변수들
	UPROPERTY(EditDefaultsOnly, Category="Dash|Visual")
	UNiagaraSystem* ForwardDashFX;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Visual")
	UNiagaraSystem* BackwardDashFX;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Visual")
	UNiagaraSystem* LeftDashFX;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Visual")
	UNiagaraSystem* RightDashFX;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Visual")
	UNiagaraSystem* UpwardDashFX;

	UPROPERTY(EditDefaultsOnly, Category="Dash|Visual")
	UNiagaraSystem* DownwardDashFX;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Audio", meta = (AllowPrivateAccess = "true"))
	class USoundBase* DashSound;
};