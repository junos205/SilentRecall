// Fill out your copyright notice in the Description page of Project Settings.


#include "MetaxisBlueprintFunctionLibrary.h"
#include "Perception/AIPerceptionComponent.h"
#include "GameFramework/Actor.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Sight.h"


// Forget Functions

void UMetaxisBlueprintFunctionLibrary::AIForgetActor(UAIPerceptionComponent* AIPerceptionComponent, AActor* ActorToForget)
{
	if (!IsValid(AIPerceptionComponent) || !IsValid(ActorToForget))
	{
		UE_LOG(LogTemp, Warning, TEXT("AI Forget Actor: Invalid AIPerceptionComponent or ActorToForget provided. Cannot forget."));
		return;
	}

	AIPerceptionComponent->ForgetActor(ActorToForget);
	UE_LOG(LogTemp, Log, TEXT("AI Forget Actor: Attempted to forget actor %s."), *ActorToForget->GetName());
}

void UMetaxisBlueprintFunctionLibrary::AIForgetActors(UAIPerceptionComponent* AIPerceptionComponent, const TArray<AActor*>& ActorsToForget)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("AI Forget Actors: Invalid AIPerceptionComponent provided. Cannot forget any actors."));
		return;
	}

	bool bAnyForgotten = false; // Track if any actor was actually forgotten for logging/update request

	for (AActor* CurrentActor : ActorsToForget)
	{
		if (IsValid(CurrentActor))
		{
			AIPerceptionComponent->ForgetActor(CurrentActor);
			bAnyForgotten = true;
			UE_LOG(LogTemp, Log, TEXT("AI Forget Actors: Successfully told component to forget actor %s."), *CurrentActor->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AI Forget Actors: Null or invalid actor found in the input array. Skipping."));
		}
	}
	
	if (bAnyForgotten) 
	{
		AIPerceptionComponent->RequestStimuliListenerUpdate();
		UE_LOG(LogTemp, Log, TEXT("AI Forget Actors: Requesting stimuli listener update for component %s."), *AIPerceptionComponent->GetName());
	}
}



// Set Sight Config Functions


void UMetaxisBlueprintFunctionLibrary::SetVisionConeHalfAngle(UAIPerceptionComponent* AIPerceptionComponent, float NewHalfAngle)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetVisionConeHalfAngle: Invalid AIPerceptionComponent provided."));
		return;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
	
	if (SightConfig)
	{
		SightConfig->PeripheralVisionAngleDegrees = FMath::Clamp(NewHalfAngle, 0.0f, 180.0f);
		AIPerceptionComponent->RequestStimuliListenerUpdate();
		
		UE_LOG(LogTemp, Log, TEXT("SetVisionConeHalfAngle: Successfully set sight sense half angle to %f degrees for component %s."),
			NewHalfAngle, *AIPerceptionComponent->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetVisionConeHalfAngle: No UAISenseConfig_Sight found on component %s. Ensure it has a Sight Sense configured."), *AIPerceptionComponent->GetName());
	}
}

void UMetaxisBlueprintFunctionLibrary::SetVisionConeSightRadius(UAIPerceptionComponent* AIPerceptionComponent, float NewRadius)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetVisionConeSightRadius: Invalid AIPerceptionComponent provided."));
		return;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
	
	if (SightConfig)
	{
		SightConfig->SightRadius = FMath::Max(0.0f, NewRadius);
		AIPerceptionComponent->RequestStimuliListenerUpdate();

		UE_LOG(LogTemp, Log, TEXT("SetVisionConeSightRadius: Successfully set sight sense radius to %f for component %s."),
			NewRadius, *AIPerceptionComponent->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetVisionConeSightRadius: No UAISenseConfig_Sight found on component %s. Ensure it has a Sight Sense configured."), *AIPerceptionComponent->GetName());
	}
}

void UMetaxisBlueprintFunctionLibrary::SetVisionConeLoseSightRadius(UAIPerceptionComponent* AIPerceptionComponent, float NewLoseSightRadius)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetVisionConeLoseSightRadius: Invalid AIPerceptionComponent provided."));
		return;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
    
	if (SightConfig)
	{
		SightConfig->LoseSightRadius = FMath::Max(0.0f, NewLoseSightRadius);
        
		AIPerceptionComponent->RequestStimuliListenerUpdate();

		UE_LOG(LogTemp, Log, TEXT("SetVisionConeLoseSightRadius: Successfully set sight sense lose sight radius to %f for component %s."),
			NewLoseSightRadius, *AIPerceptionComponent->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetVisionConeLoseSightRadius: No UAISenseConfig_Sight found on component %s. Ensure it has a Sight Sense configured."), *AIPerceptionComponent->GetName());
	}
}

void UMetaxisBlueprintFunctionLibrary::SetPointOfViewBackwardOffset(UAIPerceptionComponent* AIPerceptionComponent, float NewOffset)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetPointOfViewBackwardOffset: Invalid AIPerceptionComponent provided."));
		return;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
	
	if (SightConfig)
	{
		SightConfig->PointOfViewBackwardOffset = FMath::Max(0.0f, NewOffset); 
		AIPerceptionComponent->RequestStimuliListenerUpdate();
		UE_LOG(LogTemp, Log, TEXT("SetPointOfViewBackwardOffset: Successfully set sight sense POV backward offset to %f for component %s."),
			NewOffset, *AIPerceptionComponent->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetPointOfViewBackwardOffset: No UAISenseConfig_Sight found on component %s. Ensure it has a Sight Sense configured."), *AIPerceptionComponent->GetName());
	}
}

void UMetaxisBlueprintFunctionLibrary::SetNearClippingRadius(UAIPerceptionComponent* AIPerceptionComponent, float NewRadius)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNearClippingRadius: Invalid AIPerceptionComponent provided."));
		return;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
	
	if (SightConfig)
	{
		SightConfig->NearClippingRadius = FMath::Max(0.0f, NewRadius); 
		AIPerceptionComponent->RequestStimuliListenerUpdate();
		UE_LOG(LogTemp, Log, TEXT("SetNearClippingRadius: Successfully set sight sense near clipping radius to %f for component %s."),
			NewRadius, *AIPerceptionComponent->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNearClippingRadius: No UAISenseConfig_Sight found on component %s. Ensure it has a Sight Sense configured."), *AIPerceptionComponent->GetName());
	}
}



// Get Sight Config Functions


float UMetaxisBlueprintFunctionLibrary::GetVisionConeHalfAngle(UAIPerceptionComponent* AIPerceptionComponent)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetVisionConeHalfAngle: Invalid AIPerceptionComponent provided. Returning 0.0f."));
		return 0.0f;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
	
	if (SightConfig)
	{
		return SightConfig->PeripheralVisionAngleDegrees;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetVisionConeHalfAngle: No UAISenseConfig_Sight found on component %s. Returning 0.0f."), *AIPerceptionComponent->GetName());
	return 0.0f;
}

float UMetaxisBlueprintFunctionLibrary::GetVisionConeSightRadius(UAIPerceptionComponent* AIPerceptionComponent)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetVisionConeSightRadius: Invalid AIPerceptionComponent provided. Returning 0.0f."));
		return 0.0f;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
	
	if (SightConfig)
	{
		return SightConfig->SightRadius;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetVisionConeSightRadius: No UAISenseConfig_Sight found on component %s. Returning 0.0f."), *AIPerceptionComponent->GetName());
	return 0.0f;
}

float UMetaxisBlueprintFunctionLibrary::GetVisionConeLoseSightRadius(UAIPerceptionComponent* AIPerceptionComponent)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetVisionConeLoseSightRadius: Invalid AIPerceptionComponent provided. Returning 0.0f."));
		return 0.0f;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
    
	if (SightConfig)
	{
		return SightConfig->LoseSightRadius;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetVisionConeLoseSightRadius: No UAISenseConfig_Sight found on component %s. Returning 0.0f."), *AIPerceptionComponent->GetName());
	return 0.0f;
}

float UMetaxisBlueprintFunctionLibrary::GetPointOfViewBackwardOffset(UAIPerceptionComponent* AIPerceptionComponent)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetPointOfViewBackwardOffset: Invalid AIPerceptionComponent provided. Returning 0.0f."));
		return 0.0f;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
	
	if (SightConfig)
	{
		return SightConfig->PointOfViewBackwardOffset;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetPointOfViewBackwardOffset: No UAISenseConfig_Sight found on component %s. Returning 0.0f."), *AIPerceptionComponent->GetName());
	return 0.0f;
}

float UMetaxisBlueprintFunctionLibrary::GetNearClippingRadius(UAIPerceptionComponent* AIPerceptionComponent)
{
	if (!IsValid(AIPerceptionComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetNearClippingRadius: Invalid AIPerceptionComponent provided. Returning 0.0f."));
		return 0.0f;
	}

	UAISenseConfig_Sight* SightConfig = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
	
	if (SightConfig)
	{
		return SightConfig->NearClippingRadius;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetNearClippingRadius: No UAISenseConfig_Sight found on component %s. Returning 0.0f."), *AIPerceptionComponent->GetName());
	return 0.0f;
}