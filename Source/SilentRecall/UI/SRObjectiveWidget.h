#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "SRObjectiveWidget.generated.h"

UCLASS()
class SILENTRECALL_API USRObjectiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 🌟 UMG 디자이너 창에서 변수 이름을 똑같이 'Txt_Distance'로 만들면 C++이 자동 바인딩합니다.
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Txt_Distance;

	// 소수점 한 자리 반올림 정밀 포맷팅이 탑재된 텍스트 갈아끼우기 함수
	void UpdateDistanceText(float DistanceInMeters);
};