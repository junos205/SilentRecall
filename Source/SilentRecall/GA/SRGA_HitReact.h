#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_HitReact.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_HitReact : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_HitReact();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact|VFX")
	class UNiagaraSystem* HitNiagaraVFX;

	/** 🌟 [신규] 플레이어가 피격당했을 때 격발할 카메라 쉐이크 에셋 클래스 */
	UPROPERTY(EditDefaultsOnly, Category = "Effects|PlayerOnly")
	TSubclassOf<class UCameraShakeBase> PlayerHitCameraShake;

	/** 🌟 [신규] 플레이어 피격 시 HUD 테두리 레드 플래시 UI를 켜기 위한 이벤트 태그 또는 Cue */
	UPROPERTY(EditDefaultsOnly, Category = "Effects|PlayerOnly")
	FGameplayTag PlayerHitUIEventTag;
	// 몽타주 재생이 끝났을 때 스킬을 종료할 콜백 함수
	UFUNCTION()
	void OnMontageCompleted();
};