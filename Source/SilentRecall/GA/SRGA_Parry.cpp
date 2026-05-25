#include "SRGA_Parry.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Character.h"
#include "Weapon/SRProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h" // ⭐️ 타이머 매니저 헤더 추가
#include "AttributeSet/SRDefaultAttributeSet.h" // 어트리뷰트 셋 헤더 포함 확인

USRGA_Parry::USRGA_Parry()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 패링 활성화 중 상태 태그 자동 부여
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Parry.Active")));
    
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
    // 🎬 1. 패링 시동 몽타주 재생
    // ==========================================================
    if (ParryAnticipationMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ParryAnticipationMontage);
        MontageTask->OnCompleted.AddDynamic(this, &USRGA_Parry::OnAnticipationMontageEnded);
        MontageTask->OnInterrupted.AddDynamic(this, &USRGA_Parry::OnAnticipationMontageEnded);
        MontageTask->OnCancelled.AddDynamic(this, &USRGA_Parry::OnAnticipationMontageEnded);
        MontageTask->ReadyForActivation();
    }

    // ==========================================================
    // 📡 2. ExecCalc로부터 전송될 근접 / 원거리 성공 무전 대기 개시
    // ==========================================================
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this, FGameplayTag::RequestGameplayTag(FName("Event.Character.ParrySuccess")), nullptr, false, true
    );
    EventTask->EventReceived.AddDynamic(this, &USRGA_Parry::OnParryEventReceived);
    EventTask->ReadyForActivation();
}

void USRGA_Parry::OnParryEventReceived(FGameplayEventData Payload)
{
    // 헛스윙 몽타주 즉시 차단
    MontageStop();

    AActor* Avatar = GetAvatarActorFromActorInfo();
    ACharacter* PlayerChar = Cast<ACharacter>(Avatar);
    if (!PlayerChar) return;

    // ==========================================================
    // 🎥 [카메라 및 슬로우 연출 가동]
    // ==========================================================
    if (APlayerController* PC = Cast<APlayerController>(PlayerChar->GetController()))
    {
        if (PC->PlayerCameraManager)
        {
            float CurrentYaw = PC->GetControlRotation().Yaw;
            PC->PlayerCameraManager->ViewYawMin = CurrentYaw;
            PC->PlayerCameraManager->ViewYawMax = CurrentYaw;
        }
    }

    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.2f);

    // ==========================================================
    // 🛡️ [성공 몽타주 기간 무적 처리]
    // ==========================================================
    FGameplayTag InvincibleTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Invincible"));
    GetAbilitySystemComponentFromActorInfo()->AddLooseGameplayTag(InvincibleTag);

    bool bIsMeleeParry = Payload.EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Event.Character.ParrySuccess.Melee")));

    if (bIsMeleeParry)
    {
        // ==========================================================
        // ⚔️ [근접 패링 보상 코스]
        // ==========================================================
        UE_LOG(LogTemp, Warning, TEXT("[ParryGA] 근접 카운터 발동 -> 앞방향 트레이스 스캔 시작"));

        FVector TraceStart = PlayerChar->GetActorLocation();
        FVector TraceEnd = TraceStart + (PlayerChar->GetActorForwardVector() * 350.0f);
        
        FHitResult HitResult;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(PlayerChar);

        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Pawn, Params);

        // ⭐️ [해결 1] Payload.Instigator는 const 속성을 가지므로 const_cast로 안전하게 변환하여 받아옵니다.
        AActor* EnemyActor = bHit ? HitResult.GetActor() : const_cast<AActor*>(Payload.Instigator.Get());
        ACharacter* EnemyChar = Cast<ACharacter>(EnemyActor);

        if (EnemyChar)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ParryGA] 카운터 타겟 검거 성공: %s"), *EnemyChar->GetName());

            FVector LaunchDir = PlayerChar->GetActorForwardVector() * 1800.0f + FVector::UpVector * 400.0f;
            EnemyChar->LaunchCharacter(LaunchDir, true, true);

            // ⭐️ [해결 2] UE 5.6 전용 모던 GAS 수동 버프/데미지 처리 시스템 가동
            // 에디터 에셋 생성 없이 C++ 런타임에서 안전하고 직관적으로 3초 기절 및 데미지를 꼽아넣습니다.
            if (UAbilitySystemComponent* EnemyASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(EnemyChar))
            {
                // 1. 깡데미지 (40) 즉각 누적 반영
                EnemyASC->ApplyModToAttribute(USRDefaultAttributeSet::GetDamageAttribute(), EGameplayModOp::Additive, 40.0f);

                // 2. 3초간 지속될 기절 Loose 태그 수동 주입
                FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun"));
                EnemyASC->AddLooseGameplayTag(StunTag);

                // 3. 약속된 3초 뒤에 기절 태그만 쏙 빼주는 람다(Lambda) 타이머 작동
                FTimerHandle EnemyStunTimerHandle;
                GetWorld()->GetTimerManager().SetTimer(
                    EnemyStunTimerHandle, 
                    FTimerDelegate::CreateWeakLambda(EnemyASC, [EnemyASC, StunTag]()
                    {
                        if (EnemyASC)
                        {
                            EnemyASC->RemoveLooseGameplayTag(StunTag);
                            UE_LOG(LogTemp, Log, TEXT("[ParryGA] 적 기절(Stun) 상태 시간 만료로 인한 해제 완료."));
                        }
                    }), 
                    3.0f, 
                    false
                );
            }
        }

        if (ParrySuccessMontage)
        {
            UAbilityTask_PlayMontageAndWait* SuccessTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ParrySuccessMontage);
            SuccessTask->OnCompleted.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
            SuccessTask->OnInterrupted.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
            SuccessTask->OnCancelled.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
            SuccessTask->ReadyForActivation();
            return;
        }
    }
    else
    {
        // ==========================================================
        // 🏓 [원거리 패링 보상 코스]
        // ==========================================================
        // ⭐️ [해결 3] TObjectPtr은 스마트 포인터가 아니므로 IsValid()가 없고, 일반 bool 조건문으로 null 체크를 수행해야 합니다.
        if (Payload.OptionalObject)
        {
            // ⭐️ [해결 4] OptionalObject는 const UObject* 이므로 const_cast를 통해 투사체 인스턴스로 안전하게 복구합니다.
            ASRProjectile* BlockedProjectile = Cast<ASRProjectile>(const_cast<UObject*>(Payload.OptionalObject.Get()));
            if (BlockedProjectile)
            {
                UE_LOG(LogTemp, Warning, TEXT("[ParryGA] 원거리 투사체 반사 시전!"));
                BlockedProjectile->DeflectProjectile(PlayerChar);
            }
        }

        if (ParrySuccessMontage)
        {
            UAbilityTask_PlayMontageAndWait* SuccessTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ParrySuccessMontage);
            SuccessTask->OnCompleted.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
            SuccessTask->OnInterrupted.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
            SuccessTask->OnCancelled.AddDynamic(this, &USRGA_Parry::OnSuccessMontageEnded);
            SuccessTask->ReadyForActivation();
            return;
        }
    }

    OnSuccessMontageEnded();
}

void USRGA_Parry::OnAnticipationMontageEnded()
{
    bool bReplicateEndAbility = true;
    bool bWasCancelled = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USRGA_Parry::OnSuccessMontageEnded()
{
    bool bReplicateEndAbility = true;
    bool bWasCancelled = false;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USRGA_Parry::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    ACharacter* PlayerChar = Cast<ACharacter>(Avatar);

    if (PlayerChar)
    {
        if (APlayerController* PC = Cast<APlayerController>(PlayerChar->GetController()))
        {
            if (PC->PlayerCameraManager)
            {
                PC->PlayerCameraManager->ViewYawMin = 0.0f;
                PC->PlayerCameraManager->ViewYawMax = 359.999f;
            }
        }
    }

    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);

    FGameplayTag InvincibleTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Invincible"));
    GetAbilitySystemComponentFromActorInfo()->RemoveLooseGameplayTag(InvincibleTag);

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}