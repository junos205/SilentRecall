// Fill out your copyright notice in the Description page of Project Settings.


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

USRGA_Melee::USRGA_Melee()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTagContainer TempTags;
    TempTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Attack.Melee")));
    SetAssetTags(TempTags);
    
    // ⭐️ [추가됨] 이 태그들을 달고 있는 동안에는 근접 공격 실행 불가!
    // (기절, 피격, 벽 넘기 중에는 칼질 불가)
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
    // 기존 ActivationBlockedTags 아래에 추가
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Exhausted")));
    // (선택) 근접 공격을 실행할 때 내 몸에 달아줄 태그 (진행 중임을 알리기 위해)
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Melee")));
}
void USRGA_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (FindExecutionTarget() != nullptr)
    {
        // 대상이 있다면? 내 몸(ASC)에게 "나 대신 글로리 킬 GA 켜줘!" 라고 명령 토스
        FGameplayTag GloryKillTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.GloryKill"));
        
        if (GetAbilitySystemComponentFromActorInfo()->TryActivateAbilitiesByTag(FGameplayTagContainer(GloryKillTag)))
        {
            // 글로리 킬이 성공적으로 켜졌다면, 평타는 기력 소모 없이 칼같이 종료!
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }
    
    // ⭐️ 1타 기력 소모 및 쿨타임 결제! (실패 시 발동 안 됨)
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    CurrentComboIndex = 1;
    bIsComboSaved = false;

    // 1. 첫 타격 재생
    PlayComboSection();

    // ❌ 태스크 생성 코드 삭제됨! (InputTask 부분 전부 날림)

    // 2. 타이밍 체크 대기 태스크 (이것만 남겨둡니다. 몽타주 노티파이를 들어야 하니까요!)
    FGameplayTag ComboCheckTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.CheckCombo"));
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboCheckTag);
    EventTask->EventReceived.AddDynamic(this, &USRGA_Melee::OnComboCheckEventReceived);
    EventTask->ReadyForActivation();

    FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Melee.Hit"));
    UAbilityTask_WaitGameplayEvent* WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag);
    
    // GA 이벤트 발동 시 OnHitEventReceived 함수를 실행
    WaitHitTask->EventReceived.AddDynamic(this, &USRGA_Melee::OnHitEventReceived);
    
    // 태스크 실행
    WaitHitTask->ReadyForActivation();
}

AActor* USRGA_Melee::FindExecutionTarget()
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar) return nullptr;

    FVector StartLoc = Avatar->GetActorLocation();
    FVector ForwardDir = Avatar->GetActorForwardVector();
    
    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(250.0f); // 처형 인식 반경
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Avatar);

    // 내 앞쪽으로 살짝 구체를 날려봄
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

            // 시체는 두 번 죽이지 않음
            if (TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead")))) continue;

            // 1. 체력 검사 (50 이하일 때만 발동)
            float CurrentHealth = TargetASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());
            if (CurrentHealth > 50.0f) continue;

            // 2. 각도 검사 (내적 0.7 이상: 약 정면 45도 시야각 이내)
            FVector DirToTarget = (PotentialTarget->GetActorLocation() - StartLoc).GetSafeNormal();
            if (FVector::DotProduct(ForwardDir, DirToTarget) > 0.7f) 
            {
                // 3. 거리 검사 (가장 가까운 놈 찾기)
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
        // 1. 컨텍스트 주머니 생성
        FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
        ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

        // ⭐️ 2. [핵심 추가] ANS에서 페이로드에 담아 보냈던 타격 정보(TargetData)를 꺼내서 주머니에 쏙 넣습니다!
        if (Payload.TargetData.IsValid(0))
        {
            const FHitResult* HitResult = Payload.TargetData.Get(0)->GetHitResult();
            if (HitResult)
            {
                ContextHandle.AddHitResult(*HitResult);
            }
        }

        // 3. 이펙트 스펙 만들고 발사! (이제 이 스펙 안에 HitResult가 들어있습니다)
        FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);

        if (SpecHandle.IsValid())
        {
            GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
        }
    }
}

void USRGA_Melee::InputPressed(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputPressed(Handle, ActorInfo, ActivationInfo);
    
    bIsComboSaved = true;
}

void USRGA_Melee::OnComboCheckEventReceived(FGameplayEventData Payload)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    bool bShouldProceedCombo = bIsComboSaved; // 플레이어의 입력 여부로 초기화

    // ==========================================================
    // ⭐️ 1. AI 콤보 의지(태그) 확인 로직
    // ==========================================================
    if (Avatar && Cast<AAIController>(Avatar->GetInstigatorController()) != nullptr)
    {
        UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
        FGameplayTag WantsToComboTag = FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Combat.Fire"));
        
        // StateTree가 사거리 안에 있어서 태그를 붙여줬다면 콤보 진행!
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

    // ==========================================================
    // ⭐️ 2. 유저님 원본: 1P/3P 애니메이션 섹션 점프 로직!
    // ==========================================================
    if (bShouldProceedCombo && CurrentComboIndex < MaxComboCount)
    {
        // ⭐️ 1. 내 몸에 탈진(Exhausted) 태그가 있는지 확인합니다.
        FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Exhausted"));
        bool bIsExhausted = GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(ExhaustedTag);

        // ⭐️ 2. 탈진 상태가 아닐 때만! 그리고 지갑에 기력(Cost)이 있을 때만! 콤보를 이어나갑니다.
        if (!bIsExhausted && CheckCost(CurrentSpecHandle, CurrentActorInfo))
        {
            ApplyCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
            // ... 콤보 애니메이션 점프 로직 ...
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
            // 💡 [신규 추가] 마우스 광클을 했더라도, 기력이 모자라면 콤보가 여기서 강제 중단됩니다!
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

        // ⭐️ [추가된 로직] 1인칭 메쉬(1P)는 인터페이스로 재생!
        if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(GetAvatarActorFromActorInfo()))
        {
            CharInterface->PlayWeaponMontage(ComboMontage, true);
        }

        // 기존 태스크: 3인칭 메쉬(3P) 재생 및 몽타주 종료 추적
        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, ComboMontage, 1.0f, SectionName
        );

        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_Melee::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_Melee::OnMontageCompleted);
        
        // 피격당해서 강제 캔슬되었을 때도 태그를 떼고 종료하게 해주는 캔슬 보험!
        PlayMontageTask->OnCancelled.AddDynamic(this, &USRGA_Melee::OnMontageCompleted);
        // ================================

        // 태스크 실행
        PlayMontageTask->ReadyForActivation();
    }
}

void USRGA_Melee::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    
    bIsComboSaved = false;
    CurrentComboIndex = 1;
}
