// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/SRItemPickupBase.h"
#include "SRHealthPickup.generated.h"

UCLASS()
class SILENTRECALL_API ASRHealthPickup : public ASRItemPickupBase
{
	GENERATED_BODY()

public:
	ASRHealthPickup();

protected:
	/** 🌟 부모의 픽업 이벤트를 오버라이드하여 회복 로직을 수행합니다. */
	virtual void OnPickedUp(class USRInventoryComponent* InventoryComp) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|GAS")
	TSubclassOf<class UGameplayEffect> HealGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|GAS")
	float HealAmount = 5;
};