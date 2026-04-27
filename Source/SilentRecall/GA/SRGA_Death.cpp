#include "SRGA_Death.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Character/SRInventoryComponent.h"
#include "NiagaraFunctionLibrary.h" // ⭐️ 나이아가라 함수 라이브러리 포함

USRGA_Death::USRGA_Death()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead")));
}

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

    // 2. 가해자의 무기 데이터 확인 (절단 가능 여부)
    bool bShouldDismember = false;
    if (TriggerEventData && TriggerEventData->Instigator.Get())
    {
        AActor* Attacker = const_cast<AActor*>(TriggerEventData->Instigator.Get());
        if (Attacker)
        {
            USRInventoryComponent* InvComp = Attacker->FindComponentByClass<USRInventoryComponent>();
            if (InvComp && InvComp->GetCurrentActiveWeaponActor())
            {
                USRWeaponInstance* WeaponInst = Cast<USRWeaponInstance>(InvComp->GetCurrentActiveWeaponInstance());
                if (WeaponInst && WeaponInst->WeaponData && WeaponInst->WeaponData->bCanDismember)
                {
                    bShouldDismember = true;
                }
            }
        }
    }

    // 3. 피격 정보 분석 (임펄스 방향, 이펙트 위치/회전, 뼈 이름)
    FVector ImpactLocation = Victim->GetActorLocation();
    FRotator ImpactRotation = FRotator::ZeroRotator;
    FVector ImpulseDir = -Victim->GetActorForwardVector();
    FName SeveredBoneName = NAME_None;

    if (TriggerEventData && TriggerEventData->TargetData.IsValid(0))
    {
        const FHitResult* HitResult = TriggerEventData->TargetData.Get(0)->GetHitResult();
        if (HitResult) 
        {
            ImpactLocation = HitResult->ImpactPoint;
            ImpactRotation = HitResult->ImpactNormal.Rotation(); // ⭐️ 타격 표면의 수직 방향 (피 튀는 방향)
            SeveredBoneName = HitResult->BoneName;
            
            FVector ShotDir = (HitResult->TraceEnd - HitResult->TraceStart).GetSafeNormal();
            ImpulseDir = (ShotDir * 0.8f + FVector(0, 0, 0.5f)).GetSafeNormal();
        }
    }

    // 캡슐에 맞았거나 뼈를 못 찾은 경우 기본값(상체) 지정
    if (SeveredBoneName == NAME_None || SeveredBoneName == FName("pelvis") || SeveredBoneName == FName("root"))
    {
        SeveredBoneName = FName("spine_02"); 
    }

    // 4. 래그돌 활성화 및 넉백(Impulse)
    Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    Mesh->SetCollisionProfileName(FName("Ragdoll"));
    Mesh->SetSimulatePhysics(true);
    Mesh->AddImpulse(ImpulseDir * 1500.f, SeveredBoneName, true); 

    // 5. 절단 가능 무기일 때 사지 절단 및 나이아가라 효과!
    if (bShouldDismember)
    {
        // ⭐️ [복구됨] 유저님이 요청하신 디버그용 절단 부위 확인 로그!
        UE_LOG(LogTemp, Warning, TEXT("[Death] Should Dismember Severed Bone: %s"), *SeveredBoneName.ToString());

        Mesh->HideBoneByName(SeveredBoneName, EPhysBodyOp::PBO_Term);
        
        if (FleshPlugMesh)
        {
            UStaticMeshComponent* FleshPlug = NewObject<UStaticMeshComponent>(Victim);
            FleshPlug->SetStaticMesh(FleshPlugMesh);
            FleshPlug->RegisterComponent();
            FleshPlug->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            FleshPlug->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SeveredBoneName);
            FleshPlug->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));
        }

        // 절단 부위에서 타격 방향(Normal)으로 나이아가라 피 분수 재생!
        if (BloodNiagaraVFX)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BloodNiagaraVFX, ImpactLocation - FVector(0.0f, 0.0f, 130.0f), ImpactRotation);
        }
    }
}

void USRGA_Death::OnMontageCompleted() {}