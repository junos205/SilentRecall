// Fill out your copyright notice in the Description page of Project Settings.

#include "SRAT_Play1PMontageAndWait.h"
#include "Interface/SRCharacterInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h" 
#include "TimerManager.h"

USRAT_Play1PMontageAndWait* USRAT_Play1PMontageAndWait::CreatePlay1PMontageAndWaitProxy(UGameplayAbility* OwningAbility, FName TaskInstanceName, UAnimMontage* MontageToPlay, float PlayRate)
{
    USRAT_Play1PMontageAndWait* MyObj = NewAbilityTask<USRAT_Play1PMontageAndWait>(OwningAbility, TaskInstanceName);
    MyObj->MontageToPlay = MontageToPlay;
    MyObj->PlayRate = PlayRate;
    return MyObj;
}

void USRAT_Play1PMontageAndWait::Activate()
{
    if (!MontageToPlay)
    {
        OnMontageTimerFinished();
        return;
    }

    float CalculatedDuration = MontageToPlay->GetPlayLength() / FMath::Abs(PlayRate);
    if (CalculatedDuration <= 0.0f) CalculatedDuration = 0.01f;

    AActor* Avatar = GetAvatarActor();
    if (Avatar)
    {
        // 🟢 항상 처음(0.0f)부터 깔끔하게 정방향 재생하도록 단순화
        float StartTime = 0.0f;

        if (ACharacter* OwnerChar = Cast<ACharacter>(Avatar))
        {
            if (USkeletalMeshComponent* Mesh3P = OwnerChar->GetMesh())
            {
                if (UAnimInstance* AnimInst3P = Mesh3P->GetAnimInstance())
                {
                    AnimInst3P->Montage_Play(MontageToPlay, PlayRate, EMontagePlayReturnType::MontageLength, StartTime);
                }
            }
        }

        if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(Avatar))
        {
            if (USkeletalMeshComponent* Mesh1P = CharInterface->Get1PMesh())
            {
                if (UAnimInstance* AnimInst1P = Mesh1P->GetAnimInstance())
                {
                    AnimInst1P->Montage_Play(MontageToPlay, PlayRate, EMontagePlayReturnType::MontageLength, StartTime);
                }
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("[1P태스크 🎬] 장착 몽타주 타이머 기동. 대기 시간: [%.4f초]"), CalculatedDuration);

        Avatar->GetWorld()->GetTimerManager().SetTimer(
            TimerHandle, this, &USRAT_Play1PMontageAndWait::OnMontageTimerFinished, CalculatedDuration, false
        );
    }
    else
    {
        OnMontageTimerFinished();
    }
}

void USRAT_Play1PMontageAndWait::OnMontageTimerFinished()
{
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        OnCompleted.Broadcast();
    }
    EndTask();
}

void USRAT_Play1PMontageAndWait::OnDestroy(bool bInOwnerDestroyed)
{
    AActor* Avatar = GetAvatarActor();
    
    if (Avatar)
    {
        if (Avatar->GetWorld())
        {
            Avatar->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
        }

        if (ACharacter* OwnerChar = Cast<ACharacter>(Avatar))
        {
            if (UAnimInstance* AnimInst3P = OwnerChar->GetMesh()->GetAnimInstance())
            {
                if (AnimInst3P->Montage_IsActive(MontageToPlay)) AnimInst3P->Montage_Stop(0.1f, MontageToPlay);
            }
        }
    }

    if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(Avatar))
    {
        if (USkeletalMeshComponent* Mesh1P = CharInterface->Get1PMesh())
        {
            if (UAnimInstance* AnimInst1P = Mesh1P->GetAnimInstance())
            {
                if (AnimInst1P->Montage_IsActive(MontageToPlay))
                {
                    AnimInst1P->Montage_Stop(0.1f, MontageToPlay);
                }
            }
        }
    }

    Super::OnDestroy(bInOwnerDestroyed);
}