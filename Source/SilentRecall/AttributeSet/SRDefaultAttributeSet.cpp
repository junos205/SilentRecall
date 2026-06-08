// Fill out your copyright notice in the Description page of Project Settings.


#include "SRDefaultAttributeSet.h"
#include "Perception/AISense_Damage.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectExtension.h"

USRDefaultAttributeSet::USRDefaultAttributeSet() :
Level(1.f),
    MaxAP(100.f),
    AP(100.f),
    MaxXP(100.f),
    XP(0.f),
    Mana(50.f),
    MaxMana(100.f),
    AttackRate(20.f),
    MaxAttackRate(100.f),
    Speed(0.0f),
    MaxSpeed(100.0f),
    Damage(0.0f),
    Health(200.f),
    MaxHealth(200.f),
    APRegenRate(10.0f)
{
    InitHealth(GetMaxHealth());
}


void USRDefaultAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetDamageAttribute())
    {
       NewValue = NewValue < 0.0f ? 0.0f : NewValue;
    }

    if (Attribute == GetHealthAttribute())
    {
       // 체력이 실제로 바뀌기 전에 MaxHealth를 넘지 못하도록 값을 미리 깎아둡니다.
       NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
}

bool USRDefaultAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
    if (!Super::PreGameplayEffectExecute(Data))
    {
       return false;
    }

    if (Data.EvaluatedData.Attribute == GetDamageAttribute())
    {
       if (Data.EvaluatedData.Magnitude > 0.0f)
       {
          
       }
    }
    return true;
}

void USRDefaultAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    
    float MinimumHealth = 0.0f;

    const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
    UAbilitySystemComponent* SourceASC = EffectContext.GetOriginalInstigatorAbilitySystemComponent();
       
    if (Data.EvaluatedData.Attribute == GetXPAttribute())
    {
       UE_LOG(LogTemp, Display, TEXT("Current XP = %f"), GetXP());
       
       float CurrentXP = GetXP();
       float CurrentMaxXP = GetMaxXP();

       while (CurrentXP >= CurrentMaxXP)
       {
          CurrentXP -= CurrentMaxXP;
          SetXP(CurrentXP);

          float NextLevel = GetLevel() + 1.0f;
          SetLevel(NextLevel);

          float NewMaxXP = CurrentMaxXP * 1.2f;
          SetMaxXP(NewMaxXP);

          SetHealth(GetMaxHealth());
          UE_LOG(LogTemp, Display, TEXT("Current XP = %f"), GetXP());

          OnLevelChange.Broadcast();
       }
    }
    
    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
       SetHealth(FMath::Clamp(GetHealth(), MinimumHealth, GetMaxHealth()));
    }
    
    if (Data.EvaluatedData.Attribute == GetSpeedAttribute())
    {
       SetSpeed(FMath::Clamp(GetSpeed(), 0.0f, GetMaxSpeed()));
    }
    
    if (Data.EvaluatedData.Attribute == GetDamageAttribute())
    {
       float LocalDamage = GetDamage();
       SetDamage(0.0f); 

       AActor* SourceActor = (SourceASC ? SourceASC->GetAvatarActor() : nullptr);
       AActor* TargetActor = Data.Target.GetAvatarActor();
       
       if (LocalDamage > 0.0f)
       {
          UE_LOG(LogTemp, Warning, TEXT("[Attribute] %s took %f damage from %s"), *TargetActor->GetName(), LocalDamage, SourceActor ? *SourceActor->GetName() : TEXT("Unknown"));
          const FString TargetDisplayName = TargetActor->GetName(); 
          float NewHealth = FMath::Clamp(GetHealth() - LocalDamage, 0.0f, GetMaxHealth());
          SetHealth(NewHealth);
          
          if (TargetActor)
          {
             FGameplayEventData Payload;
             Payload.Instigator = SourceActor;
             Payload.Target = TargetActor;
             Payload.EventMagnitude = LocalDamage;
             
             FVector HitLocation = TargetActor->GetActorLocation();
             
             if (EffectContext.GetHitResult())
             {
                Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(*EffectContext.GetHitResult());
             }
             
             UAISense_Damage::ReportDamageEvent(
              TargetActor->GetWorld(), 
              TargetActor,             
              SourceActor,             
              LocalDamage,             
              SourceActor ? SourceActor->GetActorLocation() : FVector::ZeroVector, 
              HitLocation              
             );
             
             if (NewHealth <= 0.0f)
             {
                // 1. 사망 무전 (Event.Death) 발송 (기존 유지)
                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, FGameplayTag::RequestGameplayTag(FName("Character.Event.Death")), Payload);

                // =======================================================================
                // 🌟 [교정 완료] 빠져있던 사망 방송국 델리게이트 가동 무전선 연결!
                // =======================================================================
                OnOutOfHealth.Broadcast(TargetActor);
                // =======================================================================
             }
             else
             {
                // 2. 일반 피격 무전 (Event.HitReact) 발송
                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, FGameplayTag::RequestGameplayTag(FName("Character.Event.HitReact")), Payload);
             }
          }
       }
    }
    
    if (Data.EvaluatedData.Attribute == GetAPAttribute())
    {
       SetAP(FMath::Clamp(GetAP(), 0.0f, GetMaxAP()));

       UAbilitySystemComponent* TargetASC = &Data.Target;
       FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Exhausted"));

       if (GetAP() <= 0.0f && !TargetASC->HasMatchingGameplayTag(ExhaustedTag))
       {
          TargetASC->AddLooseGameplayTag(ExhaustedTag);
          UE_LOG(LogTemp, Warning, TEXT("[Attribute] 기력 방전! 탈진 상태 돌입."));
       }
       else if (GetAP() >= (GetMaxAP() * 0.1f) && TargetASC->HasMatchingGameplayTag(ExhaustedTag))
       {
          TargetASC->RemoveLooseGameplayTag(ExhaustedTag);
          UE_LOG(LogTemp, Warning, TEXT("[Attribute] 기력 10%% 회복! 탈진 상태 해제."));
       }
    }

    bOutOfHealth = (GetHealth() <= MinimumHealth);
}