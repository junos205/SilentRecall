// Fill out your copyright notice in the Description page of Project Settings.

#include "SRGA_RangedAttack.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Weapon/SRWeaponInstance.h"
#include "Character/SRInventoryComponent.h"
#include "Character/SRPlayerCharacter.h"
#include "Interface/SRCharacterInterface.h"
#include "Weapon/SRProjectile.h"
#include "AIController.h"
#include "Character/SRBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"

USRGA_RangedAttack::USRGA_RangedAttack()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    FGameplayTagContainer TempTags;
    TempTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Attack.Ranged"))); 
    SetAssetTags(TempTags);

    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));

    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Ranged")));
}

void USRGA_RangedAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 📡 사격 애니메이션 내부 노티파이가 보내올 무전을 상시 수신 대기합니다. (능력 종료 전까지 영구 지속)
    FGameplayTag FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
    UAbilityTask_WaitGameplayEvent* WaitFireTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FireEventTag);
    WaitFireTask->EventReceived.AddDynamic(this, &USRGA_RangedAttack::OnFireEventReceived);
    WaitFireTask->ReadyForActivation();

    // 첫 번째 격발 파이어 시전
    FireShot();
}

void USRGA_RangedAttack::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);
}

void USRGA_RangedAttack::FireShot()
{
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    ASRBaseCharacter* AvatarChar = Cast<ASRBaseCharacter>(AvatarActor);
    if (!AvatarChar) 
    {
        UE_LOG(LogTemp, Error, TEXT("[RangedAttack] FireShot Failed: Avatar is not ASRBaseCharacter."));
        EndAbilityDelegate();
        return;
    }

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    bool bIsAI = (AIC != nullptr);

    if (bIsAI)
    {
        FGameplayTag FireCommandTag = FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Combat.Fire"));
        if (!GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FireCommandTag))
        {
            UE_LOG(LogTemp, Log, TEXT("[RangedAttack] AI Fire Command Tag removed. Stopping Fire Loop."));
            EndAbilityDelegate();
            return;
        }

        AActor* TargetActor = AIC->GetFocusActor();
        if (TargetActor)
        {
            FVector DirectionToTarget = (TargetActor->GetActorLocation() - AvatarChar->GetActorLocation()).GetSafeNormal();
            FVector MyForward = AvatarChar->GetActorForwardVector();
            float DotResult = FVector::DotProduct(DirectionToTarget, MyForward);
            
            if (DotResult < 0.8f)
            {
                UE_LOG(LogTemp, Warning, TEXT("[RangedAttack] AI is turning... Retrying in 0.05s."));
                UAbilityTask_WaitDelay* TurnWaitTask = UAbilityTask_WaitDelay::WaitDelay(this, 0.05f);
                TurnWaitTask->OnFinish.AddDynamic(this, &USRGA_RangedAttack::FireShot);
                TurnWaitTask->ReadyForActivation();
                return; 
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[RangedAttack] AI has no Focus Actor! Canceling Attack."));
            EndAbilityDelegate();
            return;
        }
    }
    
    USRWeaponInstance* WeaponInst = Cast<USRWeaponInstance>(GetCurrentSourceObject());
    if (!WeaponInst || !WeaponInst->WeaponData || !WeaponInst->HasAmmo())
    {
        UE_LOG(LogTemp, Error, TEXT("[RangedAttack] FireShot Failed: Invalid Weapon or Out of Ammo."));
        EndAbilityDelegate();
        return;
    }

    // 장탄수 소비
    WeaponInst->ConsumeAmmo();

    // 🎬 무기 몽타주 연출 활성화
    UAnimMontage* FireMontage = WeaponInst->WeaponData->AttackComboMontages.Num() > 0 ? WeaponInst->WeaponData->AttackComboMontages[0] : nullptr;
    if (FireMontage)
    {
        if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(AvatarChar))
        {
            // 1인칭 및 3인칭 레이어 전방위 시각 재생 가동
            CharInterface->PlayWeaponMontage(FireMontage, true);
        }
        
        // ❌ [해결 3] 연사 시 Montage Task 프록시를 무한대로 중첩 생성하여 메모리를 파괴하던 구형 로직 제거!
        // 총기 반동 모션 연출은 위의 인터페이스 단독 호출만으로도 완벽하게 드로잉됩니다.
    }

    // 반동 및 카메라 쉐이크 절차 수행
    float RecoilPitch = FMath::RandRange(WeaponInst->WeaponData->MinRecoilPitch, WeaponInst->WeaponData->MaxRecoilPitch);
    float RecoilYaw = FMath::RandRange(WeaponInst->WeaponData->MinRecoilYaw, WeaponInst->WeaponData->MaxRecoilYaw);
    if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(AvatarChar))
    {
        CharInterface->ApplyRecoil(RecoilPitch, RecoilYaw);
    }
    
    if (!bIsAI && WeaponInst->WeaponData->FireCameraShake)
    {
        if (APlayerController* PC = Cast<APlayerController>(AvatarChar->GetController()))
        {
            if (PC->PlayerCameraManager)
            {
                PC->PlayerCameraManager->StartCameraShake(WeaponInst->WeaponData->FireCameraShake, 1.0f);
            }
        }
    }

    // 연사 자동 루프 검사 여부 확인
    bool bShouldLoop = false;
    if (WeaponInst->WeaponData->bIsAutomatic)
    {
        bShouldLoop = (GetCurrentAbilitySpec()->InputPressed || bIsAI);
    }

    // ⭐️ [해결 2] 단발이든 자동이든 무조건 FireRate 딜레이 태스크 안전장치를 걸어둠으로써, 
    // 애니메이션 몽타주 중간에 심겨있는 노티파이 무전이 도착하기 전에 능력이 공중 폭파되는 현상을 완벽 차단합니다!
    UAbilityTask_WaitDelay* FireRateDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, WeaponInst->WeaponData->FireRate);
    
    if (bShouldLoop)
    {
        FireRateDelayTask->OnFinish.AddDynamic(this, &USRGA_RangedAttack::FireShot);
    }
    else
    {
        FireRateDelayTask->OnFinish.AddDynamic(this, &USRGA_RangedAttack::EndAbilityDelegate);
    }
    
    FireRateDelayTask->ReadyForActivation();
}

void USRGA_RangedAttack::OnFireEventReceived(FGameplayEventData Payload)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    USRWeaponInstance* WeaponInst = Cast<USRWeaponInstance>(GetCurrentSourceObject());

    if (!Avatar || !WeaponInst || !WeaponInst->WeaponData || !DamageEffectClass) return;

    FVector MuzzleLocation = Avatar->GetActorLocation(); 
    bool bFoundSocket = false;
    
    // 1. 플레이어 캐릭터 총구 소켓 탐색
    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(Avatar))
    {
        USkeletalMeshComponent* FP_Mesh = PlayerChar->Get1PMesh();
        if (FP_Mesh)
        {
            if (USkeletalMeshComponent* FP_WeaponMesh = PlayerChar->GetWeaponMeshForComponent(FP_Mesh))
            {
                if (FP_WeaponMesh->DoesSocketExist(FName("Muzzle")))
                {
                    MuzzleLocation = FP_WeaponMesh->GetSocketLocation(FName("Muzzle"));
                    bFoundSocket = true;
                }
            }
            if (!bFoundSocket && FP_Mesh->DoesSocketExist(FName("Muzzle")))
            {
                MuzzleLocation = FP_Mesh->GetSocketLocation(FName("Muzzle"));
                bFoundSocket = true;
            }
        }
    }
    // 2. 적 AI 무기 액터 총구 소켓 탐색
    else 
    {
        USRInventoryComponent* InvComp = Avatar->FindComponentByClass<USRInventoryComponent>();
        if (InvComp && InvComp->GetCurrentActiveWeaponActor())
        {
            AActor* WeaponActor = InvComp->GetCurrentActiveWeaponActor();
            TArray<USkeletalMeshComponent*> SkelMeshes;
            WeaponActor->GetComponents<USkeletalMeshComponent>(SkelMeshes);
            
            for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
            {
                if (SkelMesh && SkelMesh->DoesSocketExist(FName("Muzzle")))
                {
                    MuzzleLocation = SkelMesh->GetSocketLocation(FName("Muzzle"));
                    bFoundSocket = true;
                    break;
                }
            }
        }
    }

    if (!bFoundSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("[RangedAttack] 🔴 'Muzzle' socket NOT found for Avatar: %s"), *Avatar->GetName());
    }

    FVector TargetPoint = FVector::ZeroVector;
    const FHitResult* HitResult = Payload.TargetData.IsValid(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;

    if (HitResult)
    {
        TargetPoint = HitResult->bBlockingHit ? HitResult->ImpactPoint : HitResult->TraceEnd;
    }
    else
    {
        AAIController* AIC = Cast<AAIController>(Avatar->GetInstigatorController());
        if (AIC && AIC->GetFocusActor())
        {
            TargetPoint = AIC->GetFocusActor()->GetActorLocation();
        }
        else
        {
            return;
        }
    }

    // ==========================================================
    // 🚀 1. 투사체(Projectile) 발사 모드 분기
    // ==========================================================
    if (WeaponInst->WeaponData->bIsProjectile && WeaponInst->WeaponData->ProjectileClass)
    {
        FRotator SpawnRotation = (TargetPoint - MuzzleLocation).Rotation();
        FTransform SpawnTransform(SpawnRotation, MuzzleLocation);

        ASRProjectile* SpawnedProj = GetWorld()->SpawnActorDeferred<ASRProjectile>(
            WeaponInst->WeaponData->ProjectileClass, SpawnTransform, Avatar, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (SpawnedProj)
        {
            SpawnedProj->InstigatorActor = Avatar;
            SpawnedProj->DamageAmount = WeaponInst->WeaponData->BaseDamage;
            SpawnedProj->SetImpactForce(WeaponInst->WeaponData->ImpactForce);
            SpawnedProj->DamageEffectClass = DamageEffectClass; 
            SpawnedProj->SourceWeaponData = WeaponInst->WeaponData; // 데이터 애셋 청구서 전달

            SpawnedProj->FinishSpawning(SpawnTransform);
            
            FVector ShootDir = (TargetPoint - MuzzleLocation).GetSafeNormal();
            SpawnedProj->SetSpeed(WeaponInst->WeaponData->ProjectileSpeed, ShootDir); 
        }
        return; // 투사체 생성 완료 시 즉시 탈출 (하단 즉시 데미지 중복 방지)
    }

    // ==========================================================
    // 💥 2. 히트스캔(HitScan) 즉시 데미지 적용 모드 분기
    // ==========================================================
    AActor* OriginalShooter = Avatar;
    AActor* TargetActor = const_cast<AActor*>(Payload.Target.Get());
    if (TargetActor)
    {
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
        if (TargetASC)
        {
            FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
            if (HitResult) ContextHandle.AddHitResult(*HitResult);

            // ⭐️ [해결] 어빌리티 단축코드 야매 패링 타겟 변조식을 걷어냅니다!
            // 패링 데미지 면제 및 피해 반사 판정은 우리가 정밀 설계해 둔 SRDamageExecCalc가 100% 처리합니다.
            ContextHandle.AddInstigator(OriginalShooter, OriginalShooter);

            FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);
            if (SpecHandle.IsValid())
            {
                GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
            }
        }
    }
}

void USRGA_RangedAttack::EndAbilityDelegate()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_RangedAttack::OnMontageCompleted()
{
    // 현재는 자동소총 연사 최적화를 위해 몽타주 태스크를 사용하지 않으므로 
    // 컴파일러 링크 에러 방지용으로 빈 몸통만 남겨둡니다.
}