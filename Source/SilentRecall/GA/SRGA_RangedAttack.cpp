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
#include "NiagaraFunctionLibrary.h"
#include "Character/SRBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Perception/AISense_Hearing.h"

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
        EndAbilityDelegate();
        return;
    }

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    bool bIsAI = (AIC != nullptr);

    // =======================================================================
    // 🚀 [최적화 1] AI 에임 보정 태스크 폭탄 제거
    // =======================================================================
    if (bIsAI)
    {
        FGameplayTag FireCommandTag = FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Combat.Fire"));
        if (!GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FireCommandTag))
        {
            EndAbilityDelegate();
            return;
        }

        AActor* TargetActor = AIC->GetFocusActor();
        if (TargetActor)
        {
            FVector DirectionToTarget = (TargetActor->GetActorLocation() - AvatarChar->GetActorLocation()).GetSafeNormal();
            if (FVector::DotProduct(DirectionToTarget, AvatarChar->GetActorForwardVector()) < 0.8f)
            {
                // ❌ 0.05초 Delay Task 루프를 삭제합니다.
                // AI가 각도를 못 맞췄다면 어빌리티를 즉시 종료시키세요. 
                // 어차피 비헤이비어 트리(BT)가 캐릭터를 회전시킨 뒤 다시 사격 어빌리티를 발동시키는 것이 훨씬 안정적입니다.
                EndAbilityDelegate();
                return; 
            }
        }
        else
        {
            EndAbilityDelegate();
            return;
        }
    }
    
    USRWeaponInstance* WeaponInst = Cast<USRWeaponInstance>(GetCurrentSourceObject());
    if (!WeaponInst || !WeaponInst->WeaponData || !WeaponInst->HasAmmo())
    {
        EndAbilityDelegate();
        return;
    }

    // =======================================================================
    // 🚀 [최적화 2] 장탄수 소비 및 UI 실시간 즉각 동기화 (누락분 복구)
    // =======================================================================
    WeaponInst->ConsumeAmmo();
    
    // 사격으로 총알이 깎였으니 인벤토리에 UI 화면을 즉시 새로고침하라고 찌릅니다.
    if (USRInventoryComponent* InvComp = AvatarChar->FindComponentByClass<USRInventoryComponent>())
    {
        InvComp->RefreshWeaponHUD();
    }

    // AI 소음 발생
    UAISense_Hearing::ReportNoiseEvent(
        GetWorld(), AvatarChar->GetActorLocation(), 1.0f, AvatarChar, 3000.0f, TEXT("Gunshot")
    );

    // 몽타주 및 반동 재생 (캐스팅 중복 제거로 최적화)
    if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(AvatarChar))
    {
        UAnimMontage* FireMontage = WeaponInst->WeaponData->AttackComboMontages.Num() > 0 ? WeaponInst->WeaponData->AttackComboMontages[0] : nullptr;
        if (FireMontage)
        {
            CharInterface->PlayWeaponMontage(FireMontage, true);
        }

        float RecoilPitch = FMath::RandRange(WeaponInst->WeaponData->MinRecoilPitch, WeaponInst->WeaponData->MaxRecoilPitch);
        float RecoilYaw = FMath::RandRange(WeaponInst->WeaponData->MinRecoilYaw, WeaponInst->WeaponData->MaxRecoilYaw);
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

    // =======================================================================
    // 🚀 [최적화 3] 딜레이 태스크 안전화
    // =======================================================================
    bool bShouldLoop = WeaponInst->WeaponData->bIsAutomatic && (GetCurrentAbilitySpec()->InputPressed || bIsAI);

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

    // 🚨 [방어선] 기본 데이터 검문
    if (!Avatar || !WeaponInst || !WeaponInst->WeaponData || !DamageEffectClass) return;

    // ==========================================================
    // 🎯 [치트키] AnimNotify가 이미 계산해 준 대박 데이터 장부 꺼내기
    // ==========================================================
    const FHitResult* HitResult = Payload.TargetData.IsValid(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr;
    if (!HitResult) return;

    // 🌟 복잡한 소켓 탐색 코드 싹 다 제거! AnimNotify가 찾은 총구 위치 그대로 사용
    FVector MuzzleLocation = HitResult->TraceStart; 
    
    // 🌟 이미 탄착군(Spread)과 지형 충돌이 연산 완료된 조준 최종 목적지
    FVector TargetPoint = HitResult->bBlockingHit ? HitResult->ImpactPoint : HitResult->TraceEnd;

    // ==========================================================
    // 🔥 [VFX 스폰] 위치와 순서가 완벽하게 보정된 총구 화염 및 연기 연출
    // ==========================================================
    // 총구에서 조준점을 똑바로 바라보는 정밀 회전값 계산
    FRotator MuzzleRotation = (TargetPoint - MuzzleLocation).Rotation();

    if (MuzzleFlashVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), MuzzleFlashVFX, MuzzleLocation, MuzzleRotation);
    }

    if (MuzzleSmokeVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), MuzzleSmokeVFX, MuzzleLocation, MuzzleRotation);
    }

    // ==========================================================
    // 🚀 1. 투사체(Projectile) 발사 모드 분기 (수식 개깔끔해짐)
    // ==========================================================
    if (WeaponInst->WeaponData->bIsProjectile && WeaponInst->WeaponData->ProjectileClass)
    {
        FTransform SpawnTransform(MuzzleRotation, MuzzleLocation);

        ASRProjectile* SpawnedProj = GetWorld()->SpawnActorDeferred<ASRProjectile>(
            WeaponInst->WeaponData->ProjectileClass, SpawnTransform, Avatar, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (SpawnedProj)
        {
            SpawnedProj->InstigatorActor = Avatar;
            SpawnedProj->DamageAmount = WeaponInst->WeaponData->BaseDamage;
            SpawnedProj->SetImpactForce(WeaponInst->WeaponData->ImpactForce);
            SpawnedProj->DamageEffectClass = DamageEffectClass; 
            SpawnedProj->SourceWeaponData = WeaponInst->WeaponData;

            SpawnedProj->FinishSpawning(SpawnTransform);
            
            FVector ShootDir = (TargetPoint - MuzzleLocation).GetSafeNormal();
            SpawnedProj->SetSpeed(WeaponInst->WeaponData->ProjectileSpeed, ShootDir); 
        }
        return; 
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
            ContextHandle.AddHitResult(*HitResult);
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