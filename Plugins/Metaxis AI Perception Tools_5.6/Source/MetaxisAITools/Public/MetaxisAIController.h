// Copyright Metaxis Games 2025

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MetaxisAIController.generated.h"

/**
 * A custom AI Controller that expands perception functionality.
 */

UCLASS()
class METAXISAITOOLS_API AMetaxisAIController : public AAIController
{
    GENERATED_BODY()

public:
    AMetaxisAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

    UFUNCTION(BlueprintCallable, Category = "Metaxis AI Perception Tools")
    void SetFlattenSightForwardAxis(bool bEnabled);

protected:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Perception Tools (Metaxis Games)", DisplayName = "Enable Override Eyes Viewpoint")
    bool bEnableOverrideEyesViewpoint;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Perception Tools (Metaxis Games)")
    FName EyeSocketName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Perception Tools (Metaxis Games)", meta=(DefaultValue=false), DisplayName = "Flatten Sight Forward Axis")
    bool bFlattenSightForwardAxis;
};