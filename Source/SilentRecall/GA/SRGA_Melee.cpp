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
#include "Perception/AISense_Hearing.h"

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
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Dash")));
    
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Melee")));
}

void USRGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());

    if (AvatarChar && AvatarChar->IsPlayerControlled() && FindExecutionTarget() != nullptr)
    {
        FGameplayTag GloryKillTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.GloryKill"));
        if (GetAbilitySystemComponentFromActorInfo()->TryActivateAbilitiesByTag(FGameplayTagContainer(GloryKillTag)))
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }
    
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    CurrentComboIndex = 1;
    bIsComboSaved = false;

    // 🎯 [신규 추가] 근접 공격 시작 시 타겟을 정밀 서치하여 락온 회로를 가동합니다.
    LockedOnTarget = ScanMeleeLockOnTarget();
    if (LockedOnTarget.IsValid() && AvatarChar && AvatarChar->IsPlayerControlled())
    {
        // 플레이어 캐릭터의 마우스 입력 바인딩 제어를 위해 커스텀 플래그 가드 가동 (3단계에서 처리)
        if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(AvatarChar))
        {
            // 필요 시 캐릭터 인터페이스나 캐스팅을 통해 캐릭터 내부의 입력 차단 변수를 켭니다.
            // 여기서는 깔끔하게 플레이어 컨트롤러의 IgnoreLookInput을 활용해 마우스 휙휙 도는 현상을 차단합니다.
            if (APlayerController* PC = Cast<APlayerController>(AvatarChar->GetController()))
            {
                PC->SetIgnoreLookInput(true);
            }
        }

        // 매 프레임(0.01초 간격) 적을 향해 카메라를 회전시키는 실시간 태스크 구동
        GetWorld()->GetTimerManager().SetTimer(MeleeLockOnTimerHandle, this, &USRGA_Melee::ExecuteMeleeLockOnTick, 0.01f, true);
    }

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

// 2️⃣ 🌟 [무결성 철저 방어] 어빌리티가 어떤 이유로든 종료(EndAbility)될 때 무조건 잠금을 풀어줍니다.
// 다른행동(대시/파쿠르) 발동으로 인해 MeleeGA가 캔슬되더라도 이 오버라이드 함수가 백엔드에서 100% 실행되므로 복구됩니다!
void USRGA_Melee::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ClearMeleeLockOn();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 3️⃣ 🌟 [핵심 알고리즘] 시야 범위 내적 0.5 필터링 스캔 기능 구현
AActor* USRGA_Melee::ScanMeleeLockOnTarget() const
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar) return nullptr;

    APlayerController* PC = Cast<APlayerController>(Avatar->GetInstigatorController());
    if (!PC || !PC->PlayerCameraManager) return nullptr;

    FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
    FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();

    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(LockOnRadius);
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Avatar);

    // 전방 구형 스윕을 통해 타겟 후보군 추출
    bool bHit = GetWorld()->SweepMultiByChannel(HitResults, CameraLoc, CameraLoc + (CameraForward * 10.0f), FQuat::Identity, ECC_Pawn, SphereShape, Params);

    AActor* BestTarget = nullptr;
    float BestDot = -1.0f;

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            AActor* Enemy = Hit.GetActor();
            if (!Enemy) continue;

            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
            if (!TargetASC || TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead")))) continue;

            // 시선과 적까지의 3D 벡터 내적 정산
            FVector DirToTarget = (Enemy->GetActorLocation() - CameraLoc).GetSafeNormal();
            float DotResult = FVector::DotProduct(CameraForward, DirToTarget);

            // 🎯 내적 0.5 (시야각 60도 이내) 필터 통과 검문 및 가장 정면에 가까운 적 선별
            if (DotResult >= 0.5f && DotResult > BestDot)
            {
                BestDot = DotResult;
                BestTarget = Enemy;
            }
        }
    }
    return BestTarget;
}

// 4️⃣ 🌟 [실시간 카메라 슬라이딩] 적을 향해 컨트롤 로테이션을 부드럽게 감속 보간
void USRGA_Melee::ExecuteMeleeLockOnTick()
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar || !LockedOnTarget.IsValid())
    {
        ClearMeleeLockOn();
        return;
    }

    APlayerController* PC = Cast<APlayerController>(Avatar->GetInstigatorController());
    if (!PC || !PC->PlayerCameraManager) return;

    // 타겟이 도중에 죽었는지 재차 가드
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(LockedOnTarget.Get());
    if (TargetASC && TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"))))
    {
        ClearMeleeLockOn();
        return;
    }

    FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
    // 🎯 적의 골반 원점보다는 약간 위쪽(가슴/헤드 중간 높이)을 바라보도록 Z축 보정 보충
    FVector TargetTargetLoc = LockedOnTarget->GetActorLocation() + FVector(0.f, 0.f, 20.f);

    // 현재 카메라 각도에서 적을 정면으로 바라보는 각도 구하기
    FRotator TargetRot = (TargetTargetLoc - CameraLoc).Rotation();
    FRotator CurrentRot = PC->GetControlRotation();

    // 1인칭 사격감이 훼손되지 않도록 Roll 축은 완벽 거세
    TargetRot.Roll = 0.0f;

    // 프레임 독립적인 부드러운 회전 감속 보간(RInterpTo) 실행
    float DeltaTime = GetWorld()->GetDeltaSeconds();
    FRotator NewControlRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, LockOnInterpSpeed);

    PC->SetControlRotation(NewControlRot);
}

// 5️⃣ 락온 리셋 및 마우스 입력 제어권 반환 마감
void USRGA_Melee::ClearMeleeLockOn()
{
    GetWorld()->GetTimerManager().ClearTimer(MeleeLockOnTimerHandle);
    
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (Avatar)
    {
        if (APlayerController* PC = Cast<APlayerController>(Avatar->GetInstigatorController()))
        {
            // 🔓 마우스 회전 입력을 다시 정상 상태로 온전하게 복귀 복구시킵니다.
            PC->SetIgnoreLookInput(false);
        }
    }
    LockedOnTarget = nullptr;
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
        
        if (SpecHandle.IsValid())
        {
            GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
        }

        // =======================================================================
        // 🔊 [신규 추가] 근접 피격 소음(Noise) 발생
        // =======================================================================
        if (AActor* Attacker = GetAvatarActorFromActorInfo())
        {
            // 소리 발생지는 '맞은 적의 위치'로 설정 (주변 동료들이 윽! 소리 듣고 돌아봄)
            FVector HitNoiseLocation = TargetActor->GetActorLocation();
            
            // 총소리보다 작은 반경 (예: 8미터)
            float MeleeLoudness = 0.6f;
            float MeleeMaxRange = 800.0f; 
            FName MeleeNoiseTag = TEXT("MeleeHit");

            UAISense_Hearing::ReportNoiseEvent(
                GetWorld(),
                HitNoiseLocation,
                MeleeLoudness,
                Attacker, // 공격한 플레이어
                MeleeMaxRange,
                MeleeNoiseTag
            );
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