#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_Parry.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_Parry : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_Parry();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// ==========================================================
	// 🎬 몽타주 세팅
	// ==========================================================
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	UAnimMontage* ParryAnticipationMontage; // 패링 대기 (칼을 들어 올리는 동작)

	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	UAnimMontage* ParrySuccessMontage;      // 패링 성공 (튕겨내는 멋진 동작)

private:
	// ==========================================================
	// 📡 비동기 태스크 콜백 함수들
	// ==========================================================
    
	// 대기 몽타주가 끝났을 때 (패링 실패/시간 초과)
	UFUNCTION()
	void OnAnticipationMontageEnded();

	// ExecCalc에서 패링 성공 이벤트를 받았을 때!
	UFUNCTION()
	void OnParryEventReceived(FGameplayEventData Payload);

	// 성공 몽타주까지 다 끝났을 때
	UFUNCTION()
	void OnSuccessMontageEnded();
};