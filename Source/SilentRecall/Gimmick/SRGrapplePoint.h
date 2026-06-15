#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SRGrapplePoint.generated.h"

class UWidgetComponent;
class UBoxComponent; // 🌟 [수정] USphereComponent 대신 박스 전방 선언 탑재

UCLASS()
class SILENTRECALL_API ASRGrapplePoint : public AActor
{
	GENERATED_BODY()
    
public:    
	ASRGrapplePoint();
	virtual void Tick(float DeltaTime) override;
	void SetWidgetActive(bool bActivate);

protected:
	virtual void BeginPlay() override;

	float CurrentAlpha = 0.0f;
	float TargetAlpha = 0.0f;

	UPROPERTY(EditAnywhere, Category = "UI")
	float FadeSpeed = 10.0f;

	float CurrentScale = 0.0f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* GrappleWidget;

	// 🌟 [수정] 구체에서 직사각형 박스 콜리전 컴포넌트로 완벽 교체
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent; 
};