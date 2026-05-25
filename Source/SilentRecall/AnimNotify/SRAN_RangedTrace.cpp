// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNotify/SRAN_RangedTrace.h"
#include "Character/SRInventoryComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Camera/CameraComponent.h"
#include "Character/SRPlayerCharacter.h" 
#include "Weapon/SRWeaponInstance.h"
#include "AIController.h" 
#include "Character/SRBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"

USRAN_RangedTrace::USRAN_RangedTrace()
{
    FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
}

void USRAN_RangedTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    AActor* OwnerActor = MeshComp->GetOwner();
    if (!OwnerActor) return;

    USRInventoryComponent* InvComp = OwnerActor->FindComponentByClass<USRInventoryComponent>();

    // ==========================================================
    // ⭐️ 1. 진짜 목적지(True Target Point) 찾기 (카메라 시점 기준)
    // ==========================================================
    FVector TrueTargetPoint = FVector::ZeroVector;

    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(OwnerActor))
    {
        // ⭐️ [해결 1] GAS 플레이 태스크가 3인칭 메쉬로 이벤트를 발생시키므로, 
        // 1인칭 메쉬가 아니라고 수천 번 튕겨내던 치명적인 얼리 리턴(return;) 필터 라인을 완전 삭제합니다!

        if (UCameraComponent* CameraComp = OwnerActor->FindComponentByClass<UCameraComponent>())
        {
            FVector CamLoc = CameraComp->GetComponentLocation();
            FVector CamForward = CameraComp->GetForwardVector();
            FVector CamEnd = CamLoc + (CamForward * AttackRange);

            TArray<AActor*> CamIgnoreActors;
            CamIgnoreActors.Add(OwnerActor);
            if (InvComp && InvComp->GetCurrentActiveWeaponActor())
            {
                CamIgnoreActors.Add(InvComp->GetCurrentActiveWeaponActor());
            }

            FHitResult CamHit;
            bool bCamHit = UKismetSystemLibrary::LineTraceSingle(
                OwnerActor->GetWorld(), CamLoc, CamEnd,
                UEngineTypes::ConvertToTraceType(ECC_Visibility), 
                false, CamIgnoreActors, EDrawDebugTrace::None, CamHit, true
            );

            TrueTargetPoint = bCamHit ? CamHit.ImpactPoint : CamEnd;
        }
    }
    else
    {
        if (AAIController* AIC = Cast<AAIController>(OwnerActor->GetInstigatorController()))
        {
            if (AActor* FocusActor = AIC->GetFocusActor())
            {
                TrueTargetPoint = FocusActor->GetActorLocation();
            }
            else
            {
                TrueTargetPoint = OwnerActor->GetActorLocation() + (OwnerActor->GetActorForwardVector() * AttackRange);
            }
        }
    }

    // ==========================================================
    // ⭐️ 2. 사격 시작점(Muzzle) 찾기
    // ==========================================================
    FVector TraceStart = OwnerActor->GetActorLocation(); 
    
    if (InvComp && InvComp->GetCurrentActiveWeaponActor())
    {
        AActor* WeaponActor = InvComp->GetCurrentActiveWeaponActor();

        if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(OwnerActor))
        {
            USkeletalMeshComponent* FP_Mesh = PlayerChar->Get1PMesh();
            if (FP_Mesh)
            {
                if (USkeletalMeshComponent* FP_WeaponMesh = PlayerChar->GetWeaponMeshForComponent(FP_Mesh))
                {
                    if (FP_WeaponMesh->DoesSocketExist(FName("Muzzle")))
                    {
                        TraceStart = FP_WeaponMesh->GetSocketLocation(FName("Muzzle"));
                    }
                }
            }
        }
        else 
        {
            TArray<USkeletalMeshComponent*> SkelMeshes;
            WeaponActor->GetComponents<USkeletalMeshComponent>(SkelMeshes);
            for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
            {
                if (SkelMesh && SkelMesh->DoesSocketExist(FName("Muzzle")))
                {
                    TraceStart = SkelMesh->GetSocketLocation(FName("Muzzle"));
                    break;
                }
            }
        }
    }

    // ==========================================================
    // ⭐️ 3. 방향 보정 및 탄착군(Spread) 계산
    // ==========================================================
    FVector TraceForward = (TrueTargetPoint - TraceStart).GetSafeNormal();

    float SpreadAngle = 0.0f; 
    bool bIsProjectile = false; 

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

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

    FVector SpreadDirection = FMath::VRandCone(TraceForward, FMath::DegreesToRadians(SpreadAngle));
    FVector TraceEnd = TraceStart + (SpreadDirection * AttackRange);

    // ==========================================================
    // 4. 발사 로직 분기 (히트스캔 vs 투사체)
    // ==========================================================
    FHitResult HitResult;
    bool bShouldSendEvent = false;

    if (bIsProjectile)
    {
        HitResult.TraceStart = TraceStart; 
        HitResult.TraceEnd = TraceEnd;
        bShouldSendEvent = true; 
    }
    else
    {
        bool bHit = UKismetSystemLibrary::LineTraceSingle(
            OwnerActor->GetWorld(), TraceStart, TraceEnd,
            UEngineTypes::ConvertToTraceType(ECC_Visibility), 
            false, ActorsToIgnore, EDrawDebugTrace::None, HitResult, true
         );

        // ⭐️ [개선 점] 이제 히트스캔 사격 중 벽이나 사물을 맞추더라도, 
        // 궤적 정보 패키징 무전(SendEvent)은 무조건 전달하도록 개방하여 불발 현상을 완전 소멸시킵니다.
        bShouldSendEvent = true; 

        if (bHit && HitResult.GetActor())
        {
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
