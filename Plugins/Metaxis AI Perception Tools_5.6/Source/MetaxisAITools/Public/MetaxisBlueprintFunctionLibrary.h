// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetaxisBlueprintFunctionLibrary.generated.h"

class UAIPerceptionComponent;
class AActor;

/**
 * Blueprint Function Library for Metaxis AI Perception Tools.
 */
UCLASS()
class UMetaxisBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Forget Functions
	
	UFUNCTION(BlueprintCallable, Category = "Metaxis AI Perception Tools", meta=(DisplayName = "AI Forget Actor"))
	static void AIForgetActor(UAIPerceptionComponent* AIPerceptionComponent, AActor* ActorToForget);
	
	UFUNCTION(BlueprintCallable, Category = "Metaxis AI Perception Tools", meta=(DisplayName = "AI Forget Actors"))
	static void AIForgetActors(UAIPerceptionComponent* AIPerceptionComponent, const TArray<AActor*>& ActorsToForget);

	// Set Sight Config Functions
	
	UFUNCTION(BlueprintCallable, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Set Vision Cone Half Angle"))
	static void SetVisionConeHalfAngle(UAIPerceptionComponent* AIPerceptionComponent, float NewHalfAngle);
	
	UFUNCTION(BlueprintCallable, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Set Vision Cone Sight Radius"))
	static void SetVisionConeSightRadius(UAIPerceptionComponent* AIPerceptionComponent, float NewRadius);
	
	UFUNCTION(BlueprintCallable, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Set Vision Cone Lose Sight Radius"))
	static void SetVisionConeLoseSightRadius(UAIPerceptionComponent* AIPerceptionComponent, float NewLoseSightRadius);
	
	UFUNCTION(BlueprintCallable, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Set Point Of View Backward Offset"))
	static void SetPointOfViewBackwardOffset(UAIPerceptionComponent* AIPerceptionComponent, float NewOffset);
	
	UFUNCTION(BlueprintCallable, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Set Near Clipping Radius"))
	static void SetNearClippingRadius(UAIPerceptionComponent* AIPerceptionComponent, float NewRadius);
	

	// Get Sight Config Functions

	UFUNCTION(BlueprintPure, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Get Vision Cone Half Angle"))
	static float GetVisionConeHalfAngle(UAIPerceptionComponent* AIPerceptionComponent);
	
	UFUNCTION(BlueprintPure, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Get Vision Cone Sight Radius"))
	static float GetVisionConeSightRadius(UAIPerceptionComponent* AIPerceptionComponent);
	
	UFUNCTION(BlueprintPure, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Get Vision Cone Lose Sight Radius"))
	static float GetVisionConeLoseSightRadius(UAIPerceptionComponent* AIPerceptionComponent);

	UFUNCTION(BlueprintPure, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Get Point Of View Backward Offset"))
	static float GetPointOfViewBackwardOffset(UAIPerceptionComponent* AIPerceptionComponent);

	UFUNCTION(BlueprintPure, Category = "Metaxis AI Perception Tools|Sight Config", meta=(DisplayName = "Get Near Clipping Radius"))
	static float GetNearClippingRadius(UAIPerceptionComponent* AIPerceptionComponent);
	
};