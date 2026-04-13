// Fill out your copyright notice in the Description page of Project Settings.

#include "SRAN_RangedTrace.h"
#include "Character/SRInventoryComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Camera/CameraComponent.h"
#include "Character/SRPlayerCharacter.h" // 1P 메쉬 필터링을 위해 필요
#include "Weapon/SRWeaponInstance.h"     // 무기 데이터(탄퍼짐)를 읽기 위해 필요

USRAN_RangedTrace::USRAN_RangedTrace()
{
    FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
}

void USRAN_RangedTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    // (기존 코드에 Super::Notify가 두 번 연달아 있던 오타 수정!)

    AActor* OwnerActor = MeshComp->GetOwner();
    if (!OwnerActor) return;

    // ⭐️ 1. [이중 발사 방지] 1P와 3P 몽타주가 동시에 틀어지므로, 1P에서 불린 노티파이만 쏘게 막습니다!
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

    // 3. 가방 뒤져서 무기 및 탄착군(Spread) 정보 가져오기
    float SpreadAngle = 0.0f; // 기본 탄퍼짐 각도 (0이면 100% 명중)
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    USRInventoryComponent* InvComp = OwnerActor->FindComponentByClass<USRInventoryComponent>();
    if (InvComp && InvComp->GetCurrentActiveWeaponActor())
    {
        ActorsToIgnore.Add(InvComp->GetCurrentActiveWeaponActor());

        // ⭐️ 현재 무기 인스턴스를 가져와서 데이터 애셋의 Spread 값을 뽑아옵니다.
        USRWeaponInstance* WeaponInst = Cast<USRWeaponInstance>(InvComp->GetCurrentActiveWeaponActor());
        if (WeaponInst && WeaponInst->WeaponData)
        {
            SpreadAngle = WeaponInst->WeaponData->BaseSpreadAngle;
        }
    }

    // ⭐️ 4. 탄착군 적용된 최종 궤적 계산 (핵심 수학!)
    // CameraForward를 중심으로 SpreadAngle만큼 벌어진 원뿔(Cone) 형태에서 랜덤한 방향을 하나 픽(Pick)합니다.
    FVector SpreadDirection = FMath::VRandCone(CameraForward, FMath::DegreesToRadians(SpreadAngle));
    FVector TraceEnd = CameraStart + (SpreadDirection * AttackRange);

    // 5. 레이저(히트스캔) 발사!
    FHitResult HitResult;
    bool bHit = UKismetSystemLibrary::LineTraceSingle(
       OwnerActor->GetWorld(),
       CameraStart,
       TraceEnd,
       UEngineTypes::ConvertToTraceType(ECC_Pawn), // 적이 Pawn 채널이라고 가정
       false, // bTraceComplex
       ActorsToIgnore,
       EDrawDebugTrace::ForDuration, // 디버그 선 보기 (빨간색/초록색 선)
       HitResult,
       true, FLinearColor::Red, FLinearColor::Green, 2.0f
    );

    // 6. 맞은 대상이 있다면 GA로 무전 발송!
    if (bHit && HitResult.GetActor())
    {
       FGameplayEventData Payload;
       Payload.Instigator = OwnerActor;
       Payload.Target = HitResult.GetActor();
        
       // 타격 위치와 표면 정보를 페이로드에 꾹꾹 담기
       Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitResult);

       // "ASC 매니저님! Event.Ranged.Fire 발송합니다!!"
       UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, FireEventTag, Payload);
    }
}