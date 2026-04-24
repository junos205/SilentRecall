#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_RangedAttack.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_RangedAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_RangedAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    
	// 유저가 마우스 버튼을 뗐을 때 호출 (연사 중지용)
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	// 실제 사격 로직 (총알 소비, 몽타주 재생, 연사 루프)
	UFUNCTION()
	void FireShot();

	// AnimNotify에서 보낸 "총알이 맞았다!" 이벤트를 수신하는 함수
	UFUNCTION()
	void OnFireEventReceived(FGameplayEventData Payload);

	// 몽타주 재생이 끝났을 때 처리
	UFUNCTION()
	void OnMontageCompleted();

	// 지연 시간(FireRate) 후 GA를 깔끔하게 종료하기 위한 델리게이트
	UFUNCTION()
	void EndAbilityDelegate();

	// 데미지를 줄 게임플레이 이펙트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Damage")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;
};