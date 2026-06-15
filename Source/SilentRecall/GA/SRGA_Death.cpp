#include "SRGA_Death.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Character/SRInventoryComponent.h"
#include "NiagaraFunctionLibrary.h" 
#include "Game/SRGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Character/SRPlayerCharacter.h" 
#include "UI/SRHUDWidget.h"
#include "Character/SRBaseCharacter.h" // 🔊 조율을 위한 캐릭터 베이스 인클루드

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

    FVector DeathLocation = Victim->GetActorLocation();
    FRotator DeathRotation = Victim->GetActorRotation();

    // =======================================================================
    // 🔊 [오디오 레이어링 스택 정산 구역] - 여러 소리를 한 번에 중첩 재생
    // =======================================================================
    
    // [레이어 1] Death GA 자체에 등록된 시각적 폭발(VFX)과 동기화되는 순수 폭발음 재생
    // (만약 USRGA_Death 헤더에 USoundBase* DeathExplosionSound 가 있다면 여기서 같이 터트리기 좋습니다)
    if (DeathExplosionVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DeathExplosionVFX, DeathLocation, DeathRotation);
    }

    // 캐릭터 베이스 클래스로 안전하게 진입하여 유닛 내부 사운드 추출
    if (ASRBaseCharacter* CharacterBase = Cast<ASRBaseCharacter>(Victim))
    {
        // [레이어 2] 적 목소리로 사망하는 소리 (배열 중 무작위 1개 원샷 재생)
        if (USoundBase* DeathVoice = CharacterBase->GetRandomDeathVoice())
        {
            UGameplayStatics::PlaySoundAtLocation(GetWorld(), DeathVoice, DeathLocation);
        }

        // [레이어 3] 신체 파괴 및 살점 파열 효과음 (필요 시 동시에 중첩 재생)
        if (USoundBase* BodyImpactSound = CharacterBase->GetBodyImpactDeathSound())
        {
            UGameplayStatics::PlaySoundAtLocation(GetWorld(), BodyImpactSound, DeathLocation);
        }
    }
    // =======================================================================

    // 1. 캐릭터 컴포넌트 정리
    ACharacter* VictimChar = Cast<ACharacter>(Victim);
    if (VictimChar)
    {
        VictimChar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        VictimChar->GetCharacterMovement()->DisableMovement();

        // 🎥 [플레이어 전용 사망 연출 (슬로우 + 페이드아웃)]
        if (VictimChar->IsPlayerControlled())
        {
            UE_LOG(LogTemp, Warning, TEXT("[DeathGA] 플레이어 사망 감지 -> 슬로우 모션 및 페이드 아웃 가동"));
            UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.15f);

            if (APlayerController* PC = Cast<APlayerController>(VictimChar->GetController()))
            {
                if (PC->PlayerCameraManager)
                {
                    PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, 0.5f, FLinearColor::Black, false, true);
                }
            }

            if (ASRPlayerCharacter* SRChar = Cast<ASRPlayerCharacter>(VictimChar))
            {
                if (USRHUDWidget* HUDWidget = Cast<USRHUDWidget>(SRChar->GetMainHUDWidget()))
                {
                    HUDWidget->PlayDeathFadeOut();
                }
            }

            // 🛡️ TWeakObjectPtr 크래시 방어선 타이머 설정
            TWeakObjectPtr<ACharacter> WeakVictim(VictimChar);
            TWeakObjectPtr<UWorld> WeakWorld(GetWorld());

            FTimerHandle RespawnTimerHandle;
            FTimerDelegate RespawnDelegate;
            
            RespawnDelegate.BindLambda([WeakVictim, WeakWorld]()
            {
                if (!WeakVictim.IsValid() || !WeakWorld.IsValid()) return;
                UGameplayStatics::SetGlobalTimeDilation(WeakWorld.Get(), 1.0f);

                if (ASRGameMode* GM = Cast<ASRGameMode>(UGameplayStatics::GetGameMode(WeakWorld.Get())))
                {
                    GM->OnPlayerCharacterDeath(WeakVictim.Get());
                    UE_LOG(LogTemp, Warning, TEXT("[DeathGA] 페이드아웃 완료 -> 게임 모드 수동 부활 시스템 집행 가동"));
                }
            });

            GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, 0.5f, false);
        }
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
            ImpactRotation = HitResult->ImpactNormal.Rotation(); 
            SeveredBoneName = HitResult->BoneName;
            
            FVector ShotDir = (HitResult->TraceEnd - HitResult->TraceStart).GetSafeNormal();
            ImpulseDir = (ShotDir * 0.8f + FVector(0, 0, 0.5f)).GetSafeNormal();
        }
    }

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

        if (BloodNiagaraVFX)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BloodNiagaraVFX, ImpactLocation - FVector(0.0f, 0.0f, 130.0f), ImpactRotation);
        }
    }
}

void USRGA_Death::OnMontageCompleted() {}