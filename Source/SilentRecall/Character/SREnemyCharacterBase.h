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

	// ⭐️ 제일 처음 기획하셨던 깔끔한 방식! 데이터 애셋 하나로 전부 처리합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	class USRWeaponDataAsset* DefaultWeaponData;
    
	// (선택) AI가 스폰할 무기 3D 모델(액터) 클래스. 
	// 만약 WeaponDataAsset 안에 WeaponClass가 있다면 이 변수도 필요 없습니다!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	TSubclassOf<class AActor> DefaultWeaponActorClass; 
};