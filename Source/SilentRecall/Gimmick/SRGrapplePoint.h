#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SRGrapplePoint.generated.h"

class UWidgetComponent;
class USphereComponent;

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


protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* GrappleWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* SphereComponent;
};