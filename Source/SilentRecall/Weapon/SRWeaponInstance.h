#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/SRWeaponDataAsset.h" // 우리가 아까 만든 데이터 애셋 원본
#include "SRWeaponInstance.generated.h"

UCLASS(BlueprintType, Blueprintable)
class SILENTRECALL_API USRWeaponInstance : public UObject
{
	GENERATED_BODY()

public:
	// ⭐️ 1. 나는 어떤 무기인가? (불변하는 원본 데이터 포인터)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Data")
	USRWeaponDataAsset* WeaponData;

	// ⭐️ 2. 내 현재 상태는 어떤가? (변하는 인스턴스 데이터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon State")
	int32 CurrentAmmoInMag;

	// 생성할 때 값을 쏙 넣어주는 편의성 함수
	void InitializeInstance(USRWeaponDataAsset* InWeaponData, int32 InAmmo)
	{
		WeaponData = InWeaponData;
		CurrentAmmoInMag = InAmmo;
	}
};