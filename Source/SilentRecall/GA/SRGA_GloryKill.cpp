// Fill out your copyright notice in the Description page of Project Settings.

#include "SRGA_GloryKill.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "MotionWarpingComponent.h"
#include "AttributeSet/SRDefaultAttributeSet.h" // 경로에 맞게 수정해주세요!

USRGA_GloryKill::USRGA_GloryKill()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTagContainer TempTags;
    TempTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.GloryKill")));
    SetAssetTags(TempTags);

    // ⭐️ [매우 중요] 처형 도중에는 내가 무적(Invincible)이 되고, 글로리 킬 상태임을 알립니다!
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Buff.Invincible")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.GloryKill")));

    // 이미 파쿠르 중이거나 기절 상태면 처형 불가
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
}

void USRGA_GloryKill::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 전방의 적 탐색!
    CurrentVictim = FindExecutionTarget();

    if (CurrentVictim)
    {
        // 2. 조건에 맞는 적이 있다면 처형 시작!
        PlayExecution(CurrentVictim);
    }
    else
    {
        // 3. 타겟이 없으면 능력 즉시 종료 (허공에 스킬 날림 방지)
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

AActor* USRGA_GloryKill::FindExecutionTarget()
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar) return nullptr;

    FVector StartLoc = Avatar->GetActorLocation();
    FVector ForwardDir = Avatar->GetActorForwardVector();
    
    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(250.0f); // 탐색 반경 250
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Avatar);

    // 캐릭터의 약간 앞에서부터 탐색 시작
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

            // 이미 죽은 적(IsDead 태그 보유)은 시체 매너를 위해 스킵
            if (TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead")))) continue;

            // ⭐️ 체력 검사 (예: 50.0 이하일 때만 발동)
            float CurrentHealth = TargetASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());

            // 체력이 50 초과면 처형 불가이므로 패스!
            if (CurrentHealth > 50.0f) continue;

            // ⭐️ 각도 검사 (내적: Dot Product) - 0.7 이상이면 전방 약 45도 시야각 이내!
            FVector DirToTarget = (PotentialTarget->GetActorLocation() - StartLoc).GetSafeNormal();
            float DotProduct = FVector::DotProduct(ForwardDir, DirToTarget);
            
            if (DotProduct > 0.7f) 
            {
                // 가장 가까운 적을 찾아서 BestTarget으로 설정
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

void USRGA_GloryKill::PlayExecution(AActor* TargetActor)
{
    // 1. 적 몸에 Stun 태그를 붙여서 꼼짝 못 하게 만듦
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC)
    {
        TargetASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    }

    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    ACharacter* TargetChar = Cast<ACharacter>(TargetActor);

    // 2. 모션 워핑 (나와 적을 완벽한 위치로 맞물리게 함)
    if (AvatarChar && TargetChar)
    {
        if (UMotionWarpingComponent* MotionWarpComp = AvatarChar->FindComponentByClass<UMotionWarpingComponent>())
        {
            // ⭐️ 목표 지점: 적의 바로 앞 (예: 전방 80 유닛 거리)
            FVector WarpLocation = TargetChar->GetActorLocation() + (TargetChar->GetActorForwardVector() * 80.0f);
            
            // ⭐️ 목표 회전: 적과 내가 서로 마주 보도록!
            FRotator WarpRotation = (TargetChar->GetActorLocation() - WarpLocation).Rotation();

            // "ExecutionTarget"이라는 이름으로 워프 좌표를 심어줌
            MotionWarpComp->AddOrUpdateWarpTargetFromLocationAndRotation(FName("ExecutionTarget"), WarpLocation, WarpRotation);
        }
    }

    // 3. 내 처형 몽타주 재생 (PlayMontageAndWait 활용)
    if (AttackerMontage)
    {
        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, AttackerMontage, 1.0f
        );
        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_GloryKill::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_GloryKill::OnMontageCompleted);
        PlayMontageTask->ReadyForActivation();
    }

    // 4. 적의 리액션(Victim) 몽타주 재생
    if (TargetChar && VictimMontage)
    {
        // 간단히 적의 메인 메쉬에 애니메이션을 틉니다.
        TargetChar->PlayAnimMontage(VictimMontage);
    }
}

void USRGA_GloryKill::OnMontageCompleted()
{
    // 몽타주가 끝나는 순간 적에게 9999 데미지(즉사) 적용
    if (CurrentVictim && ExecutionDamageEffect)
    {
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CurrentVictim);
        if (TargetASC)
        {
            FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
            ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
            
            FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(ExecutionDamageEffect, GetAbilityLevel(), ContextHandle);
            if (SpecHandle.IsValid())
            {
                GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
            }
        }
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}