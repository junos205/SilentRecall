#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "SRCharacterData.h"
#include "SRWeaponDataAsset.generated.h"

UENUM(BlueprintType)
enum class EWeaponDamageMode : uint8
{
	Absolute,      // [절대 데미지] 캐릭터 스탯을 무시하고 무기 BaseDamage만 100% 적용 (예: 고정 데미지 수류탄, 특수 무기)
	Additive,      // [스탯 합산] 무기 BaseDamage + 캐릭터 AttackRate (예: 소형 단검류, 고정 가산 방식)
	Multiplicative // [스탯 배율 - 가장 추천] 무기 BaseDamage * (AttackRate 계수) (예: 대검, 저격소총 등 스탯 효율이 극대화되는 무기)
};

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
	float ProjectileLifespan = 2.0f; 
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Attack Type")
	bool bCanDismember = false;
	
	// ⭐️ 2. 투사체 클래스 (bIsProjectile이 true일 때만 에디터에 보이게 설정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Projectile", meta = (EditCondition = "bIsProjectile"))
	TSubclassOf<class ASRProjectile> ProjectileClass;

	// ⭐️ 3. 투사체 속도 및 무기 기본 데미지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Projectile", meta = (EditCondition = "bIsProjectile"))
	float ProjectileSpeed = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Damage")
	EWeaponDamageMode DamageMode = EWeaponDamageMode::Multiplicative; // 기본값은 배율 적용

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Damage")
	float BaseDamage = 20.0f;

	// ⭐️ [선택적 추가] 스탯 반영 효율 계수 (예: 근력 보정치 S, A, B, C 등)
	// 1.0이면 스탯 100% 반영, 0.5면 스탯 효율 절반
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Damage", meta = (EditCondition = "DamageMode != EWeaponDamageMode::Absolute"))
	float StatScalingFactor = 1.0f;
	
	// 3. 콤보 및 애니메이션 데이터 (GA 내부에서 꺼내 쓸 페이로드)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TArray<class UAnimMontage*> AttackComboMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Cost")
	float MeleeAPCost = 15.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	class UAnimMontage* EquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	class UAnimMontage* UnEquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	class UAnimMontage* ReloadMontage = nullptr;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Recoil")
	TSubclassOf<class UCameraShakeBase> FireCameraShake;
};