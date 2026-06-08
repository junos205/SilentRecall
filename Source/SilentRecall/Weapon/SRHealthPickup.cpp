// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/SRHealthPickup.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/SRInventoryComponent.h"
#include "GameplayEffectTypes.h"

ASRHealthPickup::ASRHealthPickup()
{
	// 기본 회복량 설정
	HealAmount = 5.0f;
}

void ASRHealthPickup::OnPickedUp(USRInventoryComponent* InventoryComp)
{
	if (!InventoryComp) return;

	AActor* OverlappingActor = InventoryComp->GetOwner();
	if (!OverlappingActor) return;

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OverlappingActor);
	if (ASC && HealGameplayEffectClass)
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddInstigator(this, this); 
        
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(HealGameplayEffectClass, 1.0f, EffectContext);
		if (SpecHandle.IsValid())
		{
			FGameplayTag HealTag = FGameplayTag::RequestGameplayTag(FName("Data.HealAmount"));
			SpecHandle.Data->SetSetByCallerMagnitude(HealTag, HealAmount);

			// 🔥 [핵심 수정] Instant 에펙트는 핸들을 남기지 않으므로 리턴값을 검사하지 않고 바로 주입합니다.
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            
			UE_LOG(LogTemp, Log, TEXT("[Pickup] ➕ %s 가 체력 키트를 먹어 %f 만큼 회복되었습니다!"), *OverlappingActor->GetName(), HealAmount);
            
			// 주입에 성공했으니 미련 없이 즉시 아이템을 파괴합니다!
			Destroy();
		}
	}
}