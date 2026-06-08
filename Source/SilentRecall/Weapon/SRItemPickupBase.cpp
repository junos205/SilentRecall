#include "Weapon/SRItemPickupBase.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
// 🌟 무브먼트 컴포넌트들을 사용하기 위한 헤더 포함
#include "NiagaraFunctionLibrary.h"
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
    if (!OtherActor || !bCanPickup) return;

    if (OtherActor->IsA(ASRPlayerCharacter::StaticClass()))
    {
        USRInventoryComponent* InventoryComp = OtherActor->FindComponentByClass<USRInventoryComponent>();
        if (InventoryComp)
        {
            // ==========================================================
            // ✨ [신규 추가] 아이템 획득 성공 시 이펙트 소환
            // ==========================================================
            if (PickupVFX)
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), PickupVFX, GetActorLocation(), GetActorRotation());
            }
            // ==========================================================

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
        
        // 🔴 원래 있던 Pawn 무시에 더해, 아래의 결정타 한 줄을 추가합니다.
        CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        
        // =======================================================================
        // 🛡️ [치트키] 래그돌 시체 채널(PhysicsBody)을 통째로 무시(Ignore)합니다!
        // 이로써 적의 몸뚱아리, 사지, 총과 완벽히 겹쳐도 절대 물리 폭발이 일어나지 않습니다.
        // =======================================================================
        CollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
        // =======================================================================

        if (FBodyInstance* BodyInst = CollisionComponent->GetBodyInstance())
        {
            BodyInst->bLockXRotation = true; 
            BodyInst->bLockYRotation = true; 
            BodyInst->bLockZRotation = true; 
            BodyInst->SetMaxDepenetrationVelocity(300.0f);
        }

        // 카오스 엔진에 무시 설정 즉시 주입
        CollisionComponent->RecreatePhysicsState();

        // 물리 켜고 던지기 (이제 바닥/벽에만 부딪히며 부드럽게 날아갑니다)
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

    // 1. 물리 종료 및 트리거 전환 (가만히 멈춘 그 상태로 락)
    CollisionComponent->SetSimulatePhysics(false);
    CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));

    // =======================================================================
    // ❌ [스냅 주범 1 제거] 누워있던 각도를 즉시 수평 리셋하던 코드를 과감히 삭제합니다!
    // 이 변환 연산이 사라지면서 시계추처럼 메쉬가 맵에서 튀는 현상이 완전히 사라집니다.
    // =======================================================================
    // FRotator CurrentRot = GetActorRotation();
    // SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
    // =======================================================================

    // 2. 현재 안착한 상태 그대로 피벗 정렬 유지
    AdjustVisualOffset(); 
    VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);

    // 3. 회전 컴포넌트 가동 (안착한 기울기 축을 기준으로 부드럽게 자전 시작)
    if (RotatingMovement) 
    {
        RotatingMovement->SetUpdatedComponent(VisualRoot);
        RotatingMovement->Activate(true);
    }
    
    // 4. 🌟 [스냅 주범 2 진압 - InterpToMovement 정산 강제 통제]
    if (InterpToMovement)
    {
        InterpToMovement->ControlPoints.Empty();
        
        // 🟢 안착한 그 상태(0.0f)를 완벽한 시작점으로 잡습니다.
        InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 0.0f), true));
        
        // 🟢 안착한 면에서 로컬 위쪽 방향으로 가볍게 15cm 정도만 연출용 왕복 운동을 주입합니다.
        InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 15.0f), true));
        
        // =======================================================================
        // 📡 [결정타] 데이터 갱신을 카오스/무브먼트 시스템에 정식으로 컴파일 공지합니다!
        // 이 코드가 들어가야 첫 프레임에 생성자 수치(30cm)로 강제 워프하는 버그가 박멸됩니다.
        // =======================================================================
        InterpToMovement->FinaliseControlPoints();
        // =======================================================================
        
        InterpToMovement->SetUpdatedComponent(HoverRoot);
        InterpToMovement->Activate(true);
    }
}