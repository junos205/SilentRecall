#include "SRGA_Death.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"

USRGA_Death::USRGA_Death()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead")));
}

#include "SRGA_Death.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"

void USRGA_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* Victim = GetAvatarActorFromActorInfo();
    if (!Victim) return;

    // 1. 캐릭터 컴포넌트 정리
    ACharacter* VictimChar = Cast<ACharacter>(Victim);
    if (VictimChar)
    {
        VictimChar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        VictimChar->GetCharacterMovement()->DisableMovement();
    }

    USkeletalMeshComponent* Mesh = Victim->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh) return;

    // 2. 절단 위치 및 뼈 찾기
    FVector PlaneCenter = Victim->GetActorLocation();
    FVector PlaneNormal = Victim->GetActorUpVector();
    if (TriggerEventData && TriggerEventData->TargetData.IsValid(0))
    {
        const FHitResult* HitResult = TriggerEventData->TargetData.Get(0)->GetHitResult();
        if (HitResult) 
        {
            PlaneCenter = HitResult->ImpactPoint;
            FVector SwingDir = (HitResult->TraceEnd - HitResult->TraceStart).GetSafeNormal();
            PlaneNormal = FVector::CrossProduct(SwingDir, Victim->GetActorForwardVector()).GetSafeNormal();
        }
    }

    if (!TriggerEventData || !TriggerEventData->TargetData.IsValid(0)) return;
    const FHitResult* HitResult = TriggerEventData->TargetData.Get(0)->GetHitResult();
    if (!HitResult) return;

    // ⭐️ [해결 1] 수학적 계산 대신 실제 타격된 본 이름을 가져옵니다.
    FName SeveredBoneName = HitResult->BoneName;
    
    // 예외 처리: 루트나 골반을 맞췄을 때 캐릭터가 통째로 사라지는 것 방지
    if (SeveredBoneName == FName("pelvis") || SeveredBoneName == FName("root"))
    {
        SeveredBoneName = FName("spine_02"); // 최소한 가슴 위쪽부터 잘리도록 유도
    }

    // ⭐️ [해결 2] 임펄스 방향을 무기 궤적(SwingDir)에 맞게 수정
    FVector ShotDir = (HitResult->TraceEnd - HitResult->TraceStart).GetSafeNormal();
    // 너무 위로만 뜨지 않게 ShotDir 비중을 높이고, 위쪽 힘(Z)은 살짝만 섞습니다.
    FVector ImpulseDir = (ShotDir * 0.8f + FVector(0, 0, 0.2f)).GetSafeNormal();

    // 3. 본 숨기기 및 물리 설정
    Mesh->HideBoneByName(SeveredBoneName, EPhysBodyOp::PBO_Term);
    
    // 4. 고기 마개 부착
    if (FleshPlugMesh)
    {
        UStaticMeshComponent* FleshPlug = NewObject<UStaticMeshComponent>(Victim);
        FleshPlug->SetStaticMesh(FleshPlugMesh);
        FleshPlug->RegisterComponent();
        FleshPlug->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        
        // 맞은 부위(Bone)의 정확한 위치와 회전값에 부착
        FleshPlug->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SeveredBoneName);
        FleshPlug->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));
    }

    // 5. 래그돌 활성화 및 임펄스 가동
    Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    Mesh->SetCollisionProfileName(FName("Ragdoll"));
    Mesh->SetSimulatePhysics(true);
    
    // ⭐️ [해결 3] 임펄스 세기를 조절하고 해당 본에 직접 힘을 가합니다.
    Mesh->AddImpulse(ImpulseDir * 10000.f);

    // 6. 효과음 및 파티클
    if (BloodSpurtVFX)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodSpurtVFX, PlaneCenter, PlaneNormal.Rotation());
    }
}

void USRGA_Death::OnMontageCompleted() {}