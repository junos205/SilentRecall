#include "SRInventoryComponent.h"
#include "Interface/SRCharacterInterface.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Interface/ItemStateInterface.h"

USRInventoryComponent::USRInventoryComponent() {}

bool USRInventoryComponent::AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, AActor* PickedUpWeaponActor)
{
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] AddWeapon Called. SlotType: %d"), (int32)SlotType);

    if (!PickedUpWeaponActor || !NewInstance) return false;

    // 기존 무기 드롭 & 장탄수 인계
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

    // 새 무기 물리 끄기 및 등록
    if (UPrimitiveComponent* NewRoot = Cast<UPrimitiveComponent>(PickedUpWeaponActor->GetRootComponent()))
    {
        NewRoot->SetSimulatePhysics(false); 
        NewRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    
    WeaponLoadout.Add(SlotType, NewInstance);
    SpawnedWeapons.Add(SlotType, PickedUpWeaponActor);

    if (ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner()))
    {
        Char->AttachWeaponToHolster(PickedUpWeaponActor, NewInstance->WeaponData->HolsterSocketName);
    }

    if (CurrentActiveSlot == SlotType)
    {
        CurrentActiveSlot = EWeaponSlot::None;
    }

    UE_LOG(LogTemp, Warning, TEXT("[Inventory] AddWeapon Success. Calling RequestSwitchWeapon"));
    RequestSwitchWeapon(SlotType);

    return true;
}

void USRInventoryComponent::RequestSwitchWeapon(EWeaponSlot NewSlot)
{
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] RequestSwitchWeapon Called. TargetSlot: %d, bIsSwitchingWeapon: %d"), (int32)NewSlot, bIsSwitchingWeapon);

    if (bIsSwitchingWeapon || CurrentActiveSlot == NewSlot) return;
    
    bIsSwitchingWeapon = true;
    NextSlotToEquip = NewSlot;
    
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] Calling BeginUnEquip"));
    BeginUnEquip();
}

void USRInventoryComponent::BeginUnEquip()
{
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] BeginUnEquip Called"));

    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
    {
        for (auto& Handle : CurrentGrantedAbilityHandles) ASC->ClearAbility(Handle);
        CurrentGrantedAbilityHandles.Empty();

        // ⭐️ [수정됨] CurrentActiveSlot이 None이 아닐 때, 그리고 Map에 데이터가 확실히 있을 때만 접근!
        if (CurrentActiveSlot != EWeaponSlot::None && WeaponLoadout.Contains(CurrentActiveSlot))
        {
            FGameplayTag OldWeaponTag = WeaponLoadout[CurrentActiveSlot]->WeaponData->WeaponTypeTag;
            ASC->RemoveLooseGameplayTag(OldWeaponTag);
        }
    }

    if (CurrentActiveSlot != EWeaponSlot::None)
    {
        if (ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner()))
        {
            // 여기도 안전망(Contains)이 있으면 더 좋습니다!
            if (WeaponLoadout.Contains(CurrentActiveSlot) && WeaponLoadout[CurrentActiveSlot]->WeaponData)
            {
                UAnimMontage* MontageToPlay = WeaponLoadout[CurrentActiveSlot]->WeaponData->UnEquipMontage;
                if (MontageToPlay)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Inventory] Playing UnEquip Montage and Waiting"));
                    Char->PlayWeaponMontage(MontageToPlay);
                    return; 
                }
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] No UnEquip Montage. Calling FinishUnEquip directly"));
    FinishUnEquip();
}

void USRInventoryComponent::FinishUnEquip()
{
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] FinishUnEquip Called"));

    if (CurrentActiveSlot == NextSlotToEquip) return; 

    ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner());
    if (!Char) return;

    if (CurrentActiveSlot != EWeaponSlot::None)
    {
        Char->AttachWeaponToHolster(SpawnedWeapons[CurrentActiveSlot], WeaponLoadout[CurrentActiveSlot]->WeaponData->HolsterSocketName);
    }

    CurrentActiveSlot = NextSlotToEquip;
    AActor* NewWeapon = SpawnedWeapons[CurrentActiveSlot];
    USRWeaponInstance* NewInstance = WeaponLoadout[CurrentActiveSlot];

    if (NewWeapon && NewInstance && NewInstance->WeaponData)
    {
        Char->AttachWeaponToHands(NewWeapon, NewInstance->WeaponData->EquipSocketName);
        OnWeaponChanged.Broadcast(NewInstance->WeaponData); 

        UAnimMontage* EquipMontage = NewInstance->WeaponData->EquipMontage;
        if (EquipMontage)
        {
            UE_LOG(LogTemp, Warning, TEXT("[Inventory] Playing Equip Montage and Waiting"));
            Char->PlayWeaponMontage(EquipMontage);
            return; 
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] No Equip Montage. Calling FinishEquip directly"));
    FinishEquip();
}

void USRInventoryComponent::FinishEquip()
{
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] FinishEquip Called"));

    if (!bIsSwitchingWeapon) return;

    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
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
    
    bIsSwitchingWeapon = false;
    UE_LOG(LogTemp, Warning, TEXT("[Inventory] Switch Complete. bIsSwitchingWeapon set to false"));
}

void USRInventoryComponent::CycleWeapon(bool bNext)
{
    // 순환 로직 (기존 유지)
}

// ==========================================
// 탄약 관리 로직
// ==========================================
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

    UE_LOG(LogTemp, Warning, TEXT("[Inventory] Reloaded %d ammo. Remaining Reserve: %d"), AmountToReload, AmmoReserve[AmmoTag]);
}