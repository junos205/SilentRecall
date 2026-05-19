// Fill out your copyright notice in the Description page of Project Settings.


#include "SRGA_Reload.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/SRPlayerCharacter.h"
#include "Character/SRInventoryComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"

USRGA_Reload::USRGA_Reload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
	// (선택) 장전 태그 부여. 이 어빌리티가 켜져 있는 동안 캐릭터는 이 태그를 가집니다.
	// 이 태그를 통해 사격(Fire) GA가 발동되지 않도록 블로킹할 수 있습니다.
	// AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Reload")));
}

bool USRGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;

	// ⭐️ 발동 전, 장전이 진짜로 필요한 상황인지 검사합니다!
	ASRPlayerCharacter* Player = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!Player) return false;

	USRInventoryComponent* Inventory = Player->FindComponentByClass<USRInventoryComponent>();
	if (!Inventory) return false;

	USRWeaponInstance* WeaponInstance = Inventory->GetCurrentActiveWeaponInstance();
	if (!WeaponInstance || !WeaponInstance->WeaponData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reload] Cannot reload: No active weapon instance or weapon data."));
		return false;
	}

	int32 CurrentMag = WeaponInstance->CurrentAmmoInMag;
	int32 MaxMag = WeaponInstance->WeaponData->MaxAmmoInMag;
	int32 ReserveAmmo = Inventory->GetReserveAmmo(WeaponInstance->WeaponData->WeaponTypeTag);

	// 탄창이 꽉 찼거나, 예비 탄약이 없으면 장전 불가
	if (CurrentMag >= MaxMag || ReserveAmmo <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Reload] Cannot reload: CurrentMag=%d, MaxMag=%d, ReserveAmmo=%d"), CurrentMag, MaxMag, ReserveAmmo);
		
		return false;
	}
	return true;
}

void USRGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ASRPlayerCharacter* Player = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
	USRInventoryComponent* Inventory = Player ? Player->FindComponentByClass<USRInventoryComponent>() : nullptr;
    
	if (Inventory && Inventory->GetCurrentActiveWeaponInstance())
	{
		// 1. 데이터 애셋에서 장전 몽타주 가져오기 (데이터 애셋에 ReloadMontage 추가 필요)
		UAnimMontage* ReloadMontage = Inventory->GetCurrentActiveWeaponInstance()->WeaponData->ReloadMontage;

		if (ReloadMontage)
		{
			// ⭐️ 2. 애니메이션 몽타주 실행 태스크
			UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ReloadMontage);
            
			MontageTask->OnCompleted.AddDynamic(this, &USRGA_Reload::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &USRGA_Reload::OnMontageCancelled);
			MontageTask->OnCancelled.AddDynamic(this, &USRGA_Reload::OnMontageCancelled);
            
			MontageTask->ReadyForActivation();

			// ⭐️ 3. 애니메이션 노티파이(GameplayEvent) 대기 태스크
			// 탄창을 꽂는 정확한 타이밍에 이벤트를 받기 위해 기다립니다.
			UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ReloadEventTag, nullptr, false, false);
            
			WaitEventTask->EventReceived.AddDynamic(this, &USRGA_Reload::OnReloadEventReceived);
            
			WaitEventTask->ReadyForActivation();
            
			return; // 정상적으로 태스크들이 실행됨
		}
	}

	// 몽타주가 없거나 실패하면 어빌리티 즉시 종료
	UE_LOG(LogTemp, Warning, TEXT("[Reload] No Reload Montage found. Ending ability immediately."));
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void USRGA_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USRGA_Reload::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_Reload::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_Reload::OnReloadEventReceived(FGameplayEventData Payload)
{
	if (ASRPlayerCharacter* Player = Cast<ASRPlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (USRInventoryComponent* Inventory = Player->FindComponentByClass<USRInventoryComponent>())
		{
			// 이전에 만들어둔 실제 탄약 계산 로직 호출!
			Inventory->ReloadCurrentWeapon();
		}
	}
}
