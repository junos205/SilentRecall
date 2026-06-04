// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/SRHUDWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AttributeSet/SRDefaultAttributeSet.h"

void USRHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 1. 이 위젯을 화면에 띄운 주인이 누구인지 직접 추적합니다.
    APawn* OwningPawn = GetOwningPlayerPawn();
    if (!OwningPawn) return;

    // 2. 주인의 능력을 검사하여 무전기(델리게이트)를 위젯 내부 함수에 직접 용접합니다.
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

        // 첫 가동 시 현재 캐릭터 스탯 눈금 1:1 초기 동기화
        RefreshInitialHUD(ASC);
    }
}

void USRHUDWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        float MaxHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxHealthAttribute());
        OnHealthChanged(Data.NewValue, MaxHealth);
    }
}

void USRHUDWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        float CurrentHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());
        OnHealthChanged(CurrentHealth, Data.NewValue);
    }
}

void USRHUDWidget::HandleAPChanged(const FOnAttributeChangeData& Data)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        float MaxAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxAPAttribute());
        OnAPChanged(Data.NewValue, MaxAP);
    }
}

void USRHUDWidget::HandleMaxAPChanged(const FOnAttributeChangeData& Data)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn);
    if (ASC)
    {
        float CurrentAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetAPAttribute());
        OnAPChanged(CurrentAP, Data.NewValue);
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

        OnHealthChanged(CurrentHealth, MaxHealth);
        OnAPChanged(CurrentAP, MaxAP);
    }
}