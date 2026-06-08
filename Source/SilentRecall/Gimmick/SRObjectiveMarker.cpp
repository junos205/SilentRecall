#include "Gimmick/SRObjectiveMarker.h"
#include "Kismet/GameplayStatics.h"
#include "UI/SRObjectiveWidget.h"
#include "GameFramework/Character.h" 

ASRObjectiveMarker::ASRObjectiveMarker()
{
    PrimaryActorTick.bCanEverTick = true;
    bAllowTickBeforeBeginPlay = false;

    // 🟢 [흔들림 방지] 모든 무브먼트 연산이 끝난 최후순위에 틱 가동
    PrimaryActorTick.TickGroup = TG_PostUpdateWork; 

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    ObjectiveWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("ObjectiveWidgetComp"));
    ObjectiveWidgetComp->SetupAttachment(RootComponent);
    
    // 에디터에서 World 혹은 Screen 원하는 스페이스로 세팅하면 동적으로 대응합니다.
    ObjectiveWidgetComp->SetWidgetSpace(EWidgetSpace::World); 
    ObjectiveWidgetComp->SetDrawAtDesiredSize(true);
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

    // Z축 높낮이를 무시하고 평면상 거리만 정밀 정산
    float DistanceToPlayer = FVector::Dist2D(GetActorLocation(), PlayerChar->GetActorLocation());

    // 🟢 [일회성 가드] 도달 시 마스터 스위치를 내려서 영구 소멸
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

    // 🟢 컴포넌트가 월드 스페이스(World)일 때만 수동 빌보드 회전을 실행합니다.
    if (ObjectiveWidgetComp->GetWidgetSpace() == EWidgetSpace::World)
    {
        APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
        if (CamManager)
        {
            FVector CameraLocation = CamManager->GetCameraLocation();
            FVector WidgetLocation = ObjectiveWidgetComp->GetComponentLocation();
            
            // 카메라 주시 각도 계산 (크기 조절 로직은 요청대로 제거되었습니다)
            FRotator LookAtRot = FRotationMatrix::MakeFromX(CameraLocation - WidgetLocation).Rotator();
            ObjectiveWidgetComp->SetWorldRotation(LookAtRot);
        }
    }

    // UI 데이터 수신소로 미터 수치 주입
    if (USRObjectiveWidget* DistanceWidget = Cast<USRObjectiveWidget>(ObjectiveWidgetComp->GetUserWidgetObject()))
    {
        float Meters = DistanceToPlayer / 100.0f;
        DistanceWidget->UpdateDistanceText(Meters);
    }
}