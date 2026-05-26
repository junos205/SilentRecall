#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SRGrapplePoint.generated.h"

class UWidgetComponent;

UCLASS()
class SILENTRECALL_API ASRGrapplePoint : public AActor
{
	GENERATED_BODY()
    
public:    
	ASRGrapplePoint();

protected:
	virtual void BeginPlay() override;

public:
	// UI를 켜고 끄는 함수
	void SetWidgetActive(bool bActivate);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* GrappleWidget;
};