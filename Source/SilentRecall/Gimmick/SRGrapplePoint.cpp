#include "Gimmick/SRGrapplePoint.h"
#include "Components/WidgetComponent.h"
#include "Components/SphereComponent.h"
#include "Blueprint/UserWidget.h" // 🎯 중요: UUserWidget을 쓰기 위해 필수 추가!

ASRGrapplePoint::ASRGrapplePoint()
{
    PrimaryActorTick.bCanEverTick = true;

    SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
    RootComponent = SphereComponent;
    
    SphereComponent->SetSphereRadius(50.f); 
    SphereComponent->SetCollisionProfileName(TEXT("Custom"));
    SphereComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SphereComponent->SetCollisionObjectType(ECC_WorldStatic);
    SphereComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    GrappleWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("GrappleWidget"));
    GrappleWidget->SetupAttachment(RootComponent);
    
    // 🌟 [수정 1] 월드 공간으로 변경하여 글로우 머티리얼 가속을 허용합니다.
    GrappleWidget->SetWidgetSpace(EWidgetSpace::World);
    
    // 🌟 [수정 2] 월드 공간 위젯의 픽셀 해상도 세팅 (원형 아이콘 크기에 맞춤)
    GrappleWidget->SetDrawSize(FVector2D(250.0f, 250.0f));
    
    // 🌟 [수정 3] 250cm는 인게임에서 너무 거대하므로, 스케일을 역으로 줄여서 컴팩트하게 만듭니다. (250 * 0.15 = 37.5cm 크기)
    GrappleWidget->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.15f));

    GrappleWidget->SetVisibility(false);
    Tags.Add(FName("GrappleTarget"));
}

void ASRGrapplePoint::BeginPlay()
{
    Super::BeginPlay();
    
    if (GrappleWidget)
    {
       GrappleWidget->SetVisibility(false);

       // 🎯 2. 시작 프레임에 내부 UI의 투명도를 0으로 완전히 숨깁니다.
       if (UUserWidget* UserWidget = GrappleWidget->GetUserWidgetObject())
       {
          UserWidget->SetRenderOpacity(0.0f);
       }
    }
}

void ASRGrapplePoint::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // (기존 TargetAlpha 및 투명도 SetRenderOpacity 보간 로직 유지)
    CurrentAlpha = FMath::FInterpTo(CurrentAlpha, TargetAlpha, DeltaTime, FadeSpeed);

    if (GrappleWidget)
    {
        if (UUserWidget* UserWidget = GrappleWidget->GetUserWidgetObject())
        {
            UserWidget->SetRenderOpacity(CurrentAlpha);
        }

        if (TargetAlpha == 0.0f && CurrentAlpha <= 0.01f)
        {
            GrappleWidget->SetVisibility(false);
        }
        
        // =======================================================================
        // 🌟 [추가] 실시간 카메라 락온 회전 (World Space 빌보드 쉴드 가동)
        // =======================================================================
        // 위젯이 눈에 보이고 있을 때만 회전 연산을 돌려 CPU를 최적화합니다.
        if (GrappleWidget->IsVisible())
        {
            APlayerController* PC = GetWorld()->GetFirstPlayerController();
            if (PC && PC->PlayerCameraManager)
            {
                // 1. 현재 실시간 카메라 렌즈의 월드 좌표와 위젯의 월드 좌표를 확보합니다.
                FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
                FVector WidgetLocation = GrappleWidget->GetComponentLocation();
                
                // 2. 위젯 원점에서 카메라 렌즈를 정확히 겨냥하는 시선 각도(Rotation)를 계산합니다.
                FRotator BillboardRotation = (CameraLocation - WidgetLocation).Rotation();
                
                // 3. 계산된 시선 각도를 주입하여 플레이어가 어디로 가든 항상 앞면만 보이게 고정합니다.
                GrappleWidget->SetWorldRotation(BillboardRotation);
            }
        }
    }
}

// 🎯 4. 상태 제어 함수 변경
void ASRGrapplePoint::SetWidgetActive(bool bActivate)
{
    if (bActivate)
    {
        TargetAlpha = 1.0f;
        if (GrappleWidget)
        {
            // 페이드 인 연출을 보여주기 위해 가시성을 즉시 켜줍니다.
            GrappleWidget->SetVisibility(true);
        }
    }
    else
    {
        // 즉시 비활성화하지 않고 목표치만 0으로 낮춰서 자연스럽게 사라지게 유도합니다.
        TargetAlpha = 0.0f;
    }
}