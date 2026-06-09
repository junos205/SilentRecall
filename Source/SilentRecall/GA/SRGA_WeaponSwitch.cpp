#include "GA/SRGA_WeaponSwitch.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/SRInventoryComponent.h"
#include "GA/AT/SRAT_Play1PMontageAndWait.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"

USRGA_WeaponSwitch::USRGA_WeaponSwitch()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor; 
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.WeaponSwitch")));
}

void USRGA_WeaponSwitch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    CachedInventory = ActorInfo->AvatarActor->FindComponentByClass<USRInventoryComponent>();
	if (!CachedInventory)
	{
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

    // 🟢 [개편] 기존 Unequip 단계를 완전히 건너뛰고, 즉시 무기 스왑 데이터 및 소켓 처리 가동
    if (CachedInventory)
    {
        // 인벤토리에 이미 세팅되어 있는 NextSlotToEquip을 안전하게 꺼내와 인자로 주입!
        EWeaponSlot TargetSlot = CachedInventory->GetNextSlotToEquip();
    
        CachedInventory->PrepareWeaponSwitch(TargetSlot);
    }

    PlayEquipStep();
}

void USRGA_WeaponSwitch::PlayEquipStep()
{
    USRWeaponInstance* NewWeapon = CachedInventory ? CachedInventory->GetCurrentActiveWeaponInstance() : nullptr;
    if (!NewWeapon || !NewWeapon->WeaponData || !NewWeapon->WeaponData->EquipMontage)
    {
        OnEquipCompleted();
        return;
    }

    // 1️⃣ [수신 안테나 가동] 몽타주가 재생되다가 노티파이를 밟으면 작동할 수신 타스크를 먼저 켭니다.
    FGameplayTag EquipDoneTag = FGameplayTag::RequestGameplayTag(FName("Event.Weapon.EquipComplete"));
    UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EquipDoneTag);
    
    // 노티파이 무전이 도착하면 즉시 OnEquipCompleted를 실행하도록 바인딩!
    WaitEventTask->EventReceived.AddDynamic(this, &USRGA_WeaponSwitch::OnEquipEventReceived);
    WaitEventTask->ReadyForActivation();

    // 2️⃣ 1인칭 장착 몽타주 재생 개시
    UAnimMontage* EquipMontage = NewWeapon->WeaponData->EquipMontage;
    USRAT_Play1PMontageAndWait* EquipTask = USRAT_Play1PMontageAndWait::CreatePlay1PMontageAndWaitProxy(
        this, NAME_None, EquipMontage, 1.0f
    );

    if (EquipTask)
    {
        // 백업용 안전장치: 혹시라도 노티파이를 빼먹었을 때 몽타주가 다 끝나면 종료되도록 기존 완료 노선은 유지
        EquipTask->OnCompleted.AddDynamic(this, &USRGA_WeaponSwitch::OnEquipCompleted);
        EquipTask->ReadyForActivation();
        return;
    }

    OnEquipCompleted();
}

// 3️⃣ [신규 콜백 함수] 노티파이 무전을 받았을 때 처리할 장부
void USRGA_WeaponSwitch::OnEquipEventReceived(FGameplayEventData Payload)
{
    // 무전을 받는 즉시 몽타주가 남았든 말든 무기 스왑을 전격 완료 처리!
    OnEquipCompleted();
}

void USRGA_WeaponSwitch::OnEquipCompleted()
{
    if (CachedInventory)
    {
        CachedInventory->FinishEquip();
    }
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USRGA_WeaponSwitch::OnSwitchInterrupted()
{
    if (CachedInventory) { CachedInventory->FinishEquip(); }
    CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}