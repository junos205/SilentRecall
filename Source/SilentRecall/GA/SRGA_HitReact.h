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

	// ⭐️ 4방향 피격 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact")
	UAnimMontage* HitFrontMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact")
	UAnimMontage* HitBackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact")
	UAnimMontage* HitLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact")
	UAnimMontage* HitRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact|VFX")
	class UNiagaraSystem* HitNiagaraVFX;

	// 몽타주 재생이 끝났을 때 스킬을 종료할 콜백 함수
	UFUNCTION()
	void OnMontageCompleted();
};