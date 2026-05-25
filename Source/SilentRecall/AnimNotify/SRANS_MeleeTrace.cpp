// Fill out your copyright notice in the Description page of Project Settings.


#include "SRANS_MeleeTrace.h"
#include "Character/SRInventoryComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/SRPlayerCharacter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Weapon/SRWeaponInstance.h"

#define ECC_DAMAGEABLE ECC_GameTraceChannel4

USRANS_MeleeTrace::USRANS_MeleeTrace()
{
	HitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Melee.Hit"));
}

void USRANS_MeleeTrace::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	AActor* OwnerActor = MeshComp->GetOwner();
    if (!OwnerActor) return;

    // 2. 가방(Inventory)을 뒤져서 지금 손에 들고 있는 무기 액터를 찾아냅니다.
    USRInventoryComponent* InvComp = OwnerActor->FindComponentByClass<USRInventoryComponent>();
    if (!InvComp || InvComp->GetCurrentActiveSlot() == EWeaponSlot::None) return;

    // 무기 액터가 있고, 그 안에 스켈레탈 메쉬(WeaponMesh)가 있는지 확인!
    AActor* ActiveWeapon = InvComp->GetCurrentActiveWeaponActor();
    if (!ActiveWeapon) return;

    USkeletalMeshComponent* WeaponMesh = ActiveWeapon->FindComponentByClass<USkeletalMeshComponent>();
    if (!WeaponMesh) return;

    // 3. 무기 메쉬에서 Base와 Tip 소켓의 현재 위치를 가져옵니다.
    FVector StartLoc = WeaponMesh->GetSocketLocation(BaseSocketName);
    FVector EndLoc = WeaponMesh->GetSocketLocation(TipSocketName);

    // 4. 스피어 트레이스 발사! (무기 채널이나 폰 채널로 쏘는 것을 권장합니다)
    TArray<FHitResult> HitResults;
    const TArray<AActor*> ActorsToIgnore = { OwnerActor, ActiveWeapon }; // 나 자신과 내 무기는 무시!
    
    // 디버그 라인을 보려면 EDrawDebugTrace::ForDuration을 켜세요. (확인용으로 매우 좋습니다)
	UKismetSystemLibrary::SphereTraceMulti(
		OwnerActor->GetWorld(),
		StartLoc, EndLoc, TraceRadius,
		UEngineTypes::ConvertToTraceType(ECC_DAMAGEABLE), // 👈 여기 적용 완료!
		false, 
		ActorsToIgnore,
		EDrawDebugTrace::None, 
		HitResults, 
		true, FLinearColor::Red, FLinearColor::Green, 1.0f
	);



	float MeleeImpactForce = 50000.0f;
	USRWeaponInstance* WeaponInst = InvComp->GetCurrentActiveWeaponInstance();
	if (WeaponInst && WeaponInst->WeaponData)
	{
		MeleeImpactForce = WeaponInst->WeaponData->ImpactForce;
	}

	// 5. 맞은 녀석들을 검사합니다.
	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		UPrimitiveComponent* HitComp = Hit.GetComponent();

		// ⭐️ 살아있는 액터이고, 블랙리스트(이미 맞은 녀석)에 없다면?!
		if (HitActor && !AlreadyHitActors.Contains(HitActor))
		{
			// "너는 이번 스윙에 확실히 맞았어!" -> 블랙리스트 등록
			AlreadyHitActors.Add(HitActor);

			// ==========================================================
			// 💥 1. 물리 객체 밀어내기 (단 1회만 묵직하게 퍽!)
			// ==========================================================
			if (HitComp && HitComp->IsSimulatingPhysics())
			{
				// 칼이 이동한 방향을 구해서 힘을 줍니다.
				FVector ForceDirection = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();
				HitComp->AddImpulseAtLocation(ForceDirection * MeleeImpactForce, Hit.ImpactPoint);
			}

			// ==========================================================
			// 🩸 2. 생명체 데미지 무전 발송 (단 1회만!)
			// ==========================================================
			FGameplayEventData Payload;
			Payload.Instigator = OwnerActor; 
			Payload.Target = HitActor;
			Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);

			// "ASC 매니저님! Event.Melee.Hit 발송합니다!!"
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, HitEventTag, Payload);
		}
	}
}

void USRANS_MeleeTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                    const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ASRPlayerCharacter* PlayerCharacter = Cast<ASRPlayerCharacter>(MeshComp->GetOwner());

	AlreadyHitActors.Empty();
}

void USRANS_MeleeTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
