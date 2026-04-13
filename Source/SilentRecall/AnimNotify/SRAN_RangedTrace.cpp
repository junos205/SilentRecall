// Fill out your copyright notice in the Description page of Project Settings.


#include "SRAN_RangedTrace.h"
#include "Character/SRInventoryComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Camera/CameraComponent.h"

USRAN_RangedTrace::USRAN_RangedTrace()
{
	FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
}

void USRAN_RangedTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	Super::Notify(MeshComp, Animation, EventReference);

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	// 1. 카메라 찾기 (조준선 중앙을 구하기 위함)
	UCameraComponent* CameraComp = OwnerActor->FindComponentByClass<UCameraComponent>();
	if (!CameraComp) return;

	FVector CameraStart = CameraComp->GetComponentLocation();
	FVector CameraForward = CameraComp->GetForwardVector();
	FVector TraceEnd = CameraStart + (CameraForward * AttackRange);

	// 2. 가방 뒤져서 현재 손에 든 무기 찾기 (나와 내 무기는 명중에서 제외!)
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerActor);

	USRInventoryComponent* InvComp = OwnerActor->FindComponentByClass<USRInventoryComponent>();
	if (InvComp && InvComp->GetCurrentActiveWeaponActor())
	{
		ActorsToIgnore.Add(InvComp->GetCurrentActiveWeaponActor());
	}

	// 3. 레이저(라인 트레이스) 발사!
	FHitResult HitResult;
	bool bHit = UKismetSystemLibrary::LineTraceSingle(
		OwnerActor->GetWorld(),
		CameraStart,
		TraceEnd,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false, // bTraceComplex (정밀 판정 필요시 true)
		ActorsToIgnore,
		EDrawDebugTrace::ForDuration, // 디버그 선 보기
		HitResult,
		true, FLinearColor::Red, FLinearColor::Green, 2.0f
	);

	// 4. 맞은 놈이 있다면 무전기 발송! (Event 발송)
	if (bHit && HitResult.GetActor())
	{
		FGameplayEventData Payload;
		Payload.Instigator = OwnerActor;
		Payload.Target = HitResult.GetActor();
        
		// ⭐️ [가장 중요] 타격 위치와 표면 정보(TargetData)를 페이로드에 꾹꾹 눌러 담습니다!
		Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, FireEventTag, Payload);
	}
}
