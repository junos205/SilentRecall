#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> TP_AnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> FP_AnimLayerClass;
	
	// 1. 기본 무기 정보
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	FName WeaponName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	EWeaponSlot WeaponSlotType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	FGameplayTag WeaponTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	TSubclassOf<class AActor> WeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	class UTexture2D* WeaponIcon;

	// ⭐️ 2. 핵심 해결책: TArray 대신 TMap 사용!
	// "어떤 키(EInputAction)"를 누르면 "어떤 스킬(GA)"이 나갈지 1:1로 지정합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TMap<EInputAction, TSubclassOf<class UGameplayAbility>> GrantedAbilities;

	// ⭐️ 1. 공격 방식 스위치 (히트스캔 vs 투사체)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Attack Type")
	bool bIsProjectile = false; 

	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Attack Type")
	bool bCanDismember = false;
	
	// ⭐️ 2. 투사체 클래스 (bIsProjectile이 true일 때만 에디터에 보이게 설정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Projectile", meta = (EditCondition = "bIsProjectile"))
	TSubclassOf<class ASRProjectile> ProjectileClass;

	// ⭐️ 3. 투사체 속도 및 무기 기본 데미지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Projectile", meta = (EditCondition = "bIsProjectile"))
	float ProjectileSpeed = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Damage")
	float BaseDamage = 20.0f;
	
	// 3. 콤보 및 애니메이션 데이터 (GA 내부에서 꺼내 쓸 페이로드)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TArray<class UAnimMontage*> AttackComboMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	class UAnimMontage* EquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	class UAnimMontage* UnEquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Physics")
	float ImpactForce = 5000.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats")
	bool bIsAutomatic = true; // true: 연사(꾹 누르기), false: 단발(광클)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats")
	float FireRate = 0.1f; // 발사 간격 (0.1초면 초당 10발)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats")
	int32 MaxAmmoInMag = 30; // 탄창 최대 총알 수

	// ⭐️ 5. 탄착군 (Spread)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Spread")
	float BaseSpreadAngle = 1.5f; // 기본 탄퍼짐 각도 (작을수록 정확함)

	// ⭐️ 6. 반동 (Recoil)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Recoil")
	float MinRecoilPitch = 0.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Recoil")
	float MaxRecoilPitch = 1.2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Recoil")
	float MinRecoilYaw = -0.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Recoil")
	float MaxRecoilYaw = 0.5f;
};