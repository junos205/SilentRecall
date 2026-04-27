#include "SRGA_HitReact.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "NiagaraFunctionLibrary.h"

USRGA_HitReact::USRGA_HitReact()
{
    // 피격 당할 때는 보통 다른 스킬(공격 등)을 강제로 끊어야 하므로 태그 세팅이 필요할 수 있습니다.
    // InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void USRGA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    UE_LOG(LogTemp, Warning, TEXT("[HitReact] Start Hit React from %s"), *GetAvatarActorFromActorInfo()->GetName());
    
    if (!TriggerEventData || !TriggerEventData->Target)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AActor* Victim = const_cast<AActor*>(TriggerEventData->Target.Get());
    FVector VictimLoc = Victim->GetActorLocation();
    FRotator ImpactRotation = FRotator::ZeroRotator;
    
    // ⭐️ 1. 방향 벡터를 구하기 위한 타격점 변수 준비 (기본값은 공격자의 위치로 보험 처리)
    FVector ImpactPoint = VictimLoc; 
    
    // ⭐️ 2. 택배 상자(TargetData) 안에 HitResult가 잘 들어있는지 확인하고 까봅니다.
    if (TriggerEventData->TargetData.IsValid(0))
    {
        const FHitResult* HitResult = TriggerEventData->TargetData.Get(0)->GetHitResult();
        if (HitResult)
        {
            // 진짜 칼이 닿은 정확한 물리 좌표를 가져옵니다!
            ImpactPoint = HitResult->ImpactPoint;
            ImpactRotation = FRotationMatrix::MakeFromX(ImpactPoint).Rotator();
        }
    }

    if (HitNiagaraVFX)
    {
        // 정확한 타격 위치에서, 표면이 튕겨나가는 방향(Normal)으로 이펙트를 터뜨립니다.
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HitNiagaraVFX, ImpactPoint, ImpactRotation);
    }
    
    // 만약 Hit 정보가 없다면? (예: 독 데미지 등) 공격자의 위치를 대신 씁니다.
    else if (TriggerEventData->Instigator)
    {
        ImpactPoint = TriggerEventData->Instigator->GetActorLocation();
    }
    
    // ⭐️ 3. 유저님의 공식: (타격 위치 - 내 위치)
    FVector DirToHit = (ImpactPoint - VictimLoc).GetSafeNormal2D();
    
    FVector VictimForward = Victim->GetActorForwardVector().GetSafeNormal2D();
    FVector VictimRight = Victim->GetActorRightVector().GetSafeNormal2D();

    // 4. 내적 계산!
    float ForwardDot = FVector::DotProduct(VictimForward, DirToHit);
    float RightDot = FVector::DotProduct(VictimRight, DirToHit);

    // 4. 절대값을 비교해서 앞/뒤가 메인인지, 좌/우가 메인인지 판별합니다.
    UAnimMontage* MontageToPlay = nullptr;

    if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
    {
        // 앞/뒤 축이 더 강하다!
        if (ForwardDot > 0.0f)
        {
            MontageToPlay = HitFrontMontage; // 공격자가 내 앞에 있다 (정면 피격)
        }
        else
        {
            MontageToPlay = HitBackMontage;  // 공격자가 내 뒤에 있다 (후면 피격)
        }
    }
    else
    {
        // 좌/우 축이 더 강하다!
        if (RightDot > 0.0f)
        {
            MontageToPlay = HitRightMontage; // 공격자가 내 오른쪽에 있다 (우측 피격)
        }
        else
        {
            MontageToPlay = HitLeftMontage;  // 공격자가 내 왼쪽에 있다 (좌측 피격)
        }
    }

    // 5. 몽타주가 세팅되었다면 재생!
    if (MontageToPlay)
    {
        // 일반 PlayMontage가 아니라 태스크(Task)를 써야 애니메이션 종료 시점을 알 수 있습니다.
        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            FName("HitReactMontage"),
            MontageToPlay
        );

        // 몽타주가 끝나거나, 끊기거나, 취소되면 스킬 종료 함수 호출
        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_HitReact::OnMontageCompleted);
        PlayMontageTask->OnBlendOut.AddDynamic(this, &USRGA_HitReact::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_HitReact::OnMontageCompleted);
        PlayMontageTask->OnCancelled.AddDynamic(this, &USRGA_HitReact::OnMontageCompleted);

        PlayMontageTask->ReadyForActivation();
    }
    else
    {
        // 몽타주가 없다면 바로 종료
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void USRGA_HitReact::OnMontageCompleted()
{
    // 애니메이션이 끝나면 GA를 깔끔하게 종료합니다.
    bool bReplicatedEndAbility = true;
    bool bWasCancelled = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicatedEndAbility, bWasCancelled);
}