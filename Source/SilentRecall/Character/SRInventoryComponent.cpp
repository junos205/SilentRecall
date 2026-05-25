// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/SRInventoryComponent.h"
#include "Interface/SRCharacterInterface.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Interface/ItemStateInterface.h"
#include "GameFramework/Character.h"

USRInventoryComponent::USRInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool USRInventoryComponent::AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, AActor* PickedUpWeaponActor)
{
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] ====== AddWeapon 시작 ======"));

    if (!PickedUpWeaponActor || !NewInstance) return false;

    // 1. 기존 무기 드롭 처리 (유저님의 원래 명품 피지컬 드롭 코드 100% 원형 복구)
    if (WeaponLoadout.Contains(SlotType) && SpawnedWeapons.Contains(SlotType))
    {
        AActor* OldWeaponActor = SpawnedWeapons[SlotType];
        USRWeaponInstance* OldInstance = WeaponLoadout[SlotType];

        if (OldWeaponActor && OldInstance && OldInstance->WeaponData)
        {
            UClass* PickupClassToDrop = OldInstance->WeaponData->WeaponClass;
            FVector DropLoc = GetOwner()->GetActorLocation() + (GetOwner()->GetActorForwardVector() * 100.0f) + FVector(0, 0, 50.0f);
            
            AActor* NewDrop = GetWorld()->SpawnActor<AActor>(PickupClassToDrop, DropLoc, FRotator::ZeroRotator);
            if (NewDrop)
            {
                if (NewDrop->Implements<UItemStateInterface>())
                {
                    IItemStateInterface::Execute_SetDroppedAmmo(NewDrop, OldInstance->CurrentAmmoInMag);
                }

                if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(NewDrop->GetRootComponent()))
                {
                    Root->SetSimulatePhysics(true);
                    Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                    Root->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
                    Root->AddImpulse(GetOwner()->GetActorForwardVector() * 150.0f, NAME_None, true);
                }
            }
            OldWeaponActor->Destroy(); 
        }
    }

    // 2. 물리 및 스켈레탈 메쉬 내부 피지크 바디 정지 (유저님 순정 방식)
    if (UPrimitiveComponent* NewRoot = Cast<UPrimitiveComponent>(PickedUpWeaponActor->GetRootComponent()))
    {
        NewRoot->SetSimulatePhysics(false); 
        NewRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        if (USkeletalMeshComponent* SkelRoot = Cast<USkeletalMeshComponent>(NewRoot))
        {
            SkelRoot->SetAllBodiesSimulatePhysics(false);
        }
    }
    
    WeaponLoadout.Add(SlotType, NewInstance);
    SpawnedWeapons.Add(SlotType, PickedUpWeaponActor);

    // =====================================================================
    // ⭐️ [적 AI 전용 차선] 널 포인터 유발하던 타이머 싹 다 청소 완료!
    // =====================================================================
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar && !OwnerChar->IsPlayerControlled())
    {
        CurrentActiveSlot = SlotType;

        if (ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner()))
        {
            // 🟢 1. 타이머 없이 즉시 애님 레이어를 먼저 링크해서 손 뼈대 구조를 안정화시킵니다.
            OnWeaponChanged.Broadcast(NewInstance->WeaponData); 
            
            // 🟢 2. 구조가 확정된 메쉬의 정답 손 소켓("HandGrip_R")에 무기를 최종 부착합니다!
            Char->AttachWeaponToHands(PickedUpWeaponActor, NewInstance->WeaponData->EquipSocketName);
        }

        if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
        {
            for (auto& Ability : NewInstance->WeaponData->GrantedAbilities)
            {
                FGameplayAbilitySpec Spec(Ability.Value, 1, static_cast<int32>(Ability.Key), NewInstance);
                ASC->GiveAbility(Spec);
            }
            ASC->AddLooseGameplayTag(NewInstance->WeaponData->WeaponTypeTag);
        }

        bIsSwitchingWeapon = false; 
        return true; // 🚨 AI는 여기서 중복 간섭 없이 정석 리턴 탈출!
    }

    // 오직 플레이어만 통과하는 정석 등 뒤(Holster) 부착 및 스왑 시퀀스
    if (ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner()))
    {
        Char->AttachWeaponToHolster(PickedUpWeaponActor, NewInstance->WeaponData->HolsterSocketName);
    }

    if (CurrentActiveSlot == SlotType)
    {
        CurrentActiveSlot = EWeaponSlot::None;
    }

    WeaponSwitchUnEquipMontage = nullptr;
    USRWeaponInstance* CurrentActiveWeapon = GetWeaponInSlot(CurrentActiveSlot);
    if (CurrentActiveWeapon && CurrentActiveWeapon->WeaponData)
    {
        WeaponSwitchUnEquipMontage = CurrentActiveWeapon->WeaponData->UnEquipMontage;
    }

    RequestSwitchWeapon(SlotType);
    return true;
}

void USRInventoryComponent::RequestSwitchWeapon(EWeaponSlot NewSlot)
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar && !OwnerChar->IsPlayerControlled()) return;

    if (bIsSwitchingWeapon) return;
    if (CurrentActiveSlot == NewSlot) return;

    if (WeaponSwitchUnEquipMontage == nullptr && CurrentActiveSlot != EWeaponSlot::None)
    {
        USRWeaponInstance* ActiveWeapon = GetWeaponInSlot(CurrentActiveSlot);
        if (ActiveWeapon && ActiveWeapon->WeaponData)
        {
            WeaponSwitchUnEquipMontage = ActiveWeapon->WeaponData->UnEquipMontage;
            if (!WeaponSwitchUnEquipMontage) WeaponSwitchUnEquipMontage = ActiveWeapon->WeaponData->EquipMontage;
        }
    }

    bIsSwitchingWeapon = true;
    NextSlotToEquip = NewSlot;

    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    if (ASC)
    {
        FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.WeaponSwitch"));
        if (!ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(SwitchTag)))
        {
            UE_LOG(LogTemp, Error, TEXT("[Inventory 🚨] 플레이어 스왑 GA 발동 실패 -> 즉시 강제 마감 폴백"));
            WeaponSwitchUnEquipMontage = nullptr;
            BeginUnEquip();
            FinishUnEquip();
            FinishEquip(); 
        }
    }
}

void USRInventoryComponent::ExecuteWeaponSwitchPipeline(EWeaponSlot NewSlot, UAnimMontage* UnEquipMontageToPlay)
{
    bIsSwitchingWeapon = true;
    NextSlotToEquip = NewSlot;

    AActor* OwnerActor = GetOwner();
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
    
    if (ASC)
    {
        FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.WeaponSwitch"));
        
        FGameplayEventData Payload;
        Payload.EventTag = SwitchTag;
        Payload.OptionalObject = UnEquipMontageToPlay; 
        Payload.Instigator = OwnerActor;
        Payload.Target = OwnerActor;

        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, SwitchTag, Payload);
    }
    else
    {
        BeginUnEquip();
        FinishUnEquip();
        FinishEquip();
    }
}

void USRInventoryComponent::BeginUnEquip()
{
    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
    {
        for (auto& Handle : CurrentGrantedAbilityHandles) 
        {
            ASC->ClearAbility(Handle);
        }
        CurrentGrantedAbilityHandles.Empty();

        if (CurrentActiveSlot != EWeaponSlot::None && WeaponLoadout.Contains(CurrentActiveSlot))
        {
            FGameplayTag OldWeaponTag = WeaponLoadout[CurrentActiveSlot]->WeaponData->WeaponTypeTag;
            ASC->RemoveLooseGameplayTag(OldWeaponTag);
        }
    }
}

void USRInventoryComponent::FinishUnEquip()
{
    WeaponSwitchUnEquipMontage = nullptr;

    ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner());
    if (!Char) return;
    if (CurrentActiveSlot != EWeaponSlot::None && SpawnedWeapons.Contains(CurrentActiveSlot))
    {
        Char->AttachWeaponToHolster(SpawnedWeapons[CurrentActiveSlot], WeaponLoadout[CurrentActiveSlot]->WeaponData->HolsterSocketName);
    }

    CurrentActiveSlot = NextSlotToEquip;
    AActor* NewWeapon = SpawnedWeapons.Contains(CurrentActiveSlot) ? SpawnedWeapons[CurrentActiveSlot] : nullptr;
    USRWeaponInstance* NewInstance = WeaponLoadout.Contains(CurrentActiveSlot) ? WeaponLoadout[CurrentActiveSlot] : nullptr;

    if (NewWeapon && NewInstance && NewInstance->WeaponData)
    {
        // 🟢 플레이어 차선 순서 교정 완료
        OnWeaponChanged.Broadcast(NewInstance->WeaponData); 
        Char->AttachWeaponToHands(NewWeapon, NewInstance->WeaponData->EquipSocketName);
    }
}

void USRInventoryComponent::FinishEquip()
{
    if (!bIsSwitchingWeapon) return;

    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
    {
        if (WeaponLoadout.Contains(CurrentActiveSlot))
        {
            USRWeaponInstance* NewInstance = WeaponLoadout[CurrentActiveSlot];
            if (NewInstance && NewInstance->WeaponData)
            {
                for (auto& Ability : NewInstance->WeaponData->GrantedAbilities)
                {
                    FGameplayAbilitySpec Spec(Ability.Value, 1, static_cast<int32>(Ability.Key), NewInstance);
                    CurrentGrantedAbilityHandles.Add(ASC->GiveAbility(Spec));
                }
                ASC->AddLooseGameplayTag(NewInstance->WeaponData->WeaponTypeTag);
            }
        }
    }
    bIsSwitchingWeapon = false; 
}

void USRInventoryComponent::CycleWeapon(bool bNext)
{
    if (bIsSwitchingWeapon) return;

    TArray<EWeaponSlot> AvailableSlots;
    WeaponLoadout.GetKeys(AvailableSlots);

    if (AvailableSlots.Num() == 0) return;

    if (CurrentActiveSlot == EWeaponSlot::None)
    {
        RequestSwitchWeapon(AvailableSlots[0]);
        return;
    }

    if (AvailableSlots.Num() <= 1) return;

    int32 CurrentIndex = AvailableSlots.Find(CurrentActiveSlot);
    int32 NextIndex = 0;

    if (CurrentIndex != INDEX_NONE)
    {
        if (bNext) NextIndex = (CurrentIndex + 1) % AvailableSlots.Num();
        else NextIndex = (CurrentIndex - 1 + AvailableSlots.Num()) % AvailableSlots.Num();
    }

    EWeaponSlot TargetSlot = AvailableSlots[NextIndex];
    RequestSwitchWeapon(TargetSlot);
}

void USRInventoryComponent::AddReserveAmmo(FGameplayTag AmmoTag, int32 Amount)
{
    int32 MaxCap = MaxAmmoCapacity.Contains(AmmoTag) ? MaxAmmoCapacity[AmmoTag] : 999;
    int32 Current = AmmoReserve.Contains(AmmoTag) ? AmmoReserve[AmmoTag] : 0;
    
    AmmoReserve.Add(AmmoTag, FMath::Clamp(Current + Amount, 0, MaxCap));
}

int32 USRInventoryComponent::GetReserveAmmo(FGameplayTag AmmoTag) const
{
    return AmmoReserve.Contains(AmmoTag) ? AmmoReserve[AmmoTag] : 0;
}

void USRInventoryComponent::ReloadCurrentWeapon()
{
    USRWeaponInstance* CurrentWeapon = GetCurrentActiveWeaponInstance();
    if (!CurrentWeapon || !CurrentWeapon->WeaponData) return;

    FGameplayTag AmmoTag = CurrentWeapon->WeaponData->WeaponTypeTag;
    int32 CurrentMag = CurrentWeapon->CurrentAmmoInMag;
    int32 MaxMag = CurrentWeapon->WeaponData->MaxAmmoInMag;

    if (CurrentMag >= MaxMag || !AmmoReserve.Contains(AmmoTag) || AmmoReserve[AmmoTag] <= 0) return;

    int32 AmountNeeded = MaxMag - CurrentMag;
    int32 AvailableFromReserve = AmmoReserve[AmmoTag];
    int32 AmountToReload = FMath::Min(AmountNeeded, AvailableFromReserve);

    CurrentWeapon->CurrentAmmoInMag += AmountToReload;
    AmmoReserve[AmmoTag] -= AmountToReload;
}

void USRInventoryComponent::SetCurrentActiveWeaponVisibility(bool bNewVisibility)
{
    AActor* ActiveWeaponActor = GetCurrentActiveWeaponActor();
    if (ActiveWeaponActor)
    {
        ActiveWeaponActor->SetActorHiddenInGame(!bNewVisibility);
        
        TArray<USceneComponent*> Components;
        ActiveWeaponActor->GetComponents<USceneComponent>(Components);
        for (USceneComponent* Comp : Components)
        {
            if (Comp)
            {
                Comp->SetVisibility(bNewVisibility, true);
            }
        }
    }
}