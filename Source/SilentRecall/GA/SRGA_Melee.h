// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_Melee.generated.h"

/**
 * 
 */
UCLASS()
class SILENTRECALL_API USRGA_Melee : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_Melee();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
protected:
	// 현재 진행 중인 콤보 카운트 (1타, 2타...)
	int32 CurrentComboIndex = 1;

	// 최대 콤보 수 (데이터 애셋에서 가져와도 되지만, 임시로 3타로 설정)
	int32 MaxComboCount = 3;

	// 플레이어가 애니메이션 도중에 '다음 클릭'을 미리 눌렀는지 저장 (선입력)
	bool bIsComboSaved = false;
	
	// 섹션을 재생하는 헬퍼 함수
	void PlayComboSection();

	// 몽타주가 끝났을 때 호출
	UFUNCTION()
	void OnMontageCompleted();

	// 몽타주 재생 중 '타격 지점' 노티파이가 이벤트를 보냈을 때 호출
	UFUNCTION()
	void OnHitEventReceived(FGameplayEventData Payload);

	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	// 타이밍 체크용 이벤트 캐치 함수는 그대로 둡니다.
	UFUNCTION()
	void OnComboCheckEventReceived(FGameplayEventData Payload);
};