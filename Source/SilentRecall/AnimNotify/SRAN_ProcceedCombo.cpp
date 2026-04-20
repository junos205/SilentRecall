// Fill out your copyright notice in the Description page of Project Settings.


#include "SRAN_ProcceedCombo.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Interface/SRCharacterInterface.h"

USRAN_ProceedCombo::USRAN_ProceedCombo()
{
	// 기본값으로 콤보 체크 태그를 세팅해 둡니다.
	ComboCheckTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.CheckCombo"));
}

void USRAN_ProceedCombo::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	// ==========================================================
	// ⭐️ 캐스팅 지옥 탈출! 인터페이스로 깔끔하게 처리
	// ==========================================================
	if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(OwnerActor))
	{
		USkeletalMeshComponent* FirstPersonMesh = CharInterface->Get1PMesh();

		// 1. FirstPersonMesh가 존재한다면 (즉, 플레이어라면)
		// 2. 현재 노티파이를 부른 메쉬(MeshComp)가 1인칭 메쉬가 아닐 때만 무시!
		if (FirstPersonMesh != nullptr && MeshComp != FirstPersonMesh)
		{
			return; 
		}
	}
	// (참고: 만약 1P 메쉬가 없는 적 캐릭터라면 FirstPersonMesh가 nullptr이 되므로,
	// 위 if문을 무사히 통과하여 3인칭 메쉬가 정상적으로 노티파이를 쏘게 됩니다!)

	// ----------------------------------------------------------
	// 정상 통과 시 무전(Event) 발송 로직
	FGameplayEventData Payload;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, ComboCheckTag, Payload);
}