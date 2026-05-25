#include "SRGA_GloryKill.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GA/AT/SRAT_MoveToTransform.h" // ⭐️ 우리가 새로 만든 커스텀 이동 태스크 헤더 포함!
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "Character/SRInventoryComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/SpringArmComponent.h"

USRGA_GloryKill::USRGA_GloryKill()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bIsRotatingCamera = false;
    CurrentVictim = nullptr;

    FGameplayTagContainer TempTags;
    TempTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.GloryKill")));
    SetAssetTags(TempTags);

    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Buff.Invincible")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.GloryKill")));

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

    CurrentVictim = FindExecutionTarget();

    if (CurrentVictim)
    {
        PlayExecution(CurrentVictim);
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

AActor* USRGA_GloryKill::FindExecutionTarget()
{
    ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Avatar) return nullptr;

    FVector StartLoc = Avatar->GetActorLocation();
    FVector LookDir = Avatar->GetBaseAimRotation().Vector();
    LookDir.Normalize();

    TArray<FHitResult> HitResults;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(300.0f);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Avatar);

    bool bHit = GetWorld()->SweepMultiByChannel(HitResults, StartLoc, StartLoc + (LookDir * 100.0f), FQuat::Identity, ECC_Pawn, SphereShape, QueryParams);

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
            float DotProduct = FVector::DotProduct(LookDir, DirToTarget);
            
            if (DotProduct > 0.6f) 
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

void USRGA_GloryKill::PlayExecution(AActor* TargetActor)
{
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (TargetASC)
    {
        TargetASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    }

    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    ACharacter* TargetChar = Cast<ACharacter>(TargetActor);

    if (AvatarChar && TargetChar)
    {
        // 1. 시간 왜곡 슬로우 모션 발동
        UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.2f);
        AvatarChar->CustomTimeDilation = 5.0f;
        TargetChar->CustomTimeDilation = 5.0f;

        // 플레이어 제어 무조건 차단
        if (APlayerController* PC = Cast<APlayerController>(AvatarChar->GetController()))
        {
            AvatarChar->bUseControllerRotationYaw = false;
            PC->SetIgnoreLookInput(true);
            PC->SetIgnoreMoveInput(true);
        }

        // 스프링암 물리 충돌 연산 전면 차단
        if (USpringArmComponent* SpringArm = AvatarChar->FindComponentByClass<USpringArmComponent>())
        {
            SpringArm->bDoCollisionTest = false; 
        }

        // 2. 무브먼트 연산 자체를 완전히 정지 (커스텀 태스크가 위치를 강제 제어할 예정이므로 잠금)
        if (UCharacterMovementComponent* MoveComp = AvatarChar->GetCharacterMovement())
        {
            MoveComp->StopMovementImmediately(); 
            MoveComp->DisableMovement(); // 물리 브레이크 상태로 전환
        }

        // 3. 콜리전 충돌 완벽 무력화 세팅
        if (AvatarChar->GetCapsuleComponent())
        {
            AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
            AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
            AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
            AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        }

        if (TargetChar->GetCapsuleComponent())
        {
            TargetChar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            if (TargetChar->GetCharacterMovement())
            {
                TargetChar->GetCharacterMovement()->StopMovementImmediately(); 
                TargetChar->GetCharacterMovement()->DisableMovement();
            }
        }

        // 4. 인벤토리 무기 숨기기
        if (USRInventoryComponent* InvComp = AvatarChar->FindComponentByClass<USRInventoryComponent>())
        {
            InvComp->SetCurrentActiveWeaponVisibility(false);
        }

        // 6. 지면 높이 판별 및 모션 워핑 대체 커스텀 태스크 좌표 조립 구역
        float FinalGroundZ = TargetChar->GetActorLocation().Z; 

        FHitResult GroundHit;
        FVector TraceStart = TargetChar->GetActorLocation();
        FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, 1000.0f); 
        FCollisionQueryParams TraceParams;
        TraceParams.AddIgnoredActor(AvatarChar);
        TraceParams.AddIgnoredActor(TargetChar);

        if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
        {
            FinalGroundZ = GroundHit.ImpactPoint.Z; 
        }

        // 적 위치 다운 보정 (+2.0f 초밀착 정렬)
        FVector TargetResetLoc = TargetChar->GetActorLocation();
        TargetResetLoc.Z = FinalGroundZ + TargetChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f;
        TargetChar->SetActorLocation(TargetResetLoc);

        // 플레이어의 최종 안착 목적지 좌표 계산
        FVector DirFromAttacker = (TargetChar->GetActorLocation() - AvatarChar->GetActorLocation()).GetSafeNormal2D();
        FVector GoalWarpLocation = TargetChar->GetActorLocation() - (DirFromAttacker * 110.0f);
        
        // ⭐️ [수정 완료] GetCapsuleComponent()를 안전하게 거쳐서 절반 높이를 더하도록 고쳤습니다.
        if (AvatarChar->GetCapsuleComponent())
        {
            GoalWarpLocation.Z = FinalGroundZ + AvatarChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        }
        else
        {
            GoalWarpLocation.Z = FinalGroundZ + 88.0f;
        }

        FRotator GoalWarpRotation = DirFromAttacker.Rotation();
        GoalWarpRotation.Pitch = 0.0f;
        GoalWarpRotation.Roll = 0.0f;

        // 태스크 호출부
        USRAT_MoveToTransform* MoveTask = USRAT_MoveToTransform::SRMoveToTransform(this, GoalWarpLocation, GoalWarpRotation, TargetChar, FinalGroundZ, 0.15f);
        MoveTask->ReadyForActivation();
    }

    // 7. 타격 노티파이 대기 태스크
    FGameplayTag ExecuteHitTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.GloryKill.Execute"));
    UAbilityTask_WaitGameplayEvent* WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ExecuteHitTag);
    WaitHitTask->EventReceived.AddDynamic(this, &USRGA_GloryKill::OnExecuteHitNotifyReceived);
    WaitHitTask->ReadyForActivation();

    // 8. 몽타주 재생 기동
    if (AttackerMontage)
    {
        UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AttackerMontage, 1.0f);
        PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_GloryKill::OnMontageCompleted);
        PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_GloryKill::OnMontageCompleted);
        PlayMontageTask->ReadyForActivation();
    }

    if (TargetChar && VictimMontage)
    {
        TargetChar->PlayAnimMontage(VictimMontage);
    }
}

// ⭐️ 수동 보간 타이머 함수는 자취를 감추고 완전히 공백 처리 (Task 내부로 기능 대통합)
void USRGA_GloryKill::UpdateCameraRotation()
{
}

void USRGA_GloryKill::OnExecuteHitNotifyReceived(FGameplayEventData Payload)
{
    if (!CurrentVictim || !ExecutionDamageEffect) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CurrentVictim);
    UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo();

    if (TargetASC && MyASC)
    {
        // 1. 데미지 적용 전 적의 진짜 체력 실측
        float HealthBefore = TargetASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());

        FGameplayEffectContextHandle ContextHandle = MyASC->MakeEffectContext();
        ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
        
        int32 AppliedLevel = FMath::Max(1, GetAbilityLevel());
        FGameplayEffectSpecHandle SpecHandle = MyASC->MakeOutgoingSpec(ExecutionDamageEffect, AppliedLevel, ContextHandle);
        
        if (SpecHandle.IsValid())
        {
            // GE 적용 명령 시전 (Instant 이펙트는 내부적으로 여기서 체력을 깎고 끝납니다)
            TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            
            // 2. 데미지 적용 후 적의 진짜 체력 재측정
            float HealthAfter = TargetASC->GetNumericAttribute(USRDefaultAttributeSet::GetHealthAttribute());

            // 3. 비포 애프터 비교를 통한 팩트 체크 로그
            UE_LOG(LogTemp, Warning, TEXT("[GloryKill] -----------------------------------------"));
            UE_LOG(LogTemp, Warning, TEXT("[GloryKill] 데미지 타격 노티파이 발동 성공!"));
            UE_LOG(LogTemp, Warning, TEXT("[GloryKill] 적용 전 적 체력: %f"), HealthBefore);
            UE_LOG(LogTemp, Warning, TEXT("[GloryKill] 적용 후 적 체력: %f"), HealthAfter);
            
            if (HealthAfter < HealthBefore)
            {
                UE_LOG(LogTemp, Log, TEXT("[GloryKill] 결과: 체력이 정상적으로 감소했습니다! 데미지 적용 완벽 성공."));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[GloryKill] 결과: 체력 변화 없음! GE 내부에 Modifier(데미지 연산)가 누락되었거나 수치가 0인지 확인하세요."));
            }
            UE_LOG(LogTemp, Warning, TEXT("[GloryKill] -----------------------------------------"));
        }
    }
}

void USRGA_GloryKill::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_GloryKill::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    ACharacter* TargetChar = Cast<ACharacter>(CurrentVictim);

    bIsRotatingCamera = false;

    if (AvatarChar)
    {
        if (AvatarChar->GetCapsuleComponent()) 
        {
            AvatarChar->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        }
        
        AvatarChar->CustomTimeDilation = 1.0f;
        AvatarChar->bUseControllerRotationYaw = true;
        
        if (APlayerController* PC = Cast<APlayerController>(AvatarChar->GetController()))
        {
            PC->ResetIgnoreLookInput();
            PC->ResetIgnoreMoveInput();
        }

        if (USpringArmComponent* SpringArm = AvatarChar->FindComponentByClass<USpringArmComponent>())
        {
            SpringArm->bDoCollisionTest = true; 
        }

        if (USRInventoryComponent* InvComp = AvatarChar->FindComponentByClass<USRInventoryComponent>())
        {
            InvComp->SetCurrentActiveWeaponVisibility(true);
        }

        // 물리 잠금 해제 및 원래 낙하/걷기 상태 복원
        if (UCharacterMovementComponent* MoveComp = AvatarChar->GetCharacterMovement())
        {
            MoveComp->SetMovementMode(MOVE_Falling);
        }
    }

    if (TargetChar)
    {
        TargetChar->CustomTimeDilation = 1.0f;
        
        if (TargetChar->GetCapsuleComponent()) 
        {
            TargetChar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            TargetChar->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
        }
        
        if (TargetChar->GetCharacterMovement())
        {
            TargetChar->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        }
    }

    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}