// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "SRCharacterData.h"
#include "SRWeaponDataAsset.generated.h"

UENUM(BlueprintType)
enum class EWeaponDamageMode : uint8
{
    Absolute,      
    Additive,      
    Multiplicative 
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

    // ⭐️ 무기 장착 시 "캐릭터"에게 링크해줄 애니메이션 레이어
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

    // =======================================================================
    // 🌟 [추가] 실제 무기 외형 3D 모델링 및 무기 자체 애니메이션 데이터 지정
    // =======================================================================
    /** 캐릭터 손에 들릴 순수한 무기 스켈레탈 메시 원본 에셋 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info|Visual")
    TObjectPtr<USkeletalMesh> WeaponMesh;

    /** 총기 자체의 기믹(사격 시 노리쇠 후퇴, 장전 시 탄창 분리 등)을 구동할 무기 전용 애니메이션 블루프린트 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info|Visual")
    TSubclassOf<UAnimInstance> WeaponMeshAnimClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info|Visual")
    FVector WeaponScale = FVector(1.0f, 1.0f, 1.0f);
    
    // =======================================================================

    // 2. GAS 능력 부여 데이터
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
    TMap<EInputAction, TSubclassOf<class UGameplayAbility>> GrantedAbilities;

    // 3. 공격 방식 스위치 (히트스캔 vs 투사체)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Attack Type")
    bool bIsProjectile = false; 

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Attack Type")
    float ProjectileLifespan = 2.0f; 
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Attack Type")
    bool bCanDismember = false;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Projectile", meta = (EditCondition = "bIsProjectile"))
    TSubclassOf<class ASRProjectile> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Projectile", meta = (EditCondition = "bIsProjectile"))
    float ProjectileSpeed = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Damage")
    EWeaponDamageMode DamageMode = EWeaponDamageMode::Multiplicative; 

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Damage")
    float BaseDamage = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Damage", meta = (EditCondition = "DamageMode != EWeaponDamageMode::Absolute"))
    float StatScalingFactor = 1.0f;
    
    // 4. 캐릭터 연출용 콤보 및 애니메이션 데이터 몽타주
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
    bool bIsAutomatic = true; 

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats")
    float FireRate = 0.1f; 

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats")
    int32 MaxAmmoInMag = 30;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|ADS")
    float AimFOV = 65.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|ADS")
    FVector AimOffsetTuning = FVector::ZeroVector;

    // 5. 탄착군 (Spread)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Spread")
    float BaseSpreadAngle = 1.5f; 

    // 6. 반동 (Recoil)
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Stats|Audio")
    TArray<class USoundBase*> ImpactSounds;
};