// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interface/InteractableInterface.h"
#include "SRAmmoPickup.generated.h"

UCLASS()
class SILENTRECALL_API ASRAmmoPickup : public AActor, public IInteractableInterface
{
	GENERATED_BODY()
    
public:    
	ASRAmmoPickup();

	// ⭐️ 인터페이스 상호작용 함수 구현
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	// 탄약 상자의 외형 (보통 탄약은 애니메이션이 없으므로 StaticMesh를 씁니다)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* AmmoMesh;

	// 어떤 종류의 탄약인가? (예: Weapon.Ammo.Rifle, Weapon.Ammo.Pistol)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo Data")
	FGameplayTag AmmoTypeTag;

	// 주웠을 때 몇 발을 줄 것인가?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo Data")
	int32 AmmoAmount = 30;
};