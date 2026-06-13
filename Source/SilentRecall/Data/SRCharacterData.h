#pragma once

#include "CoreMinimal.h"
#include "SRCharacterData.generated.h"

UENUM(BlueprintType)
enum class EInputAction : uint8
{
    Sprint,
    Dash,
    Slide,
    Grapple,
    Attack,
    Reload,
    CycleWeapon,
    Vault,
    Aim
};

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
    None        UMETA(DisplayName = "None"),
    Melee       UMETA(DisplayName = "Melee Weapon"),   // 근접 무기
    Ranged      UMETA(DisplayName = "Ranged Weapon"),  // 원거리 무기
    Special     UMETA(DisplayName = "Special Weapon")  // 특수 무기
};

// ⭐️ 2. 무기의 정보 구조체 (나중에 CSV 데이터 테이블로 관리하기 딱 좋습니다)
USTRUCT(BlueprintType)
struct FWeaponItemData : public FTableRowBase
{
    GENERATED_BODY()

    // 무기 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
    FName WeaponName;

    // 실제 손에 쥐어줄 3D 모델(액터) 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
    TSubclassOf<class AActor> WeaponClass;

    // UI에 띄워줄 무기 아이콘
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
    class UTexture2D* WeaponIcon;

    // 💡 GAS 연동용: 이 무기를 들었을 때 부여할 어빌리티 (예: 베기 스킬, 사격 스킬)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Data")
    TArray<TSubclassOf<class UGameplayAbility>> GrantedAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TArray<class UAnimMontage*> AttackComboMontages;

    // 만약 무기 꺼내는 모션이나 장전 모션도 필요하다면?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    class UAnimMontage* EquipMontage = nullptr; // 🌟 1. 선언문 우항에 직접 = nullptr; 주입!

    // 기본 생성자 (🌟 패키징 오류 완치를 위해 EquipMontage도 nullptr로 명시적 초기화 추가)
    FWeaponItemData() 
        : WeaponName(NAME_None)
        , WeaponClass(nullptr)
        , WeaponIcon(nullptr)
        , EquipMontage(nullptr) // ➔ 이 부분이 누락되어 쿠킹 에러가 났던 것을 수정했습니다!
    {}
};