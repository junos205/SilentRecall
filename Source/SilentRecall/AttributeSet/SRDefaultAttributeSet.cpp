// Fill out your copyright notice in the Description page of Project Settings.


#include "SRDefaultAttributeSet.h"

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
	MaxHealth(200.f)
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
			if (Data.Target.HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT(""))))
			{
				Data.EvaluatedData.Magnitude = 0.0f;
				return false;
			}
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
			const FString TargetDisplayName = TargetActor->GetName(); 
			UE_LOG(LogTemp, Warning, TEXT("[AttributeSet] Target DisplayName = %s, Damage try to apply on AttributeSet = %f "),*TargetDisplayName, LocalDamage);
			float NewHealth = FMath::Clamp(GetHealth() - LocalDamage, 0.0f, GetMaxHealth());
			SetHealth(NewHealth);
			
			if (TargetActor)
			{
				FGameplayEventData Payload;
				Payload.Instigator = SourceActor;
				Payload.Target = TargetActor;
				Payload.EventMagnitude = LocalDamage;
				
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
					TargetActor, 
					FGameplayTag::RequestGameplayTag(FName("Character.Event.HitReact")), 
					Payload
				);
			}
		}
    }

	if ((GetHealth() <= 0.0f) && !bOutOfHealth)
	{
		AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
		AActor* TargetActor = Data.Target.GetAvatarActor();
		
		UE_LOG(LogTemp, Warning, TEXT("Out of Health"));
		Data.Target.AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("")));
		if (TargetActor) OnOutOfHealth.Broadcast(TargetActor);
	}

	bOutOfHealth = (GetHealth() <= MinimumHealth);
}
