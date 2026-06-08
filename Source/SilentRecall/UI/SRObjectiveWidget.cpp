#include "UI/SRObjectiveWidget.h"

void USRObjectiveWidget::UpdateDistanceText(float DistanceInMeters)
{
	// =======================================================================
	// 📡 [수신소 검문] UMG 텍스트 블록 변수 존재 여부 추적
	// =======================================================================
	if (!Txt_Distance)
	{
		// 🔴 변수 매핑 실패 로그
		UE_LOG(LogTemp, Error, TEXT("[WidgetDebug] 🚨 텍스트 주입 실패: UMG의 'Txt_Distance' 블록을 C++이 찾지 못했습니다!!"));
		return;
	}

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 1;
	Options.MaximumFractionalDigits = 1;

	FText FormattedText = FText::Format(
		FText::FromString(TEXT("{0}m")), 
		FText::AsNumber(DistanceInMeters, &Options)
	);

	Txt_Distance->SetText(FormattedText);
    
}