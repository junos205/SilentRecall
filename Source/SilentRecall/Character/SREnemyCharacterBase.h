#pragma once

#include "CoreMinimal.h"
#include "Character/SRBaseCharacter.h"
#include "SREnemyCharacterBase.generated.h"

UCLASS()
class SILENTRECALL_API ASREnemyCharacterBase : public ASRBaseCharacter
{
	GENERATED_BODY()

public:
	ASREnemyCharacterBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void DestroyEnemySequence();
	UFUNCTION()
	void HandleOutOfHealth(AActor* TargetActor);

	// ⭐️ 제일 처음 기획하셨던 깔끔한 방식! 데이터 애셋 하나로 전부 처리합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	class USRWeaponDataAsset* DefaultWeaponData;
    
	// (선택) AI가 스폰할 무기 3D 모델(액터) 클래스. 
	// 만약 WeaponDataAsset 안에 WeaponClass가 있다면 이 변수도 필요 없습니다!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	TSubclassOf<class AActor> DefaultWeaponActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Drops")
	bool bDropCurrentWeapon = true;

	/** 🌟 [신규] 들고 있던 무기 외에 추가로 사방으로 무작위 투척할 아이템 픽업 클래스 리스트 (배열) 
	 * 여기에 원하는 무기 픽업이나 탄약 픽업 블루프린트 클래스들을 마음껏 채워 넣을 수 있습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Drops")
	TArray<TSubclassOf<AActor>> ItemDropTable;

private:
	// ⭐️ [신규 추가] 중복 사망 방지 및 LookAt 업데이트 차단용 플래그
	bool bIsDead = false;

	// ⭐️ [신규 추가] 언리얼 3초 타이머 핸들
	FTimerHandle DeathTimerHandle;
	 
};