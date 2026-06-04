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
    bCanPickup = true; 
    
    // 🌟 [추가] 맵에 배치된 채로 게임이 시작될 때 즉시 최하단 피벗 정렬 가동!
    AdjustVisualOffset();
    
    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ASRItemPickupBase::OnOverlapBegin);
}

void ASRItemPickupBase::AdjustVisualOffset()
{
    UMeshComponent* MeshComp = FindComponentByClass<UMeshComponent>();
    
    if (MeshComp && VisualRoot)
    {
        // 🌟 [최종 해결책] 엔진 버전별 GetLocalBounds의 혼선을 완벽하게 우회합니다.
        // 메쉬 컴포넌트에 '기본 로컬 좌표계(Identity)'를 넘겨 순수한 로컬 기준 Bounds를 계산해냅니다.
        FBoxSphereBounds LocalBounds = MeshComp->CalcBounds(FTransform::Identity);
        
        // FBoxSphereBounds 구조체에서 정식 명시적 함수인 .GetBox()를 통해 FBox를 안전하게 추출합니다.
        FBox LocalBox = LocalBounds.GetBox();
        
        FVector LocalMin = LocalBox.Min;
        FVector LocalMax = LocalBox.Max;
        
        // 기존 축 보정 수식 동일 적용
        float MeshMinZ = LocalMin.Z * MeshComp->GetRelativeScale3D().Z;
        
        // 피벗 보정을 위해 VisualRoot의 상대 위치 한 프레임 조절
        VisualRoot->SetRelativeLocation(FVector(0.0f, 0.0f, -MeshMinZ));
        
        UE_LOG(LogTemp, Log, TEXT("[PivotFix] %s 의 최하단 오프셋 보정 완료: %f cm 인상"), *GetName(), -MeshMinZ);
    }
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

    // 1. 물리 종료 및 트리거 전환
    CollisionComponent->SetSimulatePhysics(false);
    CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));

    // 2. 누워있던 각도 수평 정렬
    FRotator CurrentRot = GetActorRotation();
    SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));

    // 3. 동적 바운드 계산으로 피벗 세탁 (칼날 투과 방지)
    HoverRoot->SetRelativeLocation(FVector::ZeroVector);
    AdjustVisualOffset(); 
    VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);

    // 4. 회전 컴포넌트 가동
    if (RotatingMovement) 
    {
        RotatingMovement->SetUpdatedComponent(VisualRoot);
        RotatingMovement->Activate(true);
    }
    
    // 5. 🌟 [스냅 현상 완벽 진압 구역]
    if (InterpToMovement)
    {
        InterpToMovement->ControlPoints.Empty();
        
        // 🟢 [핵심 변경] 첫 포인트를 30.0f가 아닌 0.0f(현재 안착한 바닥면 그 자체)로 지정합니다!
        // 이렇게 하면 컴포넌트가 켜질 때 1픽셀도 순간이동하지 않고 그 자리에서 대기합니다.
        InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 0.0f), true));
        
        // 🟢 최고 높이를 20.0f~25.0f 정도로 잡아줍니다.
        InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 20.0f), true));
        
        InterpToMovement->SetUpdatedComponent(HoverRoot);
        InterpToMovement->Activate(true);
    }
}