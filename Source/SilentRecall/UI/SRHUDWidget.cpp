#include "UI/SRHUDWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AttributeSet/SRDefaultAttributeSet.h"

void USRHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    APawn* OwningPawn = GetOwningPlayerPawn();
    if (!OwningPawn) return;

    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        ASC->GetGameplayAttributeValueChangeDelegate(USRDefaultAttributeSet::GetHealthAttribute())
            .AddUObject(this, &USRHUDWidget::HandleHealthChanged);
        ASC->GetGameplayAttributeValueChangeDelegate(USRDefaultAttributeSet::GetMaxHealthAttribute())
            .AddUObject(this, &USRHUDWidget::HandleMaxHealthChanged);

        ASC->GetGameplayAttributeValueChangeDelegate(USRDefaultAttributeSet::GetAPAttribute())
            .AddUObject(this, &USRHUDWidget::HandleAPChanged);
        ASC->GetGameplayAttributeValueChangeDelegate(USRDefaultAttributeSet::GetMaxAPAttribute())
            .AddUObject(this, &USRHUDWidget::HandleMaxAPChanged);

        // 최초 폰 생성/스폰 시점에는 애니메이션을 무시하고 수치만 동기화
        bBypassAnimation = true;
        RefreshInitialHUD(ASC);
        bBypassAnimation = false; 
    }
}

void USRHUDWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        float MaxHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxHealthAttribute());
        
        // 🟢 [UI C++ 감지 핵심] 이전 값과 새 값을 비교해 회복/피격을 자체 판정합니다!
        EHealthChangeType ChangeType = EHealthChangeType::None;
        if (Data.NewValue > Data.OldValue)       ChangeType = EHealthChangeType::Healing;
        else if (Data.NewValue < Data.OldValue)  ChangeType = EHealthChangeType::Damage;

        // 세이브 데이터 로드 중이거나 초기화 중이 아닐 때만 애니메이션 허용 플래그 ON
        bool bPlayAnim = !bBypassAnimation;

        OnHealthChanged(Data.NewValue, MaxHealth, ChangeType, bPlayAnim);
    }
}

void USRHUDWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        float CurrentHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());
        // Max 수치만 바뀐 경우는 순수 데미지/힐이 아니므로 None 처리
        OnHealthChanged(CurrentHealth, Data.NewValue, EHealthChangeType::None, false);
    }
}

void USRHUDWidget::HandleAPChanged(const FOnAttributeChangeData& Data)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        float MaxAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxAPAttribute());
        
        // 🟢 [UI C++ 감지 핵심] AP가 0 이하로 떨어졌다면 즉시 방전(탈진) 상태로 판정
        bool bIsExhausted = (Data.NewValue <= 0.0f);
        
        bool bPlayAnim = !bBypassAnimation;

        OnAPChanged(Data.NewValue, MaxAP, bIsExhausted, bPlayAnim);
    }
}

void USRHUDWidget::HandleMaxAPChanged(const FOnAttributeChangeData& Data)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        float CurrentAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetAPAttribute());
        bool bIsExhausted = (CurrentAP <= 0.0f);
        OnAPChanged(CurrentAP, Data.NewValue, bIsExhausted, false);
    }
}

void USRHUDWidget::RefreshInitialHUD(UAbilitySystemComponent* ASC)
{
    if (ASC)
    {
        float CurrentHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());
        float MaxHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxHealthAttribute());
        float CurrentAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetAPAttribute());
        float MaxAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxAPAttribute());

        OnHealthChanged(CurrentHealth, MaxHealth, EHealthChangeType::None, false);
        OnAPChanged(CurrentAP, MaxAP, (CurrentAP <= 0.0f), false);
    }
}