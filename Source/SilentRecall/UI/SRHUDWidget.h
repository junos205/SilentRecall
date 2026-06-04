// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h" // 🌟 FOnAttributeChangeData 사용을 위해 필수 포함
#include "SRHUDWidget.generated.h"

UCLASS()
class SILENTRECALL_API USRHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 🌟 블루프린트의 BeginPlay와 같은 위젯 생성 시점 액션
	virtual void NativeConstruct() override;

	// 🌟 위젯 내부로 독립 이주한 GAS 어트리뷰트 변동 콜백 함수들
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleAPChanged(const FOnAttributeChangeData& Data);
	void HandleMaxAPChanged(const FOnAttributeChangeData& Data);
	void RefreshInitialHUD(class UAbilitySystemComponent* ASC);

public:
	/** 블루프린트(UMG)에서 이벤트 그래프로 받아 체력바(ProgressBar)나 텍스트를 갱신할 때 사용합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|GAS")
	void OnHealthChanged(float CurrentHealth, float MaxHealth);

	/** 블루프린트(UMG)에서 기력바(APBar)나 이펙트 수치를 갱신할 때 사용합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|GAS")
	void OnAPChanged(float CurrentAP, float MaxAP);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Death")
	void PlayDeathFadeOut();
};