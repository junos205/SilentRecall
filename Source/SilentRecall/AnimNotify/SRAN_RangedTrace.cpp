// Fill out your copyright notice in the Description page of Project Settings.

#include "SRAN_RangedTrace.h"
#include "Character/SRInventoryComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Camera/CameraComponent.h"
#include "Character/SRPlayerCharacter.h" 
#include "Weapon/SRWeaponInstance.h"
#include "AIController.h" 
#include "Character/SRBaseCharacter.h" // ⭐️ 2차 신분증 검사(캐스팅)를 위해 헤더 추가!

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

    FVector TraceStart = OwnerActor->GetActorLocation();
    FVector TraceForward = OwnerActor->GetActorForwardVector();

    // 1. 플레이어와 AI의 트레이스 시작점/방향 분리
    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(OwnerActor))
    {
        // [플레이어 로직] 1P 메쉬 방어 및 카메라 기준 사격
        if (MeshComp != PlayerChar->Get1PMesh()) return; 

        UCameraComponent* CameraComp = OwnerActor->FindComponentByClass<UCameraComponent>();
        if (!CameraComp) return;

        TraceStart = CameraComp->GetComponentLocation();
        TraceForward = CameraComp->GetForwardVector();
    }
    else
    {
        // [AI 로직] 카메라가 없으므로 눈높이에서 타겟을 향해 사격
        TraceStart = OwnerActor->GetActorLocation() + FVector(0, 0, 60.0f); // 눈높이 보정
        
        AAIController* AIC = Cast<AAIController>(OwnerActor->GetInstigatorController());
        if (AIC && AIC->GetFocusActor())
        {
            TraceForward = (AIC->GetFocusActor()->GetActorLocation() - TraceStart).GetSafeNormal();
        }
    }

    // 2. 가방 뒤져서 무기 스탯(탄착군, 투사체 여부) 및 액터(무시용) 가져오기
    float SpreadAngle = 0.0f; 
    bool bIsProjectile = false; 

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    USRInventoryComponent* InvComp = OwnerActor->FindComponentByClass<USRInventoryComponent>();
    if (InvComp)
    {
        if (InvComp->GetCurrentActiveWeaponActor())
        {
            ActorsToIgnore.Add(InvComp->GetCurrentActiveWeaponActor());
        }

        USRWeaponInstance* WeaponInst = InvComp->GetCurrentActiveWeaponInstance();
        if (WeaponInst && WeaponInst->WeaponData)
        {
            SpreadAngle = WeaponInst->WeaponData->BaseSpreadAngle;
            bIsProjectile = WeaponInst->WeaponData->bIsProjectile; 
        }
    }

    // 3. 탄착군 적용된 최종 궤적 계산
    FVector SpreadDirection = FMath::VRandCone(TraceForward, FMath::DegreesToRadians(SpreadAngle));
    FVector TraceEnd = TraceStart + (SpreadDirection * AttackRange);

    // ==========================================================
    // 4. 발사 로직 분기 (히트스캔 vs 투사체)
    // ==========================================================
    FHitResult HitResult;
    bool bShouldSendEvent = false;

    if (bIsProjectile)
    {
        // [투사체 모드]
        HitResult.TraceStart = TraceStart; 
        HitResult.TraceEnd = TraceEnd;
        bShouldSendEvent = true; 
    }
    else
    {
        // [히트스캔 모드] 
        // ⭐️ 1차 거름망: ECC_Visibility 채널로 변경! (벽 관통 절대 금지)
        bool bHit = UKismetSystemLibrary::LineTraceSingle(
            OwnerActor->GetWorld(),
            TraceStart,
            TraceEnd,
            UEngineTypes::ConvertToTraceType(ECC_Visibility), 
            false,
            ActorsToIgnore,
            EDrawDebugTrace::ForDuration, // 디버그 선 그리기
            HitResult,
            true, FLinearColor::Red, FLinearColor::Green, 2.0f
         );

        if (bHit && HitResult.GetActor())
        {
            // ⭐️ 2차 거름망: 맞은 놈이 진짜 피를 흘리는 '캐릭터'일 때만 어빌리티에 보고!
            ASRBaseCharacter* HitCharacter = Cast<ASRBaseCharacter>(HitResult.GetActor());
            if (HitCharacter)
            {
                bShouldSendEvent = true;
            }
            
            UPrimitiveComponent* HitComp = HitResult.GetComponent();
            if (HitComp && HitComp->IsSimulatingPhysics())
            {
                FVector ForceDirection = (HitResult.TraceEnd - HitResult.TraceStart).GetSafeNormal();
                USRWeaponInstance* WeaponInst = InvComp ? InvComp->GetCurrentActiveWeaponInstance() : nullptr;
            
                float AppliedForce = (WeaponInst && WeaponInst->WeaponData) ? WeaponInst->WeaponData->ImpactForce : 5000.0f;
                HitComp->AddImpulseAtLocation(ForceDirection * AppliedForce, HitResult.ImpactPoint);
            }
        }
    }

    // ==========================================================
    // 5. GA로 무전 발송!
    // ==========================================================
    if (bShouldSendEvent)
    {
       FGameplayEventData Payload;
       Payload.Instigator = OwnerActor;
       Payload.Target = HitResult.GetActor(); 
        
       Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);

       UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, FireEventTag, Payload);
    }
}