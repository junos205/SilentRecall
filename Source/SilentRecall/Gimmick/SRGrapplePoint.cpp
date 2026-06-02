#include "Gimmick/SRGrapplePoint.h"
#include "Components/WidgetComponent.h"
#include "Components/SphereComponent.h"
#include "Blueprint/UserWidget.h" // 🎯 중요: UUserWidget을 쓰기 위해 필수 추가!

ASRGrapplePoint::ASRGrapplePoint()
{
    // 🎯 1. 틱을 true로 변경하여 프레임 연산을 허용합니다.
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
    
    GrappleWidget->SetWidgetSpace(EWidgetSpace::Screen);
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

// 🎯 3. 매 프레임마다 알파값을 목표값으로 보간합니다.
void ASRGrapplePoint::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // CurrentAlpha를 TargetAlpha(0 또는 1)로 부드럽게 가깝게 만듭니다.
    CurrentAlpha = FMath::FInterpTo(CurrentAlpha, TargetAlpha, DeltaTime, FadeSpeed);

    if (GrappleWidget)
    {
        // 실제 스크린에 그려지는 UserWidget 인스턴스의 투명도를 조절
        if (UUserWidget* UserWidget = GrappleWidget->GetUserWidgetObject())
        {
            UserWidget->SetRenderOpacity(CurrentAlpha);
        }

        // 완전히 투명해졌고, 꺼지는 상태(Target이 0)라면 컴포넌트 자체를 숨겨서 최적화합니다.
        if (TargetAlpha == 0.0f && CurrentAlpha <= 0.01f)
        {
            GrappleWidget->SetVisibility(false);
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