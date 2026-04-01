// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputAction.h"
#include "Data/SRCharacterData.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "SRBaseCharacter.generated.h"

UCLASS()
class SILENTRECALL_API ASRBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ASRBaseCharacter(const FObjectInitializer& ObjectInitializer);
	
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ASC")
	TObjectPtr<UAbilitySystemComponent> ASC;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USRDefaultAttributeSet> AttributeSet;
	
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UGameplayAbility>> DefaultAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USRInventoryComponent> InventoryComponent;
	
public:
	
	virtual void PossessedBy(AController* NewController) override;

};
