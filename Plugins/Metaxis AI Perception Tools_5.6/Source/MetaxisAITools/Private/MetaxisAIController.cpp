// MetaxisAIController.cpp
#include "MetaxisAIController.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Perception/AIPerceptionComponent.h"

AMetaxisAIController::AMetaxisAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bEnableOverrideEyesViewpoint = true;
    EyeSocketName = TEXT("Eyes");
    bFlattenSightForwardAxis = false;
}

void AMetaxisAIController::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
    {
        Super::GetActorEyesViewPoint(OutLocation, OutRotation);
        return;
    }
    
    if (bEnableOverrideEyesViewpoint)
    {
        USkeletalMeshComponent* TargetSkeletalMesh = nullptr;

        // First, try to get the mesh from a Character (if it is one)
        ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn);
        if (ControlledCharacter)
        {
            TargetSkeletalMesh = ControlledCharacter->GetMesh();
        }
        else // If not a Character, try to find a SkeletalMeshComponent directly on the Pawn
        {
            TargetSkeletalMesh = ControlledPawn->FindComponentByClass<USkeletalMeshComponent>();
        }

        if (TargetSkeletalMesh && TargetSkeletalMesh->DoesSocketExist(EyeSocketName))
        {
            FTransform SocketTransform = TargetSkeletalMesh->GetSocketTransform(EyeSocketName);

            OutLocation = SocketTransform.GetLocation();
            
            if (bFlattenSightForwardAxis)
            {
                FVector SocketForwardVector = SocketTransform.GetRotation().GetForwardVector();
                FVector FlatForwardVector = SocketForwardVector;
                FlatForwardVector.Z = 0.0f;

                if (!FlatForwardVector.IsNearlyZero())
                {
                    FlatForwardVector.Normalize();
                }
                else
                {
                    FlatForwardVector = ControlledPawn->GetActorForwardVector();
                    FlatForwardVector.Z = 0.0f;
                    if (!FlatForwardVector.IsNearlyZero())
                    {
                        FlatForwardVector.Normalize();
                    }
                    else
                    {
                        FlatForwardVector = FVector::ForwardVector;
                    }
                }
                OutRotation = UKismetMathLibrary::MakeRotFromX(FlatForwardVector);
            }
            else
            {
                OutRotation = SocketTransform.GetRotation().Rotator();
            }
            return;
        }
    }
    Super::GetActorEyesViewPoint(OutLocation, OutRotation);
    return;
}

void AMetaxisAIController::SetFlattenSightForwardAxis(bool bNewEnabled)
{
    bFlattenSightForwardAxis = bNewEnabled;
    UE_LOG(LogTemp, Log, TEXT("AMetaxisAIController '%s': Set Compensate Sight Forward Axis to %s."),
           *GetName(), bNewEnabled ? TEXT("Enabled") : TEXT("Disabled"));
    
    // Request an AI perception update if a core perception parameter changes.
    UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent();
    if (PerceptionComp)
    {
        PerceptionComp->RequestStimuliListenerUpdate();
    }
}