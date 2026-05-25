#include "SRAT_MoveToTransform.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/CapsuleComponent.h"

USRAT_MoveToTransform::USRAT_MoveToTransform(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bTickingTask = true; 
    bIsMoving = false;
    TimeElapsed = 0.0f;
    ExecutionTargetActor = nullptr;
}

USRAT_MoveToTransform* USRAT_MoveToTransform::SRMoveToTransform(UGameplayAbility* OwningAbility, FVector TargetLocation, FRotator TargetRotation, AActor* TargetActor, float AbsoluteGroundZ, float Duration)
{
    USRAT_MoveToTransform* MyTask = NewAbilityTask<USRAT_MoveToTransform>(OwningAbility);
    if (MyTask)
    {
        MyTask->GoalLocation = TargetLocation;
        MyTask->GoalRotation = TargetRotation;
        MyTask->ExecutionTargetActor = TargetActor; // ⭐️ 적 액터 저장
        MyTask->GroundZ = AbsoluteGroundZ;
        MyTask->MoveDuration = FMath::Max(Duration, 0.001f);
    }
    return MyTask;
}

void USRAT_MoveToTransform::Activate()
{
    Super::Activate();

    if (ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActor()))
    {
        StartLocation = AvatarChar->GetActorLocation();
        if (APlayerController* PC = Cast<APlayerController>(AvatarChar->GetController()))
        {
            StartRotation = PC->GetControlRotation(); 
        }
    }
    
    bIsMoving = true;
    TimeElapsed = 0.0f;
}

void USRAT_MoveToTransform::TickTask(float DeltaTime)
{
    Super::TickTask(DeltaTime);

    if (!bIsMoving) return;

    TimeElapsed += DeltaTime;
    float Progress = FMath::Clamp(TimeElapsed / MoveDuration, 0.0f, 1.0f);
    float Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, Progress, 2.0f);

    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActor());
    if (AvatarChar)
    {
        // 1. 플레이어 이동 처리
        FVector NewLocation = FMath::Lerp(StartLocation, GoalLocation, Alpha);
        FRotator NewRotation = FMath::Lerp(AvatarChar->GetActorRotation(), GoalRotation, Alpha);
        AvatarChar->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

        // 2. ⚡️ [화면 뒤집힘 버그 완벽 수정 구역]
        if (APlayerController* PC = Cast<APlayerController>(AvatarChar->GetController()))
        {
            FVector CameraLocation = NewLocation + FVector(0, 0, AvatarChar->BaseEyeHeight);
    
            FVector TargetCenter = GoalLocation; // 폴백용
            if (ExecutionTargetActor)
            {
                // 적의 골반 중심 좌표를 가져온 뒤
                TargetCenter = ExecutionTargetActor->GetActorLocation(); 
        
                // ⭐️ [시선 상향 보정] 적의 목~머리 방향을 확실하게 올려다보도록 +45.0f를 더해줍니다!
                // 구도를 더 위로 꺾고 싶다면 이 수치를 60.0f 등으로 더 높이시면 됩니다.
                TargetCenter.Z += 45.0f; 
            }

            FRotator TargetCamRot = (TargetCenter - CameraLocation).Rotation();

            FQuat StartQuat = FQuat(StartRotation);
            FQuat TargetQuat = FQuat(TargetCamRot);
            FQuat BlendedQuat = FQuat::Slerp(StartQuat, TargetQuat, Alpha);

            PC->SetControlRotation(BlendedQuat.Rotator());
        }
    }

    if (Progress >= 1.0f)
    {
        bIsMoving = false;
        if (ShouldBroadcastAbilityTaskDelegates())
        {
            OnTargetLocationReached.Broadcast();
        }
        EndTask();
    }
}

void USRAT_MoveToTransform::OnDestroy(bool bInOwnerFinished)
{
    Super::OnDestroy(bInOwnerFinished);
}