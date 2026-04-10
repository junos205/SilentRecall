#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SRCharacterData.h"
#include "SRWeaponDataAsset.generated.h"

UCLASS(BlueprintType)
class SILENTRECALL_API USRWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName EquipSocketName = FName("HandGrip_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName HolsterSocketName = FName("HolsterSocket");

	// ⭐️ 무기 전용 애니메이션 레이어
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TSubclassOf<UAnimInstance> TP_AnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TSubclassOf<UAnimInstance> FP_AnimLayerClass;
	
	// 1. 기본 무기 정보
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	FName WeaponName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	EWeaponSlot WeaponSlotType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	TSubclassOf<class AActor> WeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	class UTexture2D* WeaponIcon;

	// ⭐️ 2. 핵심 해결책: TArray 대신 TMap 사용!
	// "어떤 키(EInputAction)"를 누르면 "어떤 스킬(GA)"이 나갈지 1:1로 지정합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TMap<EInputAction, TSubclassOf<class UGameplayAbility>> GrantedAbilities;

	// 3. 콤보 및 애니메이션 데이터 (GA 내부에서 꺼내 쓸 페이로드)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TArray<class UAnimMontage*> AttackComboMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	class UAnimMontage* EquipMontage;
	
};