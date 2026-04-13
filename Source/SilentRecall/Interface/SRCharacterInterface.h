#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SRCharacterInterface.generated.h"

UINTERFACE(MinimalAPI)
class USRCharacterInterface : public UInterface  { GENERATED_BODY() };

class SILENTRECALL_API ISRCharacterInterface
{
	GENERATED_BODY()

public:
	// 인벤토리가 무기를 주면, 캐릭터가 알아서 3P 손에 붙이고 1P용 복제본을 생성합니다.
	virtual void AttachWeaponToHands(class AActor* WeaponActor, FName EquipSocketName) = 0;
    
	// 인벤토리가 무기를 주면, 캐릭터가 1P 복제본을 삭제하고 원본을 등에 붙입니다.
	virtual void AttachWeaponToHolster(class AActor* WeaponActor, FName HolsterSocketName) = 0;

	// 무기 교체 애니메이션 재생
	virtual void PlayWeaponMontage(class UAnimMontage* MontageToPlay) = 0;
};