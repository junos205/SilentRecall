// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "SRKillVolume.generated.h"

UCLASS()
class SILENTRECALL_API ASRKillVolume : public AActor
{
	GENERATED_BODY()
    
public:	
	ASRKillVolume();

protected:
	virtual void BeginPlay() override;

	/** 볼륨에 무언가 겹쳤을 때 즉사를 판정할 오버랩 함수 */
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	/** 영역을 지정할 박스 컴포넌트 */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* TriggerBox;
};