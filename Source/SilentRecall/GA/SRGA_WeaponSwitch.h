// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_WeaponSwitch.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_WeaponSwitch : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_WeaponSwitch();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 1단계: 구무기 집어넣기 시동 및 블랙박스 스캔 */
	void PlayUnEquipStep();

	/** 1단계 완료 마디 추적 콜백 */
	UFUNCTION()
	void OnUnEquipCompleted();

	/** 2단계: 신무기 꺼내기 시동 및 블랙박스 스캔 */
	void PlayEquipStep();

	/** 2단계 완료 마디 추적 콜백 */
	UFUNCTION()
	void OnEquipCompleted();

	UFUNCTION()
	void OnSwitchInterrupted();

private:
	/** 실시간 디버깅용 블랙박스 데이터 출력기 */
	void PrintWeaponBlackBox(const FString& StepContext);

private:
	UPROPERTY()
	class USRInventoryComponent* CachedInventory;
};