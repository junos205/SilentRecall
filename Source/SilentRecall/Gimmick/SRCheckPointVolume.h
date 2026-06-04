// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Character/SREnemyCharacterBase.h"
#include "SRCheckpointVolume.generated.h"

UCLASS()
class SILENTRECALL_API ASRCheckpointVolume : public AActor
{
	GENERATED_BODY()
    
public:	
	ASRCheckpointVolume();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* TriggerBox;

	/** 부활할 때 플레이어가 쳐다볼 방향 피벗 포인터 */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UArrowComponent* SpawnTransformArrow;

	/** 이 세이브 포인트의 고유 번호 (1, 2, 3...) */
	UPROPERTY(EditAnywhere, Category = "Checkpoint Settings")
	int32 CheckpointIndex = 1;

	/** 🌟 원래 원하셨던 에디터 직접 배치형 적 리스트 구조 그대로 유지 */
	UPROPERTY(EditAnywhere, Category = "Checkpoint Settings")
	TArray<ASREnemyCharacterBase*> AssignedEnemies;
};