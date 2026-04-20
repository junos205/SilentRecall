#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_Death.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_Death : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_Death();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnMontageCompleted();

	// ⭐️ 사망 방향별 몽타주 (정면에서 맞으면 뒤로 넘어짐 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* DeathFrontMontage; // 앞에서 맞았을 때 (보통 뒤로 쓰러짐)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* DeathBackMontage;  // 뒤에서 맞았을 때 (보통 앞으로 엎어짐)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* DeathLeftMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* DeathRightMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Procedural Slicing")
	class UMaterialInterface* FleshCapMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dismemberment")
	class UStaticMesh* FleshPlugMesh;

	// 🩸 피 분수 파티클
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	class UParticleSystem* BloodSpurtVFX;
};