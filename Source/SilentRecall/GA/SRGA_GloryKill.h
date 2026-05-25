#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SRGA_GloryKill.generated.h"

UCLASS()
class SILENTRECALL_API USRGA_GloryKill : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USRGA_GloryKill();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 처형 핵심 시퀀스 처리 함수
	void PlayExecution(AActor* TargetActor);

	// 애님 노티파이 이벤트 수신 함수
	UFUNCTION()
	void OnExecuteHitNotifyReceived(FGameplayEventData Payload);

	// 몽타주 종료 수신 함수
	UFUNCTION()
	void OnMontageCompleted();

	// 틱 대신 카메라 부드러운 회전을 담당할 타이머 루프 함수
	void UpdateCameraRotation();

	// 처형 대상 탐색 함수
	AActor* FindExecutionTarget();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GloryKill")
	UAnimMontage* AttackerMontage;

	UPROPERTY(EditDefaultsOnly, Category = "GloryKill")
	UAnimMontage* VictimMontage;

	UPROPERTY(EditDefaultsOnly, Category = "GloryKill")
	TSubclassOf<class UGameplayEffect> ExecutionDamageEffect;

private:
	// ⭐️ 컴파일 에러 해결용 핵심 변수 선언부
	UPROPERTY()
	AActor* CurrentVictim;

	FTimerHandle CameraRotationTimerHandle;
    
	bool bIsRotatingCamera = false;
};