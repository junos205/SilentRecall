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
#include "Perception/AISense_Hearing.h" // 🌟 AI 청각 리포터 추가

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

    // =======================================================================
    // 🔊 [공통 노이즈 리포트] 1. 발사지 소음 (Gunshot)
    // 투사체든 히트스캔이든 격발하는 순간 방방곡곡 소리를 퍼트립니다. (반경 30미터)
    // =======================================================================
    UAISense_Hearing::ReportNoiseEvent(
        OwnerActor->GetWorld(),
        TraceStart,
        1.0f,
        OwnerActor,
        3000.0f,
        TEXT("Gunshot")
    );
    // =======================================================================

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
        
        // 💡 투사체 모드일 때는 날아가서 꽂힐 때 ASRProjectile 내에서 탄착 소음(BulletImpact)을 발생시킵니다.
    }
    else
    {
        bool bHit = UKismetSystemLibrary::LineTraceSingle(
            OwnerActor->GetWorld(), TraceStart, TraceEnd,
            UEngineTypes::ConvertToTraceType(ECC_Visibility), 
            false, ActorsToIgnore, EDrawDebugTrace::None, HitResult, true
         );

        bShouldSendEvent = true; 

        // =======================================================================
        // 🔊 [히트스캔 전용 노이즈 리포트] 2. 탄착지 소음 (BulletImpact)
        // 히트스캔은 즉시 연산이므로, 벽이나 적에게 충돌한 정확한 'ImpactPoint'에 소음을 발생시킵니다.
        // 벽을 맞췄을 때 팅! 하는 소리를 주변 적들이 듣고 수색하러 옵니다. (반경 15미터)
        // =======================================================================
        if (bHit)
        {
            UAISense_Hearing::ReportNoiseEvent(
                OwnerActor->GetWorld(),
                HitResult.ImpactPoint,
                1.0f,
                OwnerActor,
                1500.0f,
                TEXT("BulletImpact")
            );
        }
        // =======================================================================

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