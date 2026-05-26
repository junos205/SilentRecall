// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "SRDefaultAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOutOfHealthDelegate, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTakeDamageDelegate, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelChangeDelegate);

/**
 * 
 */
UCLASS()
class SILENTRECALL_API USRDefaultAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USRDefaultAttributeSet();

	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, Level)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, MaxAP)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, AP)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, MaxXP)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, XP)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, Mana)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, MaxMana)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, AttackRate)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, MaxAttackRate)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, Speed)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, MaxSpeed)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, Damage)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, Health)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, MaxHealth)
	ATTRIBUTE_ACCESSORS(USRDefaultAttributeSet, APRegenRate)
    

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	mutable FOutOfHealthDelegate OnOutOfHealth;
	mutable FOnTakeDamageDelegate OnTakeDamage;
	mutable FOnLevelChangeDelegate OnLevelChange;

public:
	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Level;
	
	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxAP;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData AP;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxXP;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData XP;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Mana;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxMana;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData AttackRate;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxAttackRate;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Speed;

	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attack, meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Health, meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Health, meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxHealth;
	
	UPROPERTY(BlueprintReadWrite, Category="Stat", meta=(AllowPrivateAccess=true))
	FGameplayAttributeData APRegenRate;
	
	bool bOutOfHealth = false;
};
