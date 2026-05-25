#include "SRGA_Melee.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Interface/SRCharacterInterface.h"
#include "Weapon/SRWeaponInstance.h"
#include "AIController.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "Components/CapsuleComponent.h"

USRGA_Melee::USRGA_Melee()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTagContainer TempTags;
    TempTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Attack.Melee")));
    SetAssetTags(TempTags);
    
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Exhausted")));
    
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Melee")));
}

void USRGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // ⭐️ [수정] AActor* 대신 ACharacter*로 안전하게 캐스팅하여 가져옵니다.
    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());

    // ⭐️ 이제 AvatarChar->IsPlayerControlled()를 정상적으로 인식합니다!
    if (AvatarChar && AvatarChar->IsPlayerControlled() && FindExecutionTarget() != nullptr)
    {
        FGameplayTag GloryKillTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.GloryKill"));
        
        if (GetAbilitySystemComponentFromActorInfo()->TryActivateAbilitiesByTag(FGameplayTagContainer(GloryKillTag)))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }
    
    // 코스트 및 쿨타임 결제 (오버라이드된 CheckCost, ApplyCost가 내부적으로 자동 실행됨)
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    CurrentComboIndex = 1;
    bIsComboSaved = false;

    PlayComboSection();

    FGameplayTag ComboCheckTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.CheckCombo"));
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboCheckTag);
    EventTask->EventReceived.AddDynamic(this, &USRGA_Melee::OnComboCheckEventReceived);
    EventTask->ReadyForActivation();

    FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Melee.Hit"));
    UAbilityTask_WaitGameplayEvent* WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag);
    WaitHitTask->EventReceived.AddDynamic(this, &USRGA_Melee::OnHitEventReceived);
    WaitHitTask->ReadyForActivation();
}

// ⭐️ [신규 핵심 기능 1] 무기 데이터 기반 AP 잔액 검사 구현
bool USRGA_Melee::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;

    // 현재 들고 있는 무기 인스턴스 정보 추출
    USRWeaponInstance* WeaponInstance = Cast<USRWeaponInstance>(GetSourceObject(Handle, ActorInfo));
    if (WeaponInstance && WeaponInstance->WeaponData)
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        if (ASC)
        {
            float CurrentAP = ASC->GetNumericAttribute(USRDefaultAttributeSet::GetAPAttribute());
            // 내 AP가 현재 무기의 기력 소모량보다 적다면 코스트 지불 실패 처리!
            if (CurrentAP < WeaponInstance->WeaponData->MeleeAPCost)
            {
                return false;
            }
        }
    }
    return true;
}

void USRGA_Melee::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

    USRWeaponInstance* WeaponInstance = Cast<USRWeaponInstance>(GetSourceObject(Handle, ActorInfo));
    if (WeaponInstance && WeaponInstance->WeaponData)
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        if (ASC)
        {
            UGameplayEffect* DynamicCostGE = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("DynamicMeleeCostGE")));
            DynamicCostGE->DurationPolicy = EGameplayEffectDurationType::Instant;
            
            DynamicCostGE->Modifiers.SetNum(1);
            FGameplayModifierInfo& APMod = DynamicCostGE->Modifiers[0];
            APMod.Attribute = USRDefaultAttributeSet::GetAPAttribute();
            
            // ⭐️ [수정] EGameplayModOp::Add를 EGameplayModOp::Additive로 변경!
            APMod.ModifierOp = EGameplayModOp::Additive; 
            
            APMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-WeaponInstance->WeaponData->MeleeAPCost));

            ASC->ApplyGameplayEffectToSelf(DynamicCostGE, GetAbilityLevel(), ASC->MakeEffectContext());
        }
    }
}

AActor* USRGA_Melee::FindExecutionTarget()
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar) return nullptr;

    FVector StartLoc = Avatar->GetActorLocation();
    FVector ForwardDir = Avatar->GetActorForwardVector();
    
    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(100.0f); 
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Avatar);

    bool bHit = GetWorld()->SweepMultiByChannel(HitResults, StartLoc, StartLoc + (ForwardDir * 50.0f), FQuat::Identity, ECC_Pawn, SphereShape, QueryParams);

    AActor* BestTarget = nullptr;
    float MinDistanceSq = MAX_FLT;

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* PotentialTarget = Hit.GetActor();
            if (!PotentialTarget) continue;

            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PotentialTarget);
            if (!TargetASC) continue;

            if (TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead")))) continue;

            float CurrentHealth = TargetASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());
            if (CurrentHealth > 50.0f) continue;

            FVector DirToTarget = (PotentialTarget->GetActorLocation() - StartLoc).GetSafeNormal();
            if (FVector::DotProduct(ForwardDir, DirToTarget) > 0.7f) 
            {
                float DistSq = FVector::DistSquared(StartLoc, PotentialTarget->GetActorLocation());
                if (DistSq < MinDistanceSq)
                {
                    MinDistanceSq = DistSq;
                    BestTarget = PotentialTarget;
                }
            }
        }
    }
    return BestTarget;
}

void USRGA_Melee::OnHitEventReceived(FGameplayEventData Payload)
{
    AActor* TargetActor = const_cast<AActor*>(Payload.Target.Get());
    if (!TargetActor || !DamageEffectClass) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC)
    {
        FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

        if (Payload.TargetData.IsValid(0))
        {
            const FHitResult* HitResult = Payload.TargetData.Get(0)->GetHitResult();
            if (HitResult)
            {
                ContextHandle.AddHitResult(*HitResult);
            }
        }

        FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);
        if (SpecHandle.IsValid())
        {
            GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
        }
    }
}

void USRGA_Melee::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputPressed(Handle, ActorInfo, ActivationInfo);
    bIsComboSaved = true;
}

void USRGA_Melee::OnComboCheckEventReceived(FGameplayEventData Payload)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    bool bShouldProceedCombo = bIsComboSaved;

    if (Avatar && Cast<AAIController>(Avatar->GetInstigatorController()) != nullptr)
    {
        UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
        FGameplayTag WantsToComboTag = FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Combat.Fire"));
        
        if (ASC && ASC->HasMatchingGameplayTag(WantsToComboTag))
        {
            bShouldProceedCombo = true;
            UE_LOG(LogTemp, Warning, TEXT("[AttackGA] AI Wants to Combo!"));
        }
        else
        {
            bShouldProceedCombo = false;
        }
    }

    if (bShouldProceedCombo && CurrentComboIndex < MaxComboCount)
    {
        FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Exhausted"));
        bool bIsExhausted = GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(ExhaustedTag);

        // ⭐️ 오버라이드된 CheckCost 함수가 무기 데이터를 기반으로 콤보 연계 가능 여부를 판별합니다!
        if (!bIsExhausted && CheckCost(CurrentSpecHandle, CurrentActorInfo))
        {
            // ⭐️ 오버라이드된 ApplyCost 함수가 무기 소모량만큼 실시간 차감합니다.
            ApplyCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
            
            CurrentComboIndex++;
            bIsComboSaved = false; 

            USRWeaponInstance* WeaponInstance = Cast<USRWeaponInstance>(GetCurrentSourceObject());
            if (WeaponInstance && WeaponInstance->WeaponData && WeaponInstance->WeaponData->AttackComboMontages.Num() > 0)
            {
                FName SectionName = FName(*FString::Printf(TEXT("Attack%d"), CurrentComboIndex));
                MontageJumpToSection(SectionName);

                if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(Avatar))
                {
                    if (USkeletalMeshComponent* Mesh1P = CharInterface->Get1PMesh())
                    {
                        if (UAnimInstance* AnimInst1P = Mesh1P->GetAnimInstance())
                        {
                            AnimInst1P->Montage_JumpToSection(SectionName);
                        }
                    }
                }
            }
        }
        else
        {
            bIsComboSaved = false;
            UE_LOG(LogTemp, Warning, TEXT("[AttackGA] 기력 부족! 콤보를 이어나갈 수 없습니다."));
        }
    }
    else
    {
        bIsComboSaved = false;
    }
}

void USRGA_Melee::PlayComboSection()
{
    UE_LOG(LogTemp, Warning, TEXT("[AttackGA] Playing Combo Section: Attack%d"), CurrentComboIndex);
    
    USRWeaponInstance* WeaponInstance = Cast<USRWeaponInstance>(GetCurrentSourceObject());
    if (WeaponInstance && WeaponInstance->WeaponData && WeaponInstance->WeaponData->AttackComboMontages.Num() > 0)
    {
        UAnimMontage* ComboMontage = WeaponInstance->WeaponData->AttackComboMontages[0];
        FName SectionName = FName(*FString::Printf(TEXT("Attack%d"), CurrentComboIndex));

        if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(GetAvatarActorFromActorInfo()))
        {
            CharInterface->PlayWeaponMontage(ComboMontage, true);
        }

        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, ComboMontage, 1.0f, SectionName
        );

        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_Melee::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_Melee::OnMontageCompleted);
        PlayMontageTask->OnCancelled.AddDynamic(this, &USRGA_Melee::OnMontageCompleted);

        PlayMontageTask->ReadyForActivation();
    }
}

void USRGA_Melee::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    bIsComboSaved = false;
    CurrentComboIndex = 1;
}