#include "Gimmick/SRObjectiveMarker.h"
#include "Kismet/GameplayStatics.h"
#include "UI/SRObjectiveWidget.h"
#include "GameFramework/Character.h" 

ASRObjectiveMarker::ASRObjectiveMarker()
{
    PrimaryActorTick.bCanEverTick = true;
    bAllowTickBeforeBeginPlay = false;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork; 

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    ObjectiveWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("ObjectiveWidgetComp"));
    ObjectiveWidgetComp->SetupAttachment(RootComponent);
    
    ObjectiveWidgetComp->SetWidgetSpace(EWidgetSpace::World); 
    ObjectiveWidgetComp->SetDrawAtDesiredSize(true);

    // 🌟 [완치] 레벨에서 액터 크기를 제무리 늘려도 UI가 찌그러지지 않도록 절대 스케일 가동!
    ObjectiveWidgetComp->SetUsingAbsoluteScale(true);
}

void ASRObjectiveMarker::BeginPlay()
{
    Super::BeginPlay();
    if (ObjectiveWidgetComp) ObjectiveWidgetComp->SetVisibility(false);
    SetActorHiddenInGame(true);
    SetActorTickEnabled(false);
    bIsActive = false;
}

void ASRObjectiveMarker::ActivateMarker()
{
    bIsActive = true; 
    SetActorHiddenInGame(false);
    SetActorTickEnabled(true);
    if (ObjectiveWidgetComp) ObjectiveWidgetComp->SetVisibility(true);
}

void ASRObjectiveMarker::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsActive || !ObjectiveWidgetComp) return;

    AActor* PlayerChar = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    if (!PlayerChar) return;

    float DistanceToPlayer = FVector::Dist2D(GetActorLocation(), PlayerChar->GetActorLocation());

    if (DistanceToPlayer <= HideDistance)
    {
        ObjectiveWidgetComp->SetVisibility(false);
        SetActorHiddenInGame(true);
        SetActorTickEnabled(false);
        bIsActive = false; 
        return; 
    }
    else
    {
        if (!ObjectiveWidgetComp->IsVisible()) ObjectiveWidgetComp->SetVisibility(true);
    }

    if (ObjectiveWidgetComp->GetWidgetSpace() == EWidgetSpace::World)
    {
        APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
        if (CamManager)
        {
            FVector CameraLocation = CamManager->GetCameraLocation();
            FVector WidgetLocation = ObjectiveWidgetComp->GetComponentLocation();
            
            FRotator LookAtRot = FRotationMatrix::MakeFromX(CameraLocation - WidgetLocation).Rotator();
            ObjectiveWidgetComp->SetWorldRotation(LookAtRot);

            // 🌟 [신규 추가] 멀어질수록 화면에서 너무 무식하게 커보이지 않도록 거리별 다이내믹 스케일 제어
            // 거리가 멀어질수록 가중치를 두어 작아지게 만들고, 너무 작아지거나 커지지 않게 0.3 ~ 1.0 사이로 캡핑합니다.
            float ReferenceDistance = 2000.0f; 
            float TargetScale = FMath::Clamp(ReferenceDistance / DistanceToPlayer, 0.3f, 1.0f);
            ObjectiveWidgetComp->SetWorldScale3D(FVector(TargetScale));
        }
    }

    if (USRObjectiveWidget* DistanceWidget = Cast<USRObjectiveWidget>(ObjectiveWidgetComp->GetUserWidgetObject()))
    {
        float Meters = DistanceToPlayer / 100.0f;
        DistanceWidget->UpdateDistanceText(Meters);
    }
}