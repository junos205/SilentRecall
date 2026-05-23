// Fill out your copyright notice in the Description page of Project Settings.

#include "SRAN_RangedTrace.h"
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
        if (MeshComp != PlayerChar->Get1PMesh()) return; 

        if (UCameraComponent* CameraComp = OwnerActor->FindComponentByClass<UCameraComponent>())
        {
            FVector CamLoc = CameraComp->GetComponentLocation();
            FVector CamForward = CameraComp->GetForwardVector();
            FVector CamEnd = CamLoc + (CamForward * AttackRange);

            // 무시할 액터 세팅 (나 자신과 내 무기는 맞추면 안 됨)
            TArray<AActor*> CamIgnoreActors;
            CamIgnoreActors.Add(OwnerActor);
            if (InvComp && InvComp->GetCurrentActiveWeaponActor())
            {
                CamIgnoreActors.Add(InvComp->GetCurrentActiveWeaponActor());
            }

            // 카메라에서 크로스헤어 방향으로 가상의 레이저를 쏴서 진짜 목적지 좌표를 얻어옵니다.
            FHitResult CamHit;
            bool bCamHit = UKismetSystemLibrary::LineTraceSingle(
                OwnerActor->GetWorld(), CamLoc, CamEnd,
                UEngineTypes::ConvertToTraceType(ECC_Visibility), 
                false, CamIgnoreActors, EDrawDebugTrace::None, CamHit, true
            );

            // 벽에 맞았다면 그곳이 타겟, 허공을 쐈다면 사거리 끝 지점이 타겟
            TrueTargetPoint = bCamHit ? CamHit.ImpactPoint : CamEnd;
        }
    }
    else
    {
        // [AI 로직] AI는 카메라가 없으므로 Focus하고 있는 적의 위치를 목적지로 삼음
        if (AAIController* AIC = Cast<AAIController>(OwnerActor->GetInstigatorController()))
        {
            if (AActor* FocusActor = AIC->GetFocusActor())
            {
                // 타겟의 몸통 중앙을 노림
                TrueTargetPoint = FocusActor->GetActorLocation();
            }
            else
            {
                // 타겟이 없으면 그냥 앞을 보고 쏨
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
    // 총구에서 진짜 목적지를 향하는 완벽한 방향 벡터를 계산합니다!
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

    // 탄착군을 적용하고 최종 목적지를 도출합니다.
    FVector SpreadDirection = FMath::VRandCone(TraceForward, FMath::DegreesToRadians(SpreadAngle));
    FVector TraceEnd = TraceStart + (SpreadDirection * AttackRange);

    // ==========================================================
    // 4. 발사 로직 분기 (히트스캔 vs 투사체)
    // ==========================================================
    FHitResult HitResult;
    bool bShouldSendEvent = false;

    if (bIsProjectile)
    {
        // [투사체 모드] 궤적의 시작과 끝점만 GA로 넘겨주면, GA가 이 방향대로 총알을 소환하여 날려보냅니다.
        HitResult.TraceStart = TraceStart; 
        HitResult.TraceEnd = TraceEnd;
        bShouldSendEvent = true; 
    }
    else
    {
        // [히트스캔 모드] 보정된 궤적으로 실제 물리 트레이스를 발사합니다.
        bool bHit = UKismetSystemLibrary::LineTraceSingle(
            OwnerActor->GetWorld(),
            TraceStart,
            TraceEnd,
            UEngineTypes::ConvertToTraceType(ECC_Visibility), 
            false,
            ActorsToIgnore,
            EDrawDebugTrace::ForDuration, // 확인을 위해 디버그 선을 그립니다
            HitResult,
            true, FLinearColor::Red, FLinearColor::Green, 2.0f
         );

        if (bHit && HitResult.GetActor())
        {
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