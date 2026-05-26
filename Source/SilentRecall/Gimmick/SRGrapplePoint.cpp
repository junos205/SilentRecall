#include "SRGrapplePoint.h"
#include "Components/WidgetComponent.h"

ASRGrapplePoint::ASRGrapplePoint()
{
	PrimaryActorTick.bCanEverTick = false; // 틱은 꺼둡니다 (성능 최적화)

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	GrappleWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("GrappleWidget"));
	GrappleWidget->SetupAttachment(RootComponent);
    
	// 🔥 중요: 스크린 스페이스로 설정하여 2D 화면에 이쁘게 정렬되도록 합니다.
	GrappleWidget->SetWidgetSpace(EWidgetSpace::Screen);
	GrappleWidget->SetVisibility(false); // 기본은 숨김 상태

	// 기존에 사용하시던 태그 등록
	Tags.Add(FName("GrappleTarget"));
}

void ASRGrapplePoint::BeginPlay()
{
	Super::BeginPlay();
}

void ASRGrapplePoint::SetWidgetActive(bool bActivate)
{
	if (GrappleWidget)
	{
		GrappleWidget->SetVisibility(bActivate);
		// 필요하다면 여기서 위젯 내부의 애니메이션(포커싱 효과 등)을 트리거할 수도 있습니다.
	}
}