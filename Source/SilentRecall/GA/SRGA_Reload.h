#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_Reload.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_Reload : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_Reload();

	// 1. 어빌리티 발동 조건 검사 (총알이 꽉 찼거나 예비 탄약이 없으면 발동 불가)
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// 2. 어빌리티 실제 실행부 (애니메이션 재생 및 이벤트 대기)
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 3. 어빌리티 종료 (애니메이션이 끝나거나 캔슬되었을 때)
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// ⭐️ 애니메이션 재생을 담당할 태스크 콜백 함수들
	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageCancelled();

	// ⭐️ 애니메이션 노티파이에서 보내는 '장전 완료' 이벤트를 받을 콜백 함수
	UFUNCTION()
	void OnReloadEventReceived(FGameplayEventData Payload);

	// 이벤트 대기용 태그 (블루프린트에서 설정할 수 있게 노출)
	UPROPERTY(EditDefaultsOnly, Category = "Reload")
	FGameplayTag ReloadEventTag;
};