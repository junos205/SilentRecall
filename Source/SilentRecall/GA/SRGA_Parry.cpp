#include "SRGA_Parry.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Character.h"
#include "Weapon/SRProjectile.h"
// #include "Weapon/SRProjectile.h" // 투사체 반사 기능이 필요하다면 인클루드!

USRGA_Parry::USRGA_Parry()
{
    // 스킬 인스턴싱 정책 (보통 캐릭터마다 하나씩 생성)
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // ⭐️ 1. 이 스킬이 켜지는 순간 "나는 패링 중이다" 태그를 자동으로 부여합니다!
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Parry.Active")));
    
    // 이 스킬의 고유 태그
    FGameplayTagContainer TempTags;
    TempTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.Event.Skill.Parry")));
    SetAssetTags(TempTags);
}

void USRGA_Parry::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ==========================================================
    // 🎬 트랙 1: 패링 대기 몽타주 재생
    // ==========================================================
    if (ParryAnticipationMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ParryAnticipationMontage);
        
        // 몽타주가 자연스럽게 끝나거나, 취소되면 스킬 종료
        MontageTask->OnCompleted.AddDynamic(this, &USRGA_Parry::OnAnticipationMontageEnded);
        MontageTask->OnInterrupted.AddDynamic(this, &USRGA_Parry::OnAnticipationMontageEnded);
        MontageTask->OnCancelled.AddDynamic(this, &USRGA_Parry::OnAnticipationMontageEnded);
        
        MontageTask->ReadyForActivation();
    }

    // ==========================================================
    // 📡 트랙 2: ExecCalc의 "패링 성공" 무전 기다리기!
    // ==========================================================
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag(FName("Event.Character.ParrySuccess")));
    
    // 무전이 도착하면 OnParryEventReceived 함수 실행!
    EventTask->EventReceived.AddDynamic(this, &USRGA_Parry::OnParryEventReceived);
    
    EventTask->ReadyForActivation();
}

void USRGA_Parry::OnParryEventReceived(FGameplayEventData Payload)
{
    // 1. 대기 몽타주(헛스윙) 멈춤!
    MontageStop();

    // ==========================================================
    // 🏓 2. [핵심] 날아온 투사체 반대로 날려버리기!
    // ==========================================================
    if (Payload.OptionalObject)
    {
        // 소포(OptionalObject)를 열어서 이게 투사체가 맞는지 확인합니다.
        ASRProjectile* BlockedProjectile = Cast<ASRProjectile>(const_cast<UObject*>(Payload.OptionalObject.Get()));
        if (BlockedProjectile)
        {
            // 투사체가 맞다면? "내가 새 주인이니까 뒤로 돌아가!" 명령 하달
            BlockedProjectile->DeflectProjectile(GetAvatarActorFromActorInfo());
        }
    }

    // 3. 챙! 하고 멋지게 튕겨내는 성공 몽타주 재생
    if (ParrySuccessMontage)
    {
        UAbilityTask_PlayMontageAndWait* SuccessTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ParrySuccessMontage);
        SuccessTask->OnCompleted.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
        SuccessTask->OnInterrupted.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
        SuccessTask->OnCancelled.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
        SuccessTask->ReadyForActivation();
    }
    else
    {
        OnSuccessMontageEnded();
    }
}

void USRGA_Parry::OnAnticipationMontageEnded()
{
    // 적의 공격이 안 와서 허공에 칼만 휘두르고 끝난 상황
    bool bReplicateEndAbility = true;
    bool bWasCancelled = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USRGA_Parry::OnSuccessMontageEnded()
{
    // 성공 피드백 동작까지 모두 마친 상황
    bool bReplicateEndAbility = true;
    bool bWasCancelled = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USRGA_Parry::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 스킬이 끝나면 ActivationOwnedTags에 등록했던 State.Parry.Active 태그도 자동으로 내려갑니다!
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}