#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interface/SRCharacterInterface.h" // ⭐️ 인터페이스 상속 필수!
#include "SRBaseCharacter.generated.h"

UCLASS()
class SILENTRECALL_API ASRBaseCharacter : public ACharacter, public IAbilitySystemInterface, public ISRCharacterInterface
{
    GENERATED_BODY()

public:
    ASRBaseCharacter(const FObjectInitializer& ObjectInitializer);

    virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
    // ASRBaseCharacter.h 추가 사항
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
    TSubclassOf<class UAnimInstance> CurrentTPLayer;
    
    virtual void PossessedBy(AController* NewController) override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    class UAbilitySystemComponent* ASC;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    class USRDefaultAttributeSet* AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    class USRInventoryComponent* InventoryComponent;

    // ==========================================================
    // ⚔️ [핵심 추가] 블루프린트에서 등록할 기본 GA 배열 (HitReact, Death 등)
    // ==========================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
    TArray<TSubclassOf<class UGameplayAbility>> DefaultAbilities;

public:
    // ==========================================================
    // 🎭 인터페이스 덮어쓰기 (1P 무시, 3P 전용 처리)
    // ==========================================================
    UFUNCTION()
    virtual void HandleWeaponChanged(class USRWeaponDataAsset* NewWeaponData);
    
    virtual void AttachWeaponToHolster(AActor* WeaponActor, FName HolsterSocketName);
    virtual void AttachWeaponToHands(AActor* WeaponActor, FName EquipSocketName);
    virtual void PlayWeaponMontage(class UAnimMontage* MontageToPlay, bool bFirstPersonOnly = false);
    virtual class USkeletalMeshComponent* Get1PMesh() const {return nullptr;};
    // ⭐️ 베이스는 1P가 없으니 무조건 nullptr 반환
    virtual void ApplyRecoil(float PitchAmount, float YawAmount) override {};
};