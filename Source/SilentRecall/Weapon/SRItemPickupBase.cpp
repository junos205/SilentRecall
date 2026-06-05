#include "Weapon/SRItemPickupBase.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
// 🌟 무브먼트 컴포넌트들을 사용하기 위한 헤더 포함
#include "GameFramework/RotatingMovementComponent.h"
#include "Components/InterpToMovementComponent.h"
#include "Character/SRPlayerCharacter.h"
#include "Character/SRInventoryComponent.h"

ASRItemPickupBase::ASRItemPickupBase()
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
    RootComponent = CollisionComponent;
    CollisionComponent->SetSphereRadius(60.0f);
    CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));

    BaseVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BaseVFXComponent"));
    BaseVFXComponent->SetupAttachment(RootComponent);

    // 🌟 [1단계] 위치 전담 컴포넌트를 루트 바로 아래에 연결합니다.
    HoverRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HoverRoot"));
    HoverRoot->SetupAttachment(RootComponent);
    HoverRoot->SetRelativeLocation(FVector::ZeroVector); // 초기값은 원점

    // 🌟 [2단계] 회전 전담 컴포넌트를 'HoverRoot'의 자식으로 입적시킵니다. (샌드위치 구조)
    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    VisualRoot->SetupAttachment(HoverRoot); 
    VisualRoot->SetRelativeLocation(FVector::ZeroVector);

    RotatingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));
    // 🌟 회전 컴포넌트는 오직 자식인 VisualRoot만 상시 회전시킵니다.
    RotatingMovement->SetUpdatedComponent(VisualRoot);
    RotatingMovement->RotationRate = FRotator(0.0f, 90.0f, 0.0f);

    InterpToMovement = CreateDefaultSubobject<UInterpToMovementComponent>(TEXT("InterpToMovement"));
    // 🌟 위치 컴포넌트는 회전하지 않는 부모인 HoverRoot를 위아래로 움직입니다.
    InterpToMovement->SetUpdatedComponent(HoverRoot);
    InterpToMovement->BehaviourType = EInterpToBehaviourType::PingPong;
    InterpToMovement->Duration = 1.2f;

    // 초기 에디터 배치용 제어 포인트 (bPositionIsRelative = true)
    InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 30.0f), true));
    InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 45.0f), true));
}

void ASRItemPickupBase::BeginPlay()
{
    Super::BeginPlay();
    
    
    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ASRItemPickupBase::OnOverlapBegin);
}

void ASRItemPickupBase::StartPickupCooldown(float CooldownTime)
{
    bCanPickup = false;
    
    FTimerHandle CooldownTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this, &ASRItemPickupBase::EnablePickup, CooldownTime, false);
}

void ASRItemPickupBase::EnablePickup()
{
    bCanPickup = true;

    // 💡 혹시 콜리전이 잠긴 동안 플레이어가 이미 트리거 공간 안에 서있었다면 
    // 대기했다가 풀리는 순간 즉시 획득하도록 검사합니다.
    TArray<AActor*> OverlappingActors;
    CollisionComponent->GetOverlappingActors(OverlappingActors);
    for (AActor* Actor : OverlappingActors)
    {
        OnOverlapBegin(nullptr, Actor, nullptr, 0, false, FHitResult());
    }
}

void ASRItemPickupBase::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 🚨 [방어선 1] 플래그가 꺼져 있거나(쿨타임 중) OtherActor가 없으면 무조건 즉시 리턴하여 차단!
    if (!OtherActor || !bCanPickup) return;

    if (OtherActor->IsA(ASRPlayerCharacter::StaticClass()))
    {
        USRInventoryComponent* InventoryComp = OtherActor->FindComponentByClass<USRInventoryComponent>();
        if (InventoryComp)
        {
            OnPickedUp(InventoryComp);
        }
    }
}

void ASRItemPickupBase::InitDroppedItem(const FVector& ThrowForce)
{
    bCanPickup = false;

    if (RotatingMovement) RotatingMovement->Deactivate();
    if (InterpToMovement) InterpToMovement->Deactivate();

    if (CollisionComponent)
    {
        CollisionComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
        CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        
        // 1. 변수값 변경
        if (FBodyInstance* BodyInst = CollisionComponent->GetBodyInstance())
        {
            BodyInst->bLockXRotation = true; 
            BodyInst->bLockYRotation = true; 
            BodyInst->bLockZRotation = true; 
        }

        // 🌟 [수정 완료] 가상의 함수 대신, 컴포넌트의 물리 상태를 리빌드하여 제약 조건을 Chaos 엔진에 즉시 주입합니다.
        CollisionComponent->RecreatePhysicsState();

        // 2. 물리 켜고 던지기
        CollisionComponent->SetSimulatePhysics(true);
        CollisionComponent->AddImpulse(ThrowForce, NAME_None, true);
    }

    StartPickupCooldown(1.5f);

    PhysicsCheckCounter = 0;
    GetWorld()->GetTimerManager().SetTimer(PhysicsCheckTimer, this, &ASRItemPickupBase::CheckPhysicsRest, 0.1f, true, 0.5f);
}

void ASRItemPickupBase::CheckPhysicsRest()
{
    PhysicsCheckCounter++;
    
    if (CollisionComponent)
    {
        float CurrentPhysicsSpeed = CollisionComponent->GetComponentVelocity().Size();
        
        // 속도가 거의 0에 가깝거나, 너무 오랫동안 굴러갔다면 (최대 4초 방어선) 안착한 것으로 판정
        if (CurrentPhysicsSpeed < 2.0f || PhysicsCheckCounter > 40)
        {
            GetWorld()->GetTimerManager().ClearTimer(PhysicsCheckTimer);
            ActivateHoverState(); // 호버 모드로 체인지!
        }
    }
}

void ASRItemPickupBase::ActivateHoverState()
{
    if (!CollisionComponent) return;

    CollisionComponent->SetSimulatePhysics(false);
    CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));

    // 1. 변수값 복구
    if (FBodyInstance* BodyInst = CollisionComponent->GetBodyInstance())
    {
        BodyInst->bLockXRotation = false;
        BodyInst->bLockYRotation = false;
        BodyInst->bLockZRotation = false;
    }

    // 🌟 [수정 완료] 잠금을 풀었을 때도 마찬가지로 상태를 리프레시해 주어야 나중에 내장 컴포넌트가 정상 회전합니다.
    CollisionComponent->RecreatePhysicsState();

    // 뒤틀린 각도 최종 세탁
    FRotator CurrentRot = GetActorRotation();
    SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));

    // 계층 좌표계 리셋
    HoverRoot->SetRelativeLocation(FVector::ZeroVector);
    VisualRoot->SetRelativeLocation(FVector::ZeroVector);
    VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);

    // 내장 무브먼트 컴포넌트 재가동
    if (RotatingMovement) 
    {
        RotatingMovement->SetUpdatedComponent(VisualRoot);
        RotatingMovement->Activate(true);
    }
    
    if (InterpToMovement)
    {
        InterpToMovement->ControlPoints.Empty();
        InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 30.0f), true));
        InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 45.0f), true));
        
        InterpToMovement->SetUpdatedComponent(HoverRoot);
        InterpToMovement->Activate(true);
    }
}