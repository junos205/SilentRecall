// Fill out your copyright notice in the Description page of Project Settings.

#include "SRAN_RangedTrace.h"
#include "Character/SRInventoryComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Camera/CameraComponent.h"
#include "Character/SRPlayerCharacter.h" 
#include "Weapon/SRWeaponInstance.h"

#define ECC_DAMAGEABLE ECC_GameTraceChannel4

USRAN_RangedTrace::USRAN_RangedTrace()
{
    FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
}

void USRAN_RangedTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    AActor* OwnerActor = MeshComp->GetOwner();
    if (!OwnerActor) return;

    // 1. [이중 발사 방지] 1P 메쉬에서 불린 노티파이만 쏘게 막습니다.
    ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(OwnerActor);
    if (PlayerChar && MeshComp != PlayerChar->Get1PMesh())
    {
        return; 
    }

    // 2. 카메라 찾기 (조준선 출발점)
    UCameraComponent* CameraComp = OwnerActor->FindComponentByClass<UCameraComponent>();
    if (!CameraComp) return;

    FVector CameraStart = CameraComp->GetComponentLocation();
    FVector CameraForward = CameraComp->GetForwardVector();

    // 3. 가방 뒤져서 무기 스탯(탄착군, 투사체 여부) 및 액터(무시용) 가져오기
    float SpreadAngle = 0.0f; 
    bool bIsProjectile = false; 

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    USRInventoryComponent* InvComp = OwnerActor->FindComponentByClass<USRInventoryComponent>();
    if (InvComp)
    {
        // 3-1. 트레이스에서 무시할 껍데기(AActor) 추가
        if (InvComp->GetCurrentActiveWeaponActor())
        {
            ActorsToIgnore.Add(InvComp->GetCurrentActiveWeaponActor());
        }

        // 3-2. 알맹이(UObject)에서 스탯(탄퍼짐, 투사체 여부) 꺼내오기
        USRWeaponInstance* WeaponInst = InvComp->GetCurrentActiveWeaponInstance();
        if (WeaponInst && WeaponInst->WeaponData)
        {
            SpreadAngle = WeaponInst->WeaponData->BaseSpreadAngle;
            bIsProjectile = WeaponInst->WeaponData->bIsProjectile; 
        }
    }

    // 4. 탄착군 적용된 최종 궤적 계산
    FVector SpreadDirection = FMath::VRandCone(CameraForward, FMath::DegreesToRadians(SpreadAngle));
    FVector TraceEnd = CameraStart + (SpreadDirection * AttackRange);

    // ==========================================================
    // 5. 발사 로직 분기 (히트스캔 vs 투사체)
    // ==========================================================
    FHitResult HitResult;
    bool bShouldSendEvent = false;

    if (bIsProjectile)
    {
        // [투사체 모드] 레이저 트레이스를 쏘지 않고, 방향(시작/끝) 정보만 껍데기에 담아 GA로 넘깁니다.
        HitResult.TraceStart = CameraStart; 
        HitResult.TraceEnd = TraceEnd;
        bShouldSendEvent = true; 
    }
    else
    {
        // [히트스캔 모드] 진짜 레이저를 쏴서 맞은 놈을 판별합니다.
        bool bHit = UKismetSystemLibrary::LineTraceSingle(
            OwnerActor->GetWorld(),
            CameraStart,
            TraceEnd,
            UEngineTypes::ConvertToTraceType(ECC_DAMAGEABLE), // 👈 여기 적용 완료!
            false, // bTraceComplex
            ActorsToIgnore,
            EDrawDebugTrace::ForDuration, // 디버그 선 보기
            HitResult,
            true, FLinearColor::Red, FLinearColor::Green, 2.0f
         );

        if (bHit && HitResult.GetActor())
        {
            bShouldSendEvent = true;
        }
        
        if (bHit)
        {
            // HitResult에서 맞은 '컴포넌트'를 가져와서, 물리를 시뮬레이션 중인지 확인합니다.
            UPrimitiveComponent* HitComp = HitResult.GetComponent();
            if (HitComp && HitComp->IsSimulatingPhysics())
            {
                // 총알이 날아간 방향 계산
                FVector ForceDirection = (HitResult.TraceEnd - HitResult.TraceStart).GetSafeNormal();

                USRWeaponInstance* WeaponInst = InvComp->GetCurrentActiveWeaponInstance();
            
                // 데이터 애셋에서 가져온 힘(기본값 50000)을 곱해서 타격 지점에 물리력(Impulse)을 가합니다!
                if (WeaponInst)
                {
                    float AppliedForce = (WeaponInst) ? WeaponInst->WeaponData->ImpactForce : 5000.0f;
                    HitComp->AddImpulseAtLocation(ForceDirection * AppliedForce, HitResult.ImpactPoint);
                }
            }
        }
    }

    // ==========================================================
    // 6. 맞은 대상(또는 투사체 발사 정보)이 있다면 GA로 무전 발송!
    // ==========================================================
    if (bShouldSendEvent)
    {
       FGameplayEventData Payload;
       Payload.Instigator = OwnerActor;
       Payload.Target = HitResult.GetActor(); // 히트스캔이면 맞은 놈, 투사체면 nullptr
        
       // 타격 위치 또는 투사체가 날아갈 궤적 정보를 페이로드에 꾹꾹 담기
       Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);

       // "ASC 매니저님! Event.Ranged.Fire 발송합니다!!"
       UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, FireEventTag, Payload);
    }
}