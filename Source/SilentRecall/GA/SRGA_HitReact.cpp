#include "SRGA_HitReact.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystemComponent.h"
#include "Interface/SRCharacterInterface.h" // ⭐️ 인터페이스 호출을 위해 포함

USRGA_HitReact::USRGA_HitReact()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));

    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Attack.Melee")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Attack.Ranged")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Grapple")));
}

void USRGA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    UE_LOG(LogTemp, Warning, TEXT("[HitReact] Start Hit React from %s"), *AvatarActor->GetName());
    
    if (!TriggerEventData || !TriggerEventData->Target)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AActor* Victim = const_cast<AActor*>(TriggerEventData->Target.Get());
    FVector VictimLoc = Victim->GetActorLocation();
    FRotator ImpactRotation = FRotator::ZeroRotator;
    
    FVector ImpactPoint = VictimLoc; 
    
    if (TriggerEventData->TargetData.IsValid(0))
    {
        const FHitResult* HitResult = TriggerEventData->TargetData.Get(0)->GetHitResult();
        if (HitResult)
        {
            ImpactPoint = HitResult->ImpactPoint;
            ImpactRotation = FRotationMatrix::MakeFromX(ImpactPoint).Rotator();
        }
    }

    if (HitNiagaraVFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitNiagaraVFX, ImpactPoint, ImpactRotation);
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Buff.SuperArmor"))))
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitReact] Super Armor Active! Skipping Hit Montage."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    if (TriggerEventData->Instigator && !TriggerEventData->TargetData.IsValid(0))
    {
        ImpactPoint = TriggerEventData->Instigator->GetActorLocation();
    }
    
    FVector DirToHit = (ImpactPoint - VictimLoc).GetSafeNormal2D();
    FVector VictimForward = Victim->GetActorForwardVector().GetSafeNormal2D();
    FVector VictimRight = Victim->GetActorRightVector().GetSafeNormal2D();

    float ForwardDot = FVector::DotProduct(VictimForward, DirToHit);
    float RightDot = FVector::DotProduct(VictimRight, DirToHit);

    // ⭐️ 4방향 판별 후 인터페이스에 요청할 Enum 설정
    EHitDirection ChoosenDirection = EHitDirection::Front;

    if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
    {
        ChoosenDirection = (ForwardDot > 0.0f) ? EHitDirection::Front : EHitDirection::Back;
    }
    else
    {
        ChoosenDirection = (RightDot > 0.0f) ? EHitDirection::Right : EHitDirection::Left;
    }

    // ⭐️ 인터페이스를 통해 아바타 캐릭터로부터 몽타주를 동적으로 획득!
    UAnimMontage* MontageToPlay = nullptr;
    ISRCharacterInterface* CharacterInterface = Cast<ISRCharacterInterface>(AvatarActor);
    if (CharacterInterface)
    {
        MontageToPlay = CharacterInterface->GetHitReactMontage(ChoosenDirection);
    }

    // 5. 몽타주가 세팅되었다면 재생!
    if (MontageToPlay)
    {
        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            FName("HitReactMontage"),
            MontageToPlay
        );

        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_HitReact::OnMontageCompleted);
        PlayMontageTask->OnBlendOut.AddDynamic(this, &USRGA_HitReact::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_HitReact::OnMontageCompleted);
        PlayMontageTask->OnCancelled.AddDynamic(this, &USRGA_HitReact::OnMontageCompleted);

        PlayMontageTask->ReadyForActivation();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitReact] No Valid Montage found for direction. Ending Ability."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void USRGA_HitReact::OnMontageCompleted()
{
    bool bReplicatedEndAbility = true;
    bool bWasCancelled = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicatedEndAbility, bWasCancelled);
}