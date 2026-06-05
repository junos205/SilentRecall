// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SRHUDWidget.generated.h"

UCLASS()
class SILENTRECALL_API USRHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 블루프린트(UMG)에서 이벤트 그래프로 받아 체력바(ProgressBar)나 텍스트를 갱신할 때 사용합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|GAS")
	void OnHealthChanged(float CurrentHealth, float MaxHealth);

	/** 블루프린트(UMG)에서 기력바(APBar)나 이펙트 수치를 갱신할 때 사용합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|GAS")
	void OnAPChanged(float CurrentAP, float MaxAP);
};