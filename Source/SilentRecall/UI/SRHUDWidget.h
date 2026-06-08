// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "SRHUDWidget.generated.h"

UENUM(BlueprintType)
enum class EHealthChangeType : uint8
{
    None,
    Damage,  // 데미지 입음
    Healing  // 회복됨
};

UCLASS()
class SILENTRECALL_API USRHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    // GAS 어트리뷰트 변동 C++ 내부 감시 함수
    void HandleHealthChanged(const FOnAttributeChangeData& Data);
    void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
    void HandleAPChanged(const FOnAttributeChangeData& Data);
    void HandleMaxAPChanged(const FOnAttributeChangeData& Data);
    void RefreshInitialHUD(class UAbilitySystemComponent* ASC);

public:
    // 🌟 [SRPlayerCharacter.cpp 에러 해결] 세이브 로드 시 연출 우회용 변수 및 인라인 세터 함수 복구
    bool bBypassAnimation = false;

    FORCEINLINE void SetBypassAnimation(bool bNewState) { bBypassAnimation = bNewState; }

public:
    // 🌟 [마스터 이벤트] 블루프린트(UMG) 이벤트 그래프가 최종 수신할 4개짜리 본체 함수
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|GAS")
    void OnHealthChanged(float CurrentHealth, float MaxHealth, EHealthChangeType ChangeType, bool bPlayAnimation);

    UFUNCTION(BlueprintImplementableEvent, Category = "UI|GAS")
    void OnAPChanged(float CurrentAP, float MaxAP, bool bIsExhausted, bool bPlayAnimation);

    // =======================================================================
    // 🛡️ [SRHUDControllerComponent.cpp 에러 해결] C++ 하위 호환성 전용 오버로드 함수
    // =======================================================================
    // 구형 컴포넌트가 인자 2개로 호출하면 이 인라인 함수가 가로채서 4개짜리 마스터 이벤트로 안전하게 배달합니다.
    FORCEINLINE void OnHealthChanged(float CurrentHealth, float MaxHealth)
    {
        OnHealthChanged(CurrentHealth, MaxHealth, EHealthChangeType::None, false);
    }

    FORCEINLINE void OnAPChanged(float CurrentAP, float MaxAP)
    {
        OnAPChanged(CurrentAP, MaxAP, (CurrentAP <= 0.0f), false);
    }
    // =======================================================================

    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Death")
    void PlayDeathFadeOut();
};