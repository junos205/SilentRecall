#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Character/SREnemyCharacterBase.h"
#include "GameplayTagContainer.h"
#include "SRCheckpointVolume.generated.h"

// 🌟 레벨 블루프린트 디테일 창에서 시퀀서나 연출을 바인딩할 수 있는 다이나믹 멀티캐스트 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllEnemiesDefeated);

UCLASS()
class SILENTRECALL_API ASRCheckpointVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	ASRCheckpointVolume();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UArrowComponent* SpawnTransformArrow;

public:
	UPROPERTY(EditAnywhere, Category = "Gimmick")
	int32 CheckpointIndex = 1;

	// 에디터 배치 창에서 이 구역에 배정할 적들을 드래그 앤 드롭으로 묶는 배열
	UPROPERTY(EditInstanceOnly, Category = "Gimmick")
	TArray<ASREnemyCharacterBase*> AssignedEnemies;

	// 🌟 [신규 추가] 적 전멸 시 활성화 시킬 다음 목적지 마커 액터 포인터
	UPROPERTY(EditInstanceOnly, Category = "Gimmick")
	AActor* NextObjectiveMarker;

	// 🌟 [신규 추가] 레벨 블루프린트에서 우클릭으로 꺼내 쓸 전멸 이벤트 노드
	UPROPERTY(BlueprintAssignable, Category = "Gimmick|Event")
	FOnAllEnemiesDefeated OnAllEnemiesDefeated;

private:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 🌟 GAS 사망 태그 실시간 변동 감지 콜백
	void OnEnemyDeathTagChanged(const FGameplayTag Tag, int32 NewCount);

	// 실시간 적 전멸 상태 채점기
	void EvaluateEnemiesSanity();

	bool bAllEnemiesDefeatedFired = false;
};