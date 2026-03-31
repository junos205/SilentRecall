// Fill out your copyright notice in the Description page of Project Settings.


#include "SRANS_MeleeTrace.h"
#include "Character/SRInventoryComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/SRPlayerCharacter.h"
#include "Kismet/KismetSystemLibrary.h"

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
        UEngineTypes::ConvertToTraceType(ECC_Pawn), // 적이 Pawn 채널이라고 가정
        false, 
        ActorsToIgnore,
        	EDrawDebugTrace::ForDuration, // 눈으로 궤적을 보고 싶다면 EDrawDebugTrace::ForDuration 으로 변경!
        HitResults, 
        true, FLinearColor::Red, FLinearColor::Green, 1.0f
    );



    // 5. 맞은 녀석들을 검사합니다.
    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();

        // 살아있는 액터이고, 블랙리스트(이미 맞은 녀석)에 없다면?!
        if (HitActor && !AlreadyHitActors.Contains(HitActor))
        {
            // ⭐️ "너는 이번 공격에 맞았어!" 블랙리스트에 추가
            AlreadyHitActors.Add(HitActor);

            // ⭐️ 대망의 무전기 발송 (Gameplay Event)
            // 맞은 적(HitActor)과 때린 사람(OwnerActor) 정보를 페이로드에 꾹꾹 담습니다.
            FGameplayEventData Payload;
            Payload.Instigator = OwnerActor; 
            Payload.Target = HitActor;       
            // 필요하다면 Payload.TargetData에 HitResult 전체를 포장해서 넣을 수도 있습니다.

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
