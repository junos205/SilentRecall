// Fill out your copyright notice in the Description page of Project Settings.
#include "SRGA_RangedAttack.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Weapon/SRWeaponInstance.h"
#include "Character/SRPlayerCharacter.h"


USRGA_RangedAttack::USRGA_RangedAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void USRGA_RangedAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	FGameplayTag FireEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Ranged.Fire"));
	UAbilityTask_WaitGameplayEvent* WaitFireTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FireEventTag);
	WaitFireTask->EventReceived.AddDynamic(this, &USRGA_RangedAttack::OnFireEventReceived);
	WaitFireTask->ReadyForActivation();

	// 2. 무기 데이터에서 몽타주 꺼내서 재생
	USRWeaponInstance* WeaponInstance = Cast<USRWeaponInstance>(GetCurrentSourceObject());
	if (WeaponInstance && WeaponInstance->WeaponData && WeaponInstance->WeaponData->AttackComboMontages.Num() > 0)
	{
		// 원거리 무기는 보통 AttackComboMontages[0] 하나만 씁니다 (단발 사격)
		UAnimMontage* FireMontage = WeaponInstance->WeaponData->AttackComboMontages[0];
        
		UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, FireMontage, 1.0f
		);
		PlayMontageTask->OnCompleted.AddDynamic(this, &USRGA_RangedAttack::OnMontageCompleted);
		PlayMontageTask->OnInterrupted.AddDynamic(this, &USRGA_RangedAttack::OnMontageCompleted);
		PlayMontageTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void USRGA_RangedAttack::OnFireEventReceived(FGameplayEventData Payload)
{
	AActor* TargetActor = const_cast<AActor*>(Payload.Target.Get());
	if (!TargetActor || !DamageEffectClass) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC)
	{
		FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
		ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

		// ⭐️ 노티파이가 페이로드에 담아준 TargetData(HitResult)를 꺼내서 주머니에 쏙!
		if (Payload.TargetData.IsValid(0))
		{
			const FHitResult* HitResult = Payload.TargetData.Get(0)->GetHitResult();
			if (HitResult)
			{
				ContextHandle.AddHitResult(*HitResult);
                
				// 여기서 이펙트를 터뜨려도 됩니다!
				// UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodEffect, HitResult->ImpactPoint);
			}
		}

		// 데미지 꽂기!
		FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);
		if (SpecHandle.IsValid())
		{
			GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}
	}
}

void USRGA_RangedAttack::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
