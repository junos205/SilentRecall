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

    /** 무작위 비명소리를 꺼내주는 Getter 함수 */
    UFUNCTION(BlueprintCallable, Category = "Character|Audio")
    class USoundBase* GetRandomDeathVoice() const
    {
        if (VoiceDeathSounds.Num() > 0)
        {
            int32 RandomIndex = FMath::RandRange(0, VoiceDeathSounds.Num() - 1);
            return VoiceDeathSounds[RandomIndex];
        }
        return nullptr;
    }

    /** 공통 신체 파괴 효과음을 꺼내주는 Getter 함수 */
    FORCEINLINE class USoundBase* GetBodyImpactDeathSound() const { return BodyImpactDeathSound; }

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Animation|HitReact")
    UAnimMontage* HitFrontMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Animation|HitReact")
    UAnimMontage* HitBackMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Animation|HitReact")
    UAnimMontage* HitLeftMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Animation|HitReact")
    UAnimMontage* HitRightMontage;

    /** 사망 시 재생할 목소리(비명) 배열 - 이 중 하나가 무작위로 나옵니다 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Audio", meta = (AllowPrivateAccess = "true"))
    TArray<class USoundBase*> VoiceDeathSounds;

    /** 사망 시 목소리와 함께 '동시에' 터질 환경/신체 파괴 효과음 (예: 뼈 부러지는 소리, 살 파열음 등) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Audio", meta = (AllowPrivateAccess = "true"))
    class USoundBase* BodyImpactDeathSound;
public:
    // ==========================================================
    // 🎭 인터페이스 덮어쓰기 (1P 무시, 3P 전용 처리)
    // ==========================================================
    UFUNCTION()
    virtual void HandleWeaponChanged(class USRWeaponDataAsset* NewWeaponData);

    virtual void ApplyWeaponAnimLayer() override {}
    virtual void AttachWeaponToHolster(AActor* WeaponActor, FName HolsterSocketName);
    virtual void AttachWeaponToHands(AActor* WeaponActor, FName EquipSocketName);
    virtual void PlayWeaponMontage(class UAnimMontage* MontageToPlay, bool bFirstPersonOnly = false);
    virtual class UAnimMontage* GetHitReactMontage(EHitDirection Direction) override;
    virtual class USkeletalMeshComponent* Get1PMesh() const {return nullptr;};
    // ⭐️ 베이스는 1P가 없으니 무조건 nullptr 반환
    virtual void ApplyRecoil(float PitchAmount, float YawAmount) override {};
    
};