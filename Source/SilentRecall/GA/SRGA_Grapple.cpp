// Fill out your copyright notice in the Description page of Project Settings.

#include "SRGA_Grapple.h"
#include "Character/SRPlayerCharacter.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Camera/CameraComponent.h"

USRGA_Grapple::USRGA_Grapple()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 그래플링 상태 태그 부여
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Grappling")));
    
    // 피격 중이거나 파쿠르 중이면 훅을 쏠 수 없음
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
}

void USRGA_Grapple::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(GetAvatarActorFromActorInfo());
    if (!PlayerChar) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

    UCameraComponent* CameraComp = PlayerChar->FindComponentByClass<UCameraComponent>();
    if (CameraComp)
    {
        FVector StartLoc = CameraComp->GetComponentLocation();
        FVector LookDir = CameraComp->GetForwardVector();
        FVector EndLoc = StartLoc + (LookDir * 3000.0f); // 사거리 30m

        FHitResult HitResult;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(PlayerChar);

        // 💡 프로젝트에 맞게 ECC_Visibility 또는 별도 훅 채널로 변경하세요
        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, EndLoc, ECC_Visibility, Params);
        
        if (bHit)
        {
            // 캐릭터 내부에 구현된 그래플링 시작 로직 호출
            PlayerChar->StartGrapple(HitResult.ImpactPoint);
            
            // 유저가 해당 스킬 키(Input)를 뗄 때까지 대기
            UAbilityTask_WaitInputRelease* WaitInputTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
            WaitInputTask->OnRelease.AddDynamic(this, &USRGA_Grapple::OnInputReleased);
            WaitInputTask->ReadyForActivation();
            return;
        }
    }

    // 허공에 쐈을 경우 즉시 취소
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void USRGA_Grapple::OnInputReleased(float TimeHeld)
{
    // 유저가 키를 떼면 GA 정상 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_Grapple::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // ⭐️ 피격을 당해서 Cancel(강제 종료)되었든, 키를 뗐든 무조건 실행됩니다.
    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(GetAvatarActorFromActorInfo()))
    {
        PlayerChar->StopGrapple();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}