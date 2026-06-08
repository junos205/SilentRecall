#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "SRObjectiveMarker.generated.h"

UCLASS()
class SILENTRECALL_API ASRObjectiveMarker : public AActor
{
	GENERATED_BODY()
	
public:	
	ASRObjectiveMarker();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	void ActivateMarker();

	UPROPERTY(EditAnywhere, Category = "Objective")
	float HideDistance = 300.0f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective|UI")
	UWidgetComponent* ObjectiveWidgetComp;

	// =======================================================================
	// 🟢 [여기를 추가!] C++ 전용 가드 스위치 변수를 선언합니다.
	// =======================================================================
private:
	bool bIsActive = false;
	// =======================================================================
};