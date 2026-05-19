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

    FGameplayTag FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
    UAbilityTask_WaitGameplayEvent* WaitFireTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FireEventTag);
    WaitFireTask->EventReceived.AddDynamic(this, &USRGA_RangedAttack::OnFireEventReceived);
    WaitFireTask->ReadyForActivation();

    FireShot();
}

void USRGA_RangedAttack::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);
    // 버튼을 떼면 알아서 루프가 멈추므로 특별한 로직이 당장 필요하진 않으나,
    // 필요하다면 여기서 즉시 EndAbility를 호출할 수도 있습니다.
}

void USRGA_RangedAttack::FireShot()
{
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    ASRBaseCharacter* AvatarChar = Cast<ASRBaseCharacter>(AvatarActor);
    if (!AvatarChar) 
    {
        UE_LOG(LogTemp, Error, TEXT("[RangedAttack] FireShot Failed: Avatar is not ASRBaseCharacter."));
        return;
    }

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    bool bIsAI = (AIC != nullptr);

    // ⭐️ [런앤건 스위치] 수정한 태그 반영! 태그가 떨어지면 사격 루프 즉시 종료
    if (bIsAI)
    {
        FGameplayTag FireCommandTag = FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Combat.Fire"));
        if (!GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FireCommandTag))
        {
            UE_LOG(LogTemp, Log, TEXT("[RangedAttack] AI Fire Command Tag removed. Stopping Fire Loop."));
            EndAbilityDelegate();
            return;
        }
    }

    if (bIsAI)
    {
        AActor* TargetActor = AIC->GetFocusActor();
        if (TargetActor)
        {
            FVector DirectionToTarget = (TargetActor->GetActorLocation() - AvatarChar->GetActorLocation()).GetSafeNormal();
            FVector MyForward = AvatarChar->GetActorForwardVector();
            float DotResult = FVector::DotProduct(DirectionToTarget, MyForward);
            
            // AI가 걸으면서 쏠 때 몸이 살짝 틀어져도 쏠 수 있게 0.8f로 유지
            if (DotResult < 0.8f)
            {
                UE_LOG(LogTemp, Warning, TEXT("[RangedAttack] AI is turning... Retrying in 0.05s."));
                UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, 0.05f);
                WaitTask->OnFinish.AddDynamic(this, &USRGA_RangedAttack::FireShot);
                WaitTask->ReadyForActivation();
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

    WeaponInst->ConsumeAmmo();
    UE_LOG(LogTemp, Log, TEXT("[RangedAttack] Ammo consumed."));

    UAnimMontage* FireMontage = WeaponInst->WeaponData->AttackComboMontages.Num() > 0 ? WeaponInst->WeaponData->AttackComboMontages[0] : nullptr;
    if (FireMontage)
    {
        if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(AvatarChar))
        {
            CharInterface->PlayWeaponMontage(FireMontage, true);
        }

        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, FireMontage, 1.0f
        );
        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_RangedAttack::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_RangedAttack::OnMontageCompleted);
        PlayMontageTask->ReadyForActivation();
        UE_LOG(LogTemp, Log, TEXT("[RangedAttack] Fire Montage Played."));
    }

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

    // ⭐️ [연사 루프 제어] AI는 스위치 태그가 있는 한 무조건 루프를 돕니다!
    bool bShouldLoop = false;
    if (WeaponInst->WeaponData->bIsAutomatic)
    {
        bShouldLoop = (GetCurrentAbilitySpec()->InputPressed || bIsAI);
    }

    if (bShouldLoop)
    {
        UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, WeaponInst->WeaponData->FireRate);
        WaitTask->OnFinish.AddDynamic(this, &USRGA_RangedAttack::FireShot);
        WaitTask->ReadyForActivation();
    }
    else
    {
        UAbilityTask_WaitDelay* CooldownTask = UAbilityTask_WaitDelay::WaitDelay(this, WeaponInst->WeaponData->FireRate);
        CooldownTask->OnFinish.AddDynamic(this, &USRGA_RangedAttack::EndAbilityDelegate);
        CooldownTask->ReadyForActivation();
    }
}

void USRGA_RangedAttack::OnFireEventReceived(FGameplayEventData Payload)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    USRWeaponInstance* WeaponInst = Cast<USRWeaponInstance>(GetCurrentSourceObject());
    if (!Avatar || !WeaponInst || !WeaponInst->WeaponData || !DamageEffectClass) return;

    FVector MuzzleLocation = Avatar->GetActorLocation();
    if (ASRBaseCharacter* BaseChar = Cast<ASRBaseCharacter>(Avatar))
    {
        MuzzleLocation = BaseChar->GetMesh()->GetSocketLocation(FName("Muzzle"));
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

            SpawnedProj->FinishSpawning(SpawnTransform);
            FVector ShootDir = (TargetPoint - MuzzleLocation).GetSafeNormal();
            SpawnedProj->SetSpeed(WeaponInst->WeaponData->ProjectileSpeed, ShootDir); 
        }
    }

    AActor* OriginalShooter = Avatar;
    AActor* TargetActor = const_cast<AActor*>(Payload.Target.Get());
    if (TargetActor)
    {
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
        if (TargetASC)
        {
            FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
            if (HitResult) ContextHandle.AddHitResult(*HitResult);

            FGameplayTag ParryTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Parry.Active"));
            if (TargetASC->HasMatchingGameplayTag(ParryTag))
            {
                TargetASC = GetAbilitySystemComponentFromActorInfo();
                ContextHandle.AddInstigator(TargetActor, TargetActor);
            }
            else
            {
                ContextHandle.AddInstigator(OriginalShooter, OriginalShooter);
            }

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
    // 몽타주가 끝났을 때 추가로 처리할 내용이 있다면 여기에 작성
    // (연사 루프가 돌고 있을 수 있으므로 무조건 여기서 EndAbility를 부르지는 않습니다.)
}