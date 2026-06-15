// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SRTutorialVolume.generated.h"

UCLASS()
class SILENTRECALL_API ASRTutorialVolume : public AActor
{
	GENERATED_BODY()
    
public:    
	ASRTutorialVolume();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	// 트리거 범위를 담당할 박스 콜리전
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UBoxComponent* TriggerBox;

	// 🌟 디자이너가 에디터 디테일 창에서 직접 지정할 고유 ID (예: Tutorial_Dash, Tutorial_Vault)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial Settings")
	FName TutorialID;

	// 화면에 출력할 튜토리얼 안내 텍스트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial Settings")
	FText TutorialText;

	// 볼륨 내부 진입 시 적용할 시간 왜곡 배율 (0.2f = 5배 느려짐)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial Settings")
	float TutorialTimeDilation = 0.2f;

	// =======================================================================
	// 🎨 UI 연출 확장성을 위해 블루프린트로 통신을 넘겨주는 이벤트 하이웨이
	// =======================================================================
	// 볼륨 진입 시 메인 HUD 위젯 등 애니메이션을 켜기 위한 블플 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial Events")
	void ReceiveOnTutorialActivated(const FText& DisplayText);

	// 볼륨 탈출 시 UI를 지우기 위한 블플 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial Events")
	void ReceiveOnTutorialDeactivated();

	/** 덮어씌울 임시 포스트 프로세스 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UPostProcessComponent* TutorialPostProcess;

	/** 보간용 타겟 알파 변수들 */
	float TargetBlendWeight = 0.0f;
	float CurrentBlendWeight = 0.0f;
    
	/** 🌟 [신규 추가] 첫 프레임 오작동을 막고 페이드 아웃 완료 시점을 안전하게 캐치할 예약 스위치 */
	bool bWantsToDeactivatePP = false;

	/** 페이드 속도 (선언되어 있지 않다면 생성자나 헤더에 기본값 4.0f 등으로 세팅) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float FadeSpeed = 4.0f;
private:
	UFUNCTION()
	void OnVolumeOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnVolumeOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 중복 연산 방지용 세션 가드 플래그
	bool bIsCurrentlyActive = false;
};