// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "SRHUDControllerComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SILENTRECALL_API USRHUDControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:    
	USRHUDControllerComponent();

protected:
	virtual void BeginPlay() override;

	// GAS 어트리뷰트 변경 시 호출될 콜백 함수들
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleAPChanged(const FOnAttributeChangeData& Data);
	void HandleMaxAPChanged(const FOnAttributeChangeData& Data);

	// 인게임 진입 직후 UI 초기화용
	void RefreshInitialHUD();

private:
	UPROPERTY()
	class UAbilitySystemComponent* ASC;

	UPROPERTY()
	class USRHUDWidget* TargetHUDWidget;
};