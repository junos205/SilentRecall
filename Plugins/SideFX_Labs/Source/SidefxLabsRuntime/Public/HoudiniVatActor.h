/*
* Copyright (c) 2025 Side Effects Software Inc.  All rights reserved.
*
* Redistribution and use of in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
* promote products derived from this software without specific prior
* written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HoudiniVatActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;

/**
 * Defines how objects are matched against filter criteria for VAT triggering.
 */
UENUM(BlueprintType)
enum class EVatObjectMatchMode : uint8
{
	ExactMatch UMETA(DisplayName = "Exact Match", ToolTip = "Object names must match exactly."),
	StartsWith UMETA(DisplayName = "Starts With", ToolTip = "Object name must start with the filter text."),
	EndsWith UMETA(DisplayName = "Ends With", ToolTip = "Object name must end with the filter text."),
	Contains UMETA(DisplayName = "Contains", ToolTip = "Object name must contain the filter text (use carefully as it can match many objects)."),
	ActorClass UMETA(DisplayName = "Actor Class", ToolTip = "Match by actor class type."),
	ActorTag UMETA(DisplayName = "Actor Tag", ToolTip = "Match by actor tags.")
};

/**
 * Actor that manages VAT playback.
 * Supports triggering animations based on begin play, hit events, and overlap events.
 */
UCLASS()
class SIDEFXLABSRUNTIME_API AHoudiniVatActor : public AActor
{
	GENERATED_BODY()

public:
	AHoudiniVatActor();

    /** Triggers VAT animation playback on game begin play. */
	virtual void BeginPlay() override;

    /** Called every frame. */
	virtual void Tick(float DeltaTime) override;

    /** Triggers VAT animation playback when hit conditions are met. */
	virtual void NotifyHit(UPrimitiveComponent* HitComp,
        AActor* Other,
        UPrimitiveComponent* OtherComp, 
		bool bSelfMoved, FVector HitLocation,
        FVector HitNormal,
        FVector NormalImpulse, 
		const FHitResult& Hit) override;

    /** Triggers VAT animation playback when overlap conditions are met. */
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

#if WITH_EDITOR
    /** Updates visualization components based on property changes. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Triggers VAT animation playback. */
	UFUNCTION(BlueprintCallable, Category = "Houdini VAT")
	void TriggerVatPlayback();

	/** Resets VAT animation to its initial state. */
	UFUNCTION(BlueprintCallable, Category = "Houdini VAT")
	void ResetVatPlayback();

public:
	/** The static mesh component for the VAT static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Asset", 
		DisplayName = "VAT Static Mesh",
		meta = (ToolTip = "The static mesh component for the VAT static mesh."))
	TObjectPtr<UStaticMeshComponent> Vat_StaticMesh;

	/** Material instances that are parented to materials containing VAT material functions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Asset", 
		DisplayName = "VAT Material Instances",
		meta = (ToolTip = "The material instances that are parented to materials containing VAT material functions. Each array index corresponds with each material slot on the VAT static mesh."))
	TArray<TObjectPtr<UMaterialInterface>> Vat_MaterialInstances;

	/** Material instances assigned to the VAT static mesh before the VAT is triggered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Asset",
		meta = (ToolTip = "These are the material instances that will be assigned to the VAT static mesh before the VAT is triggered. Each array index corresponds with each material slot on the VAT static mesh."))
	TArray<TObjectPtr<UMaterialInterface>> Original_MaterialInstances;

	/** VAT will play when begin play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (ToolTip = "VAT will play when begin play starts."))
	bool bTriggerOnBeginPlay;

	/** VAT will play when hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (ToolTip = "VAT will play when hit."))
	bool bTriggerOnHit;

	/** How to match objects for hit detection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnHit", EditConditionHides,
		ToolTip = "How to match objects for hit detection."))
	EVatObjectMatchMode HitMatchMode = EVatObjectMatchMode::ActorClass;

	/** Object names or patterns to match against for hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnHit && (HitMatchMode == EVatObjectMatchMode::ExactMatch || HitMatchMode == EVatObjectMatchMode::StartsWith || HitMatchMode == EVatObjectMatchMode::EndsWith || HitMatchMode == EVatObjectMatchMode::Contains)",
		EditConditionHides,
		ToolTip = "Object names or patterns to match against."))
	TArray<FString> HitObjectNames;

	/** Actor classes that will trigger VAT to play on hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnHit && HitMatchMode == EVatObjectMatchMode::ActorClass",
		EditConditionHides,
		ToolTip = "Actor classes that will trigger VAT to play."))
	TArray<TSubclassOf<AActor>> HitActorClasses;

	/** Actor tags that will trigger VAT to play on hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnHit && HitMatchMode == EVatObjectMatchMode::ActorTag",
		EditConditionHides,
		ToolTip = "Actor tags that will trigger VAT to play."))
	TArray<FName> HitActorTags;

	/** Inverts the hit filter logic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnHit", EditConditionHides,
		ToolTip = "Inverts the filter logic. When true: listed objects will NOT trigger. When false: ONLY listed objects will trigger."))
	bool bExcludeHitObjects;

	/** VAT will play when objects overlap with specified shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (ToolTip = "VAT will play when objects overlap with specified shape in the Overlap Shape parameter."))
	bool bTriggerOnOverlap;

	/** The bounding region used to trigger the VAT to play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnOverlap", EditConditionHides,
		ToolTip = "The bounding region used to trigger the VAT to play."))
	TObjectPtr<UBoxComponent> OverlapShape;

	/** How to match objects for overlap detection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnOverlap", EditConditionHides,
		ToolTip = "How to match objects for overlap detection."))
	EVatObjectMatchMode OverlapMatchMode = EVatObjectMatchMode::ActorClass;

	/** Object names or patterns to match against for overlaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnOverlap && (OverlapMatchMode == EVatObjectMatchMode::ExactMatch || OverlapMatchMode == EVatObjectMatchMode::StartsWith || OverlapMatchMode == EVatObjectMatchMode::EndsWith || OverlapMatchMode == EVatObjectMatchMode::Contains)",
		EditConditionHides,
		ToolTip = "Object names or patterns to match against for overlaps."))
	TArray<FString> OverlapObjectNames;

	/** Actor classes that will trigger VAT to play on overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnOverlap && OverlapMatchMode == EVatObjectMatchMode::ActorClass",
		EditConditionHides,
		ToolTip = "Actor classes that will trigger VAT to play on overlap."))
	TArray<TSubclassOf<AActor>> OverlapActorClasses;

	/** Actor tags that will trigger VAT to play on overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnOverlap && OverlapMatchMode == EVatObjectMatchMode::ActorTag",
		EditConditionHides,
		ToolTip = "Actor tags that will trigger VAT to play on overlap."))
	TArray<FName> OverlapActorTags;

	/** Inverts the overlap filter logic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Conditions",
		meta = (EditCondition = "bTriggerOnOverlap", EditConditionHides,
		ToolTip = "Inverts the filter logic. When true: listed objects will NOT trigger. When false: ONLY listed objects will trigger."))
	bool bExcludeOverlapObjects;

	/** When enabled the VAT will only trigger once and not repeat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Houdini VAT|Properties",
		meta = (ToolTip = "When enabled the VAT will only trigger once and not repeat."))
	bool bTriggerOnce;

    /** Fallback material for ResetVatPlayback function. */
    UPROPERTY(EditDefaultsOnly, Category="Houdini VAT|Materials", DisplayName = "Reset VAT Fallback Material")
    TSoftObjectPtr<UMaterialInterface> ResetFallbackMaterialRef; 

protected:
	/** Applies materials to the static mesh component. */
	void ApplyMaterials(const TArray<TObjectPtr<UMaterialInterface>>& Materials);

#if WITH_EDITOR
	/** Updates the visibility and color of the overlap shape in the editor. */
	void UpdateOverlapShapeVisibility();
#endif

private:
	/** Checks if an actor matches the specified filter criteria. */
	bool DoesActorMatchFilter(
		AActor* Actor, 
		EVatObjectMatchMode MatchMode, 
		const TArray<FString>& Names, 
		const TArray<TSubclassOf<AActor>>& Classes, 
		const TArray<FName>& Tags) const;

	/** Checks if any hit filters are configured. */
	bool HasHitFilters() const;

	/** Checks if any overlap filters are configured. */
	bool HasOverlapFilters() const;

	/** Checks if an actor name matches the specified pattern based on match mode. */
	bool DoesNameMatchPattern(const FString& ActorName, const TArray<FString>& FilterNames, EVatObjectMatchMode MatchMode) const;

	/** Checks if an actor matches any of the specified classes. */
	bool DoesMatchActorClass(AActor* Actor, const TArray<TSubclassOf<AActor>>& FilterClasses) const;

	/** Checks if an actor has any of the specified tags. */
	bool DoesMatchActorTag(AActor* Actor, const TArray<FName>& FilterTags) const;

private:
	/** Time when the actor began playing. */
	float StartSeconds;
	
	/** Whether VAT playback is enabled. */
	bool bPlay;
};
