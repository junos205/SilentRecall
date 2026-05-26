#include "GA/SRGA_Grappling.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "DrawDebugHelpers.h"
#include "Character/SRPlayerCharacter.h" 
#include "AbilitySystemComponent.h" 
#include "GameplayTagContainer.h"   
#include "GameFramework/CharacterMovementComponent.h" 
#include "Gimmick/SRGrapplePoint.h"

USRGA_Grappling::USRGA_Grappling()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 그래플링 상태 태그 부여
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Grappling")));
    
    // 피격 중이거나 파쿠르 중이면 훅을 쏠 수 없음
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
}

void USRGA_Grappling::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
   if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
      return;
   }

   ASRPlayerCharacter* SRCharacter = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
   if (!SRCharacter)
   {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
      return;
   }

   // 🎯 [변경] 플레이어가 이미 틱에서 락온(조준)해둔 포인트를 즉시 획득합니다.
   ASRGrapplePoint* TargetPoint = SRCharacter->GetCurrentGrappleTarget();

   if (TargetPoint != nullptr)
   {
      // 액터의 중심 위치 혹은 컴포넌트 위치를 타깃으로 설정합니다.
      FVector TargetLoc = TargetPoint->GetActorLocation();
        
      // 캐릭터에게 그래플링 시작 명령 하달
      SRCharacter->StartGrapple(TargetLoc);
      return; 
   }
    
   // 화면에 락온된 타깃 UI가 없을 때 버튼을 눌렀다면 즉시 능력 취소 처리
   EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void USRGA_Grappling::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
       ASRPlayerCharacter* SRCharacter = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
       if (SRCharacter)
       {
          SRCharacter->StopGrapple();
       }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled); 
}

void USRGA_Grappling::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);

    if (IsActive())
    {
       EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void USRGA_Grappling::OnInputReleased(float TimeHeld)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}