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

#include "HoudiniVatActor.h"
#include "SidefxLabsRuntimeUtilities.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY(LogSideFxLabsRuntime);

/**
 * Default constructor for AHoudiniVatActor.
 * Sets up required components and initializes default values for VAT playback.
 */
AHoudiniVatActor::AHoudiniVatActor()
	: StartSeconds(0.0f)
	, bPlay(true)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));

	Vat_StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VAT Mesh"));
	Vat_StaticMesh->SetupAttachment(RootComponent);
	Vat_StaticMesh->SetMobility(EComponentMobility::Movable);

	OverlapShape = CreateDefaultSubobject<UBoxComponent>(TEXT("VAT Overlap Area"));
	OverlapShape->SetupAttachment(RootComponent);
	OverlapShape->SetMobility(EComponentMobility::Movable);
	OverlapShape->SetIsReplicated(true);
	OverlapShape->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	OverlapShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OverlapShape->SetCollisionResponseToAllChannels(ECR_Ignore);
	OverlapShape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	OverlapShape->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	OverlapShape->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	OverlapShape->SetHiddenInGame(true);

#if WITH_EDITOR
	OverlapShape->bIsEditorOnly = false;
	OverlapShape->SetIsVisualizationComponent(false);
	OverlapShape->SetLineThickness(2.0f);
	OverlapShape->ShapeColor = FColor(255, 255, 0, 255);
	OverlapShape->SetVisibility(false);
#endif

	Vat_MaterialInstances.Reset();
	bTriggerOnBeginPlay = true;
	bTriggerOnOverlap = false;
}

/**
 * Called when the game starts or when spawned.
 * Initializes materials and handles begin play trigger conditions.
 */
void AHoudiniVatActor::BeginPlay()
{
    Super::BeginPlay();

    if (!ensureMsgf(GetWorld(), TEXT("World is null in BeginPlay"))) { return; }
    StartSeconds = GetWorld()->GetTimeSeconds();

    if (!Vat_StaticMesh)
    {
        UE_LOG(LogSideFxLabsRuntime, Warning, TEXT("VAT Static Mesh is null on %s"), *GetName());
        return;
    }

#if !UE_SERVER
    const int32 NumMaterials = Vat_StaticMesh->GetNumMaterials();

    if (Original_MaterialInstances.Num() == 0)
    {
        static TSoftObjectPtr<UMaterialInterface> WorldGridMatRef(
            TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")))
        );
        UMaterialInterface* WorldGridMaterial = WorldGridMatRef.LoadSynchronous();
        if (WorldGridMaterial)
        {
            for (int32 i = 0; i < NumMaterials; ++i)
            {
                Vat_StaticMesh->SetMaterial(i, WorldGridMaterial);
            }
        }
        else
        {
            UE_LOG(LogSideFxLabsRuntime, Warning,
                   TEXT("Failed to load default World Grid Material at path: %s"),
                   TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
        }
    }
    else
    {
        const int32 ApplyCount = FMath::Min(NumMaterials, Original_MaterialInstances.Num());
        for (int32 i = 0; i < ApplyCount; ++i)
        {
            if (Original_MaterialInstances[i])
            {
                Vat_StaticMesh->SetMaterial(i, Original_MaterialInstances[i]);
            }
        }

        if (Original_MaterialInstances.Num() != NumMaterials)
        {
            UE_LOG(LogSideFxLabsRuntime, Verbose,
                   TEXT("Material slot count (%d) != Original_MaterialInstances.Num() (%d) on %s"),
                   NumMaterials, Original_MaterialInstances.Num(), *GetName());
        }
    }
#endif

    if (Vat_StaticMesh && bTriggerOnBeginPlay && Vat_MaterialInstances.Num() > 0)
    {
        TriggerVatPlayback();
    }
}

/**
 * Called every frame.
 *
 * @param DeltaTime Game time elapsed during last frame.
 */
void AHoudiniVatActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

/**
 * Event when this actor bumps into a blocking object, or blocks another actor that bumps into it.
 * Checks hit conditions and triggers VAT playback if criteria are met.
 * 
 * @param HitComp The component that was hit.
 * @param Other The other actor involved in the hit.
 * @param OtherComp The other component involved in the hit.
 * @param bSelfMoved Whether the actor was moving when the hit occurred.
 * @param HitLocation Point of impact.
 * @param HitNormal Normal vector at the hit location.
 * @param NormalImpulse Force of the hit.
 * @param Hit Hit result structure containing detailed information.
 */
void AHoudiniVatActor::NotifyHit(
	UPrimitiveComponent* HitComp,
	AActor* Other,
	UPrimitiveComponent* OtherComp,
	bool bSelfMoved,
	FVector HitLocation,
	FVector HitNormal,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	Super::NotifyHit(HitComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

#if UE_SERVER
    return;
#endif

	if (!Vat_StaticMesh || Vat_MaterialInstances.Num() == 0 || !bTriggerOnHit)
	{
		return;
	}

    if (HitComp && HitComp != Vat_StaticMesh)
    {
        return;
    }

	bool bShouldTrigger = false;
	const bool bHasFilters = HasHitFilters();

	if (!bHasFilters)
	{
		bShouldTrigger = true;
	}
	else
	{
		const bool bMatchFound = (Other != nullptr) && DoesActorMatchFilter(
			Other, 
			HitMatchMode, 
			HitObjectNames, 
			HitActorClasses, 
			HitActorTags
		);

		bShouldTrigger = bExcludeHitObjects ? !bMatchFound : bMatchFound;
	}

	if (bShouldTrigger)
	{
		TriggerVatPlayback();
	}
}

/**
 * Event when another actor begins to overlap this actor.
 * Evaluates overlap conditions and triggers VAT playback if appropriate.
 * 
 * @param OtherActor The actor that began overlapping with this actor.
 */
void AHoudiniVatActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

#if UE_SERVER
    return;
#endif

	if (!Vat_StaticMesh || Vat_MaterialInstances.Num() == 0 || !bTriggerOnOverlap)
	{
		return;
	}

    if (!IsValid(OtherActor) || OtherActor == this)
    {
        return;
    }

	bool bShouldTrigger = false;
	const bool bHasFilters = HasOverlapFilters();

	if (!bHasFilters)
	{
		bShouldTrigger = true;
	}
	else
	{
		const bool bMatchFound = DoesActorMatchFilter(
			OtherActor,
			OverlapMatchMode,
			OverlapObjectNames,
			OverlapActorClasses,
			OverlapActorTags);

		bShouldTrigger = bExcludeOverlapObjects ? !bMatchFound : bMatchFound;
	}

	if (bShouldTrigger)
	{
		TriggerVatPlayback();
	}
}

#if WITH_EDITOR
/**
 * Called when a property is changed in the editor.
 * Updates visualization components based on property changes.
 * 
 * @param PropertyChangedEvent Event describing the property that changed.
 */
void AHoudiniVatActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property || HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AHoudiniVatActor, bTriggerOnOverlap))
	{
		UpdateOverlapShapeVisibility();
	}
}
#endif

/**
 * Triggers the VAT animation playback.
 * Creates dynamic material instances and sets the appropriate timing parameters.
 * Respects the bTriggerOnce flag to prevent repeated triggers if configured.
 */
void AHoudiniVatActor::TriggerVatPlayback()
{
    UWorld* World = GetWorld();

	if (!bPlay || !World)
	{
		return;
	}

#if UE_SERVER
    return;
#endif

    if (!Vat_StaticMesh)
    {
        return;
    }

    const float Now = World->GetTimeSeconds();
    const float GameTimeInSeconds = (StartSeconds >= 0.f && StartSeconds <= Now) ? (Now - StartSeconds) : 0.f;
    const int32 NumSlots = Vat_StaticMesh->GetNumMaterials();
    const int32 ApplyCount = FMath::Min(NumSlots, Vat_MaterialInstances.Num());
    static const FName Param_GameTimeAtFirstFrame(TEXT("Game Time at First Frame"));

    for (int32 Index = 0; Index < ApplyCount; ++Index)
    {
        UMaterialInterface* Parent = Vat_MaterialInstances[Index];
        if (!Parent)
        {
            continue;
        }

        UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Vat_StaticMesh->GetMaterial(Index));

        if (!MID || MID->Parent != Parent)
        {
            MID = UMaterialInstanceDynamic::Create(Parent, this);

            if (!MID)
            {
                continue;
            }

            Vat_StaticMesh->SetMaterial(Index, MID);
        }

        MID->SetScalarParameterValue(Param_GameTimeAtFirstFrame, GameTimeInSeconds);
    }

	if (bTriggerOnce)
	{
		bPlay = false;
	}
}

/**
 * Resets the VAT animation to its initial state.
 * Restores original materials and clears all animation timing parameters.
 * Re-enables playback capability.
 */
void AHoudiniVatActor::ResetVatPlayback()
{
    if (!Vat_StaticMesh)
    {
        return;
    }

#if UE_SERVER
    return;
#endif

    const int32 NumSlots = Vat_StaticMesh->GetNumMaterials();
    static const FName Param_FirstFrame(TEXT("Game Time at First Frame"));

    for (int32 Index = 0; Index < NumSlots; ++Index)
    {
        if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Vat_StaticMesh->GetMaterial(Index)))
        {
            MID->SetScalarParameterValue(Param_FirstFrame, 0.0f);
        }
    }

    if (Original_MaterialInstances.Num() > 0)
    {
        const int32 ApplyCount = FMath::Min(NumSlots, Original_MaterialInstances.Num());
        for (int32 Index = 0; Index < ApplyCount; ++Index)
        {
            if (Original_MaterialInstances[Index])
            {
                Vat_StaticMesh->SetMaterial(Index, Original_MaterialInstances[Index]);
            }
            else
            {
                Vat_StaticMesh->SetMaterial(Index, nullptr);
            }
        }
        for (int32 Index = ApplyCount; Index < NumSlots; ++Index)
        {
            Vat_StaticMesh->SetMaterial(Index, nullptr);
        }
    }
    else
    {
        if (ResetFallbackMaterialRef.IsNull())
        {
            for (int32 Index = 0; Index < NumSlots; ++Index)
            {
                Vat_StaticMesh->SetMaterial(Index, nullptr);
            }
        }
        else
        {
            UMaterialInterface* Fallback = ResetFallbackMaterialRef.LoadSynchronous();
            if (Fallback)
            {
                for (int32 Index = 0; Index < NumSlots; ++Index)
                {
                    Vat_StaticMesh->SetMaterial(Index, Fallback);
                }
            }
        }
    }

    bPlay = true;
}

/**
 * Applies the specified materials to the static mesh component.
 *
 * @param Materials Array of materials to apply, indexed by material slot.
 */
void AHoudiniVatActor::ApplyMaterials(const TArray<TObjectPtr<UMaterialInterface>>& Materials)
{
    if (!Vat_StaticMesh)
    {
        return;
    }

#if UE_SERVER
    return;
#endif

    const int32 NumSlots = Vat_StaticMesh->GetNumMaterials();
    const int32 ApplyCount = FMath::Min(NumSlots, Materials.Num());

    for (int32 Index = 0; Index < ApplyCount; ++Index)
    {
        UMaterialInterface* NewMat = Materials[Index];
        if (Vat_StaticMesh->GetMaterial(Index) != NewMat)
        {
            Vat_StaticMesh->SetMaterial(Index, NewMat);
        }
    }

    for (int32 Index = ApplyCount; Index < NumSlots; ++Index)
    {
        if (Vat_StaticMesh->GetMaterial(Index) != nullptr)
        {
            Vat_StaticMesh->SetMaterial(Index, nullptr);
        }
    }
}

#if WITH_EDITOR
/**
 * Updates the visibility and appearance of the overlap shape in the editor.
 * Changes color based on whether overlap triggering is enabled.
 */
void AHoudiniVatActor::UpdateOverlapShapeVisibility()
{
	if (!OverlapShape)
	{
		return;
	}

	OverlapShape->SetVisibility(bTriggerOnOverlap, false);

	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(OverlapShape))
	{
		BoxComponent->ShapeColor = bTriggerOnOverlap ? FColor::Green : FColor::Red; 
        BoxComponent->MarkRenderStateDirty();
	}
}
#endif

/**
 * Determines if an actor matches the specified filter criteria.
 * Supports multiple match modes including name patterns, actor classes, and tags.
 * 
 * @param Actor The actor to check against filter criteria.
 * @param MatchMode The type of matching to perform.
 * @param Names Array of names or patterns to match against.
 * @param Classes Array of actor classes to match against.
 * @param FilterTags Array of actor tags to match against.
 *
 * @return true if the actor matches any of the filter criteria, false otherwise.
 */
bool AHoudiniVatActor::DoesActorMatchFilter(
    AActor* Actor,
    EVatObjectMatchMode MatchMode,
    const TArray<FString>& Names,
    const TArray<TSubclassOf<AActor>>& Classes,
    const TArray<FName>& FilterTags) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    switch (MatchMode)
    {
        case EVatObjectMatchMode::ExactMatch:
        case EVatObjectMatchMode::StartsWith:
        case EVatObjectMatchMode::EndsWith:
        case EVatObjectMatchMode::Contains:
        {
            if (Names.Num() == 0)
            {
                return false;
            }
#if WITH_EDITOR
            const FString& NameToMatch = Actor->GetActorLabel();
#else
            const FString& NameToMatch = Actor->GetName();
#endif
            return DoesNameMatchPattern(NameToMatch, Names, MatchMode);
        }

        case EVatObjectMatchMode::ActorClass:
        {
            if (Classes.Num() == 0)
            {
                return false;
            }
            return DoesMatchActorClass(Actor, Classes);
        }

        case EVatObjectMatchMode::ActorTag:
        {
            if (FilterTags.Num() == 0)
            {
                return false;
            }
            return DoesMatchActorTag(Actor, FilterTags);
        }

        default:
            ensureMsgf(false, TEXT("Unhandled EVatObjectMatchMode: %d"), (int32)MatchMode);
            return false;
    }
}

/**
 * Checks if any hit filter criteria are specified.
 * 
 * @return true if any hit filter criteria are set, false otherwise.
 */
bool AHoudiniVatActor::HasHitFilters() const
{
	return (HitObjectNames.Num() > 0) || (HitActorClasses.Num() > 0) || (HitActorTags.Num() > 0);
}


/**
 * Checks if any overlap filter criteria are specified.
 * 
 * @return true if any overlap filter criteria are set, false otherwise.
 */
bool AHoudiniVatActor::HasOverlapFilters() const
{
	return (OverlapObjectNames.Num() > 0) || (OverlapActorClasses.Num() > 0) || (OverlapActorTags.Num() > 0);
}


/**
 * Checks if the actor's name matches any of the specified patterns based on the chosen match mode.
 * Supports exact match, starts with, ends with, and contains match modes.
 * 
 * @param ActorName The name of the actor to compare against the filter patterns.
 * @param FilterNames Array of name patterns to check for matching.
 * @param MatchMode The type of matching to perform.
 *
 * @return true if the actor's name matches any of the patterns, false otherwise.
 */
bool AHoudiniVatActor::DoesNameMatchPattern(
	const FString& ActorName, 
	const TArray<FString>& FilterNames, 
	EVatObjectMatchMode MatchMode) const
{
	for (const FString& FilterName : FilterNames)
	{
		bool bMatches = false;
		
		switch (MatchMode)
		{
			case EVatObjectMatchMode::ExactMatch:
				bMatches = ActorName.Equals(FilterName, ESearchCase::CaseSensitive);
				break;

			case EVatObjectMatchMode::StartsWith:
				bMatches = ActorName.StartsWith(FilterName, ESearchCase::CaseSensitive);
				break;

			case EVatObjectMatchMode::EndsWith:
				bMatches = ActorName.EndsWith(FilterName, ESearchCase::CaseSensitive);
				break;

			case EVatObjectMatchMode::Contains:
				bMatches = ActorName.Contains(FilterName, ESearchCase::CaseSensitive);
				break;

			default:
				break;
		}

		if (bMatches)
		{
			return true;
		}
	}

	return false;
}


/**
 * Checks if the actor matches any of the specified actor classes in the filter.
 * 
 * @param Actor The actor to check.
 * @param FilterClasses Array of actor classes to compare against.
 *
 * @return true if the actor is an instance of one of the specified classes, false otherwise.
 */
bool AHoudiniVatActor::DoesMatchActorClass(
	AActor* Actor, 
	const TArray<TSubclassOf<AActor>>& FilterClasses) const
{
	if (!Actor)
	{
		return false;
	}

	for (const TSubclassOf<AActor>& FilterClass : FilterClasses)
	{
		if (FilterClass && Actor->IsA(FilterClass))
		{
			return true;
		}
	}

	return false;
}


/**
 * Checks if the actor has any of the specified tags.
 * 
 * @param Actor The actor to check.
 * @param FilterTags Array of actor tags to check for matching.
 *
 * @return true if the actor has any of the specified tags, false otherwise.
 */
bool AHoudiniVatActor::DoesMatchActorTag(
	AActor* Actor, 
	const TArray<FName>& FilterTags) const
{
	if (!Actor)
	{
		return false;
	}

	for (const FName& FilterTag : FilterTags)
	{
		if (Actor->ActorHasTag(FilterTag))
		{
			return true;
		}
	}

	return false;
}
