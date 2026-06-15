#include "Weapon/SRItemPickupBase.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Components/InterpToMovementComponent.h"
#include "Character/SRPlayerCharacter.h"
#include "Character/SRInventoryComponent.h"
#include "Kismet/GameplayStatics.h" // 🔊 사운드 재생을 위한 헤더 추가

ASRItemPickupBase::ASRItemPickupBase()
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
    RootComponent = CollisionComponent;
    CollisionComponent->SetSphereRadius(60.0f);
    CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));

    BaseVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BaseVFXComponent"));
    BaseVFXComponent->SetupAttachment(RootComponent);

    HoverRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HoverRoot"));
    HoverRoot->SetupAttachment(RootComponent);
    HoverRoot->SetRelativeLocation(FVector::ZeroVector); 

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    VisualRoot->SetupAttachment(HoverRoot); 
    VisualRoot->SetRelativeLocation(FVector::ZeroVector);

    RotatingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));
    RotatingMovement->SetUpdatedComponent(VisualRoot);
    RotatingMovement->RotationRate = FRotator(0.0f, 90.0f, 0.0f);

    InterpToMovement = CreateDefaultSubobject<UInterpToMovementComponent>(TEXT("InterpToMovement"));
    InterpToMovement->SetUpdatedComponent(HoverRoot);
    InterpToMovement->BehaviourType = EInterpToBehaviourType::PingPong;
    InterpToMovement->Duration = 1.2f;

    InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 30.0f), true));
    InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 45.0f), true));
}

void ASRItemPickupBase::BeginPlay()
{
    Super::BeginPlay();
    bCanPickup = true; 
    AdjustVisualOffset();
    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ASRItemPickupBase::OnOverlapBegin);
}

void ASRItemPickupBase::AdjustVisualOffset()
{
    UMeshComponent* MeshComp = FindComponentByClass<UMeshComponent>();
    if (MeshComp && VisualRoot)
    {
        FBoxSphereBounds LocalBounds = MeshComp->CalcBounds(FTransform::Identity);
        FBox LocalBox = LocalBounds.GetBox();
        
        float MeshMinZ = LocalBox.Min.Z * MeshComp->GetRelativeScale3D().Z;
        VisualRoot->SetRelativeLocation(FVector(0.0f, 0.0f, -MeshMinZ));
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
            // ✨ 아이템 획득 성공 시 이펙트 소환
            if (PickupVFX)
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), PickupVFX, GetActorLocation(), GetActorRotation());
            }

            // =======================================================================
            // 🔊 [신규 추가] 아이템 획득 성공 시 실제 획득 오디오 사운드 재생
            // =======================================================================
            if (PickupSound)
            {
                UGameplayStatics::PlaySoundAtLocation(GetWorld(), PickupSound, GetActorLocation());
            }
            // =======================================================================

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
        CollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);

        if (FBodyInstance* BodyInst = CollisionComponent->GetBodyInstance())
        {
            BodyInst->bLockXRotation = true; 
            BodyInst->bLockYRotation = true; 
            BodyInst->bLockZRotation = true; 
            BodyInst->SetMaxDepenetrationVelocity(300.0f);
        }

        CollisionComponent->RecreatePhysicsState();
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
        if (CurrentPhysicsSpeed < 2.0f || PhysicsCheckCounter > 40)
        {
            GetWorld()->GetTimerManager().ClearTimer(PhysicsCheckTimer);
            ActivateHoverState(); 
        }
    }
}

void ASRItemPickupBase::ActivateHoverState()
{
    if (!CollisionComponent) return;

    CollisionComponent->SetSimulatePhysics(false);
    CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));

    AdjustVisualOffset(); 
    VisualRoot->SetRelativeRotation(FRotator::ZeroRotator);

    if (RotatingMovement) 
    {
        RotatingMovement->SetUpdatedComponent(VisualRoot);
        RotatingMovement->Activate(true);
    }
    
    if (InterpToMovement)
    {
        InterpToMovement->ControlPoints.Empty();
        InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 0.0f), true));
        InterpToMovement->ControlPoints.Add(FInterpControlPoint(FVector(0.0f, 0.0f, 15.0f), true));
        InterpToMovement->FinaliseControlPoints();
        
        InterpToMovement->SetUpdatedComponent(HoverRoot);
        InterpToMovement->Activate(true);
    }
}