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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
protected:

	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCost(
		const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	AActor* FindExecutionTarget();
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

protected:
	/** 조준 보정을 실시간으로 굴려줄 루프 타이머 핸들 */
	FTimerHandle MeleeLockOnTimerHandle;

	/** 현재 조준 보정 목표물 */
	TWeakObjectPtr<AActor> LockedOnTarget;

	/** 보정 강도 (높을수록 적을 회전시키는 속도가 빨라짐) */
	UPROPERTY(EditAnywhere, Category = "Melee|LockOn")
	float LockOnInterpSpeed = 12.0f;

	/** 보정 추적 반경 (cm) */
	UPROPERTY(EditAnywhere, Category = "Melee|LockOn")
	float LockOnRadius = 400.0f;

	/** 실시간 추적 루프 함수 */
	void ExecuteMeleeLockOnTick();

	/** 추적 대상을 서치하는 내부 함수 */
	AActor* ScanMeleeLockOnTarget() const;

	/** 모든 카메라 잠금 상태를 초기화하고 안전하게 해제하는 함수 */
	void ClearMeleeLockOn();
};