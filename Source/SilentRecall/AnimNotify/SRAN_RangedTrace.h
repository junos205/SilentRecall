// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "SRAN_RangedTrace.generated.h"

/**
 * 
 */
UCLASS()
class SILENTRECALL_API USRAN_RangedTrace : public UAnimNotify
{
	GENERATED_BODY()

public:
	USRAN_RangedTrace();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// 블루프린트 노티파이 창에서 쏠 사거리를 세팅할 수 있게 노출!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackRange = 10000.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FGameplayTag FireEventTag;
};
