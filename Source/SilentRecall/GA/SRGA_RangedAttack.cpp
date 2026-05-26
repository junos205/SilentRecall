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

    FGameplayTag FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
    UAbilityTask_WaitGameplayEvent* WaitFireTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FireEventTag);
    WaitFireTask->EventReceived.AddDynamic(this, &USRGA_RangedAttack::OnFireEventReceived);
    WaitFireTask->ReadyForActivation();

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
    }

    if (bIsAI)
    {
        AActor* TargetActor = AIC->GetFocusActor();
        if (TargetActor)
        {
            FVector DirectionToTarget = (TargetActor->GetActorLocation() - AvatarChar->GetActorLocation()).GetSafeNormal();
            FVector MyForward = AvatarChar->GetActorForwardVector();
            float DotResult = FVector::DotProduct(DirectionToTarget, MyForward);
            
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

    // ⭐️ 침묵의 살인마들 검문소! (누가 범인인지 로그로 고발합니다)
    if (!Avatar) { UE_LOG(LogTemp, Error, TEXT("[RangedAttack] 🔴 Avatar is NULL!")); return; }
    if (!WeaponInst) { UE_LOG(LogTemp, Error, TEXT("[RangedAttack] 🔴 WeaponInst is NULL! (Did you set SourceObject?)")); return; }
    if (!WeaponInst->WeaponData) { UE_LOG(LogTemp, Error, TEXT("[RangedAttack] 🔴 WeaponData is NULL!")); return; }
    if (!DamageEffectClass) { UE_LOG(LogTemp, Error, TEXT("[RangedAttack] 🔴 DamageEffectClass is NULL! (Check BP_SRGA_RangedAttack)")); return; }

    FVector MuzzleLocation = Avatar->GetActorLocation(); 
    bool bFoundSocket = false;
    
    // ⭐️ 1. 플레이어 캐릭터일 경우 (1인칭 메쉬 검사)
    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(Avatar))
    {
        USkeletalMeshComponent* FP_Mesh = PlayerChar->Get1PMesh();
        if (FP_Mesh)
        {
            // 유저님이 만들어두신 GetWeaponMeshForComponent를 활용해 1인칭 팔에 달린 '진짜 복제 무기' 메쉬를 가져옵니다!
            if (USkeletalMeshComponent* FP_WeaponMesh = PlayerChar->GetWeaponMeshForComponent(FP_Mesh))
            {
                if (FP_WeaponMesh->DoesSocketExist(FName("Muzzle")))
                {
                    MuzzleLocation = FP_WeaponMesh->GetSocketLocation(FName("Muzzle"));
                    bFoundSocket = true;
                    UE_LOG(LogTemp, Log, TEXT("[RangedAttack] 🟢 SUCCESS: Found Muzzle on Player's 1P Weapon Mesh!"));
                }
            }
            
            // 만약 무기 메쉬에서 못 찾았다면 1인칭 팔 메쉬 자체에 소켓이 있는지 확인
            if (!bFoundSocket && FP_Mesh->DoesSocketExist(FName("Muzzle")))
            {
                MuzzleLocation = FP_Mesh->GetSocketLocation(FName("Muzzle"));
                bFoundSocket = true;
                UE_LOG(LogTemp, Log, TEXT("[RangedAttack] 🟢 SUCCESS: Found Muzzle on Player's 1P Arms Mesh!"));
            }
        }
    }
    // ⭐️ 2. 적 AI일 경우 (3인칭 무기 액터 검사)
    else 
    {
        USRInventoryComponent* InvComp = Avatar->FindComponentByClass<USRInventoryComponent>();
        if (InvComp && InvComp->GetCurrentActiveWeaponActor())
        {
            AActor* WeaponActor = InvComp->GetCurrentActiveWeaponActor();
            
            // AI는 ASRWeaponPickup 안의 모든 스켈레탈 메쉬를 순회합니다.
            TArray<USkeletalMeshComponent*> SkelMeshes;
            WeaponActor->GetComponents<USkeletalMeshComponent>(SkelMeshes);
            
            for (USkeletalMeshComponent* SkelMesh : SkelMeshes)
            {
                if (SkelMesh && SkelMesh->DoesSocketExist(FName("Muzzle")))
                {
                    MuzzleLocation = SkelMesh->GetSocketLocation(FName("Muzzle"));
                    bFoundSocket = true;
                    UE_LOG(LogTemp, Log, TEXT("[RangedAttack] 🟢 SUCCESS: Found Muzzle on AI's Weapon Mesh!"));
                    break;
                }
            }
        }
    }

    // ⭐️ 3. 최후의 경고 로그 (그래도 못 찾았을 때)
    if (!bFoundSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("[RangedAttack] 🔴 CRITICAL ERROR: 'Muzzle' socket NOT found for Avatar: %s"), *Avatar->GetName());
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

}