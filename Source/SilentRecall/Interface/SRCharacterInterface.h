#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SRCharacterInterface.generated.h"

UINTERFACE(MinimalAPI)
class USRCharacterInterface : public UInterface  { GENERATED_BODY() };

UENUM(BlueprintType)
enum class EHitDirection : uint8
{
	Front,
	Back,
	Left,
	Right
};

class SILENTRECALL_API ISRCharacterInterface
{
	GENERATED_BODY()

public:
	virtual void ApplyWeaponAnimLayer() = 0;
	
	// 인벤토리가 무기를 주면, 캐릭터가 알아서 3P 손에 붙이고 1P용 복제본을 생성합니다.
	virtual void AttachWeaponToHands(class AActor* WeaponActor, FName EquipSocketName) = 0;
    
	// 인벤토리가 무기를 주면, 캐릭터가 1P 복제본을 삭제하고 원본을 등에 붙입니다.
	virtual void AttachWeaponToHolster(class AActor* WeaponActor, FName HolsterSocketName) = 0;

	// 무기 교체 애니메이션 재생
	virtual void PlayWeaponMontage(class UAnimMontage* MontageToPlay, bool bFirstPersonOnly = false) = 0;

	// 반동 적용 함수 
	virtual void ApplyRecoil(float PitchAmount, float YawAmount) = 0;

	virtual class USkeletalMeshComponent* Get1PMesh() const = 0;

	virtual class UAnimMontage* GetHitReactMontage(EHitDirection Direction) { return nullptr; }

	virtual void PlayDoubleJumpSound() {}
	virtual void PlayWallJumpSound() {}
	virtual void PlaySlideJumpSound() {}
};