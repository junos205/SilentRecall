// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SRHUDControllerComponent.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "Components/WidgetComponent.h"
#include "UI/SRHUDWidget.h"

USRHUDControllerComponent::USRHUDControllerComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // 최적화를 위해 틱 해제
}

void USRHUDControllerComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // 1. 캐릭터가 소유한 Widget Component를 찾아 내부 UMG 위젯 인스턴스를 확보합니다.
    UWidgetComponent* WidgetComp = Owner->FindComponentByClass<UWidgetComponent>();
    if (WidgetComp)
    {
        TargetHUDWidget = Cast<USRHUDWidget>(WidgetComp->GetUserWidgetObject());
    }

    // 2. 캐릭터의 ASC를 찾아 어트리뷰트 변경 델리게이트를 바인딩합니다.
    ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();
    if (ASC && TargetHUDWidget)
    {
        // 체력 관련 바인딩
        ASC->GetGameplayAttributeValueChangeDelegate(USRDefaultAttributeSet::GetHealthAttribute())
            .AddUObject(this, &USRHUDControllerComponent::HandleHealthChanged);
        ASC->GetGameplayAttributeValueChangeDelegate(USRDefaultAttributeSet::GetMaxHealthAttribute())
            .AddUObject(this, &USRHUDControllerComponent::HandleMaxHealthChanged);

        // AP 관련 바인딩
        ASC->GetGameplayAttributeValueChangeDelegate(USRDefaultAttributeSet::GetAPAttribute())
            .AddUObject(this, &USRHUDControllerComponent::HandleAPChanged);
        ASC->GetGameplayAttributeValueChangeDelegate(USRDefaultAttributeSet::GetMaxAPAttribute())
            .AddUObject(this, &USRHUDControllerComponent::HandleMaxAPChanged);

        // 최초 1회 현재 데이터로 UI 동기화
        RefreshInitialHUD();
    }
}

void USRHUDControllerComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    if (TargetHUDWidget && ASC)
    {
        float MaxHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxHealthAttribute());
        TargetHUDWidget->OnHealthChanged(Data.NewValue, MaxHealth);
    }
}

void USRHUDControllerComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
    if (TargetHUDWidget && ASC)
    {
        float CurrentHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());
        TargetHUDWidget->OnHealthChanged(CurrentHealth, Data.NewValue);
    }
}

void USRHUDControllerComponent::HandleAPChanged(const FOnAttributeChangeData& Data)
{
    if (TargetHUDWidget && ASC)
    {
        float MaxAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxAPAttribute());
        TargetHUDWidget->OnAPChanged(Data.NewValue, MaxAP);
    }
}

void USRHUDControllerComponent::HandleMaxAPChanged(const FOnAttributeChangeData& Data)
{
    if (TargetHUDWidget && ASC)
    {
        float CurrentAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetAPAttribute());
        TargetHUDWidget->OnAPChanged(CurrentAP, Data.NewValue);
    }
}

void USRHUDControllerComponent::RefreshInitialHUD()
{
    if (TargetHUDWidget && ASC)
    {
        float CurrentHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());
        float MaxHealth = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxHealthAttribute());
        float CurrentAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetAPAttribute());
        float MaxAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetMaxAPAttribute());

        TargetHUDWidget->OnHealthChanged(CurrentHealth, MaxHealth);
        TargetHUDWidget->OnAPChanged(CurrentAP, MaxAP);
    }
}