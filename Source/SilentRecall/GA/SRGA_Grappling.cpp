#include "GA/SRGA_Grappling.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "DrawDebugHelpers.h"
#include "Character/SRPlayerCharacter.h" 
#include "AbilitySystemComponent.h" 
#include "GameplayTagContainer.h"   
#include "GameFramework/CharacterMovementComponent.h" 

USRGA_Grappling::USRGA_Grappling()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 그래플링 상태 태그 부여
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Grappling")));
    
    // 피격 중이거나 파쿠르 중이면 훅을 쏠 수 없음
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
}

void USRGA_Grappling::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
       EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
       return;
    }

    ASRPlayerCharacter* SRCharacter = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
    if (!SRCharacter)
    {
       EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
       return;
    }

    // 레이저(Line Trace) 발사 준비
    FVector StartLocation;
    FRotator ViewRotation;
    SRCharacter->GetActorEyesViewPoint(StartLocation, ViewRotation);
    FVector EndLocation = StartLocation + (ViewRotation.Vector() * GrappleRange);
    FVector ViewDir = ViewRotation.Vector().GetSafeNormal();

    TArray<FHitResult> HitResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(SRCharacter);
    
    // 빔의 두께(반경) 설정
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(250.0f);

    // 두꺼운 구체 스윕 발사!
    bool bHit = GetWorld()->SweepMultiByChannel(
       HitResults, 
       StartLocation, 
       EndLocation, 
       FQuat::Identity,
       ECC_GameTraceChannel2, 
       SphereShape, 
       QueryParams
    );
    
    if (bDrawDebug)
    {
       FColor BeamColor = bHit ? FColor::Green : FColor::Red; 
       DrawDebugCapsule(GetWorld(), (StartLocation + EndLocation) * 0.5f, FVector::Distance(StartLocation, EndLocation) * 0.5f + SphereShape.GetSphereRadius(), SphereShape.GetSphereRadius(), FRotationMatrix::MakeFromZ(EndLocation - StartLocation).ToQuat(), BeamColor, false, 3.0f, 0, 2.0f);
    }

    if (bHit)
    {
       AActor* BestTarget = nullptr;
       FVector BestImpactPoint = FVector::ZeroVector;
       float BestDotProduct = -1.0f;

       for (const FHitResult& Hit : HitResults)
       {
          AActor* HitActor = Hit.GetActor();
          if (HitActor && HitActor->ActorHasTag(FName("GrappleTarget")))
          {
             FVector DirToTarget = (Hit.ImpactPoint - StartLocation).GetSafeNormal();
             float DotProduct = FVector::DotProduct(ViewDir, DirToTarget);

             if (DotProduct > 0.5f && DotProduct > BestDotProduct)
             {
                BestDotProduct = DotProduct; 
                BestTarget = HitActor;       
                BestImpactPoint = Hit.ImpactPoint;
             }
          }
       }

       if (BestTarget != nullptr)
       {
          SRCharacter->StartGrapple(BestImpactPoint);
          
          // // 타겟을 찾았으면 버튼을 뗄 때까지 대기
          // UAbilityTask_WaitInputRelease* WaitInputTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
          // WaitInputTask->OnRelease.AddDynamic(this, &USRGA_Grappling::OnInputReleased);
          // WaitInputTask->ReadyForActivation();
          //
          return; 
       }
    }
    
    // 허공에 쏘거나 타겟을 못 찾았다면 즉시 취소
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void USRGA_Grappling::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
       ASRPlayerCharacter* SRCharacter = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
       if (SRCharacter)
       {
          SRCharacter->StopGrapple();
       }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled); 
}

void USRGA_Grappling::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);

    if (IsActive())
    {
       EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void USRGA_Grappling::OnInputReleased(float TimeHeld)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}