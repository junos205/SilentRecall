// Fill out your copyright notice in the Description page of Project Settings.

#include "SRANS_MeleeTrace.h"
#include "Character/SRInventoryComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/SRPlayerCharacter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Weapon/SRWeaponInstance.h"

#define ECC_DAMAGEABLE ECC_GameTraceChannel4

// =======================================================================
// 🛡️ [버그 분쇄기] 애니메이션 노티파이 공유(CDO) 버그를 원천 차단하기 위한 정적 레지스트리
// 공격을 시전한 주인 액터(Owner)별로 독립된 블랙리스트(TSet)를 매핑하여 다중 스윙 크로스토크를 방지합니다.
// =======================================================================
static TMap<AActor*, TSet<AActor*>> PerActorMeleeHitRegistry;
// =======================================================================

USRANS_MeleeTrace::USRANS_MeleeTrace()
{
    HitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Melee.Hit"));
}

void USRANS_MeleeTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

    // 공격을 시작한 주인의 블랙리스트만 콕 집어서 청소합니다. (다른 캐릭터의 스윙에 간섭하지 않음)
    if (MeshComp && MeshComp->GetOwner())
    {
        PerActorMeleeHitRegistry.Remove(MeshComp->GetOwner());
    }

    // 기존 구형 변수 청소 안전장치 유지
    AlreadyHitActors.Empty();
}

void USRANS_MeleeTrace::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

    AActor* OwnerActor = MeshComp->GetOwner();
    if (!OwnerActor) return;

    USRInventoryComponent* InvComp = OwnerActor->FindComponentByClass<USRInventoryComponent>();
    if (!InvComp || InvComp->GetCurrentActiveSlot() == EWeaponSlot::None) return;

    AActor* ActiveWeapon = InvComp->GetCurrentActiveWeaponActor();
    if (!ActiveWeapon) return;

    USkeletalMeshComponent* WeaponMesh = ActiveWeapon->FindComponentByClass<USkeletalMeshComponent>();
    if (!WeaponMesh) return;

    FVector StartLoc = WeaponMesh->GetSocketLocation(BaseSocketName);
    FVector EndLoc = WeaponMesh->GetSocketLocation(TipSocketName);

    TArray<FHitResult> HitResults;
    const TArray<AActor*> ActorsToIgnore = { OwnerActor, ActiveWeapon }; 
    
    UKismetSystemLibrary::SphereTraceMulti(
       OwnerActor->GetWorld(),
       StartLoc, EndLoc, TraceRadius,
       UEngineTypes::ConvertToTraceType(ECC_DAMAGEABLE), 
       false, 
       ActorsToIgnore,
       EDrawDebugTrace::ForOneFrame, 
       HitResults, 
       true, FLinearColor::Red, FLinearColor::Green, 1.0f
    );

    float MeleeImpactForce = 50000.0f;
    USRWeaponInstance* WeaponInst = InvComp->GetCurrentActiveWeaponInstance();
    if (WeaponInst && WeaponInst->WeaponData)
    {
       MeleeImpactForce = WeaponInst->WeaponData->ImpactForce;
    }

    // 🟢 이 액터 전용 블랙리스트 주머니 획득 및 참조 연결
    TSet<AActor*>& MyHitList = PerActorMeleeHitRegistry.FindOrAdd(OwnerActor);

    for (const FHitResult& Hit : HitResults)
    {
       AActor* HitActor = Hit.GetActor();
       UPrimitiveComponent* HitComp = Hit.GetComponent();

       // 🛡️ 내 고유 블랙리스트 장부에 등록되지 않은 새로운 적일 때만 타격 처리 집행!
       if (HitActor && !MyHitList.Contains(HitActor))
       {
          MyHitList.Add(HitActor); // 내 장부에 등록

          // 💥 1. 물리 객체 밀어내기
          if (HitComp && HitComp->IsSimulatingPhysics())
          {
             FVector ForceDirection = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();
             HitComp->AddImpulseAtLocation(ForceDirection * MeleeImpactForce, Hit.ImpactPoint);
          }

          // 🩸 2. 생명체 데미지 무전 발송
          FGameplayEventData Payload;
          Payload.Instigator = OwnerActor; 
          Payload.Target = HitActor;
          Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);

          UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, HitEventTag, Payload);
       }
    }
}

void USRANS_MeleeTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);

    // 스윙 렌더링이 완전히 종료되었으므로 장부에서 깔끔하게 메모리를 해제합니다.
    if (MeshComp && MeshComp->GetOwner())
    {
        PerActorMeleeHitRegistry.Remove(MeshComp->GetOwner());
    }
}