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
    if (!PickedUpWeaponActor || !NewInstance) return false;

    // 1. 기존 무기 드롭 처리
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

    // 2. 물리 정지
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

    // 🎯 [개편] 기존에 있던 AI 전용 즉시 장착 분기점(If문)을 과감히 제거했습니다.
    // 이제 플레이어와 AI 모두 동일하게 일단 무기를 홀스터에 붙이고 정식 스왑 요청 파이프라인을 탑니다.
    if (ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner()))
    {
        Char->AttachWeaponToHolster(PickedUpWeaponActor, NewInstance->WeaponData->HolsterSocketName);
    }

    if (CurrentActiveSlot == SlotType)
    {
        CurrentActiveSlot = EWeaponSlot::None;
    }

    RequestSwitchWeapon(SlotType);
    return true;
}

void USRInventoryComponent::RequestSwitchWeapon(EWeaponSlot NewSlot)
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    
    // 🎯 [AI 전용 즉시 장착 고속도로]
    // AI는 플레이어 전용 1인칭 몽타주 태스크를 실행할 수 없으므로,
    // 어빌리티 발동 단계를 건너뛰고 즉시 시각적/기능적 장착을 동기(Synchronous)로 처리합니다.
    if (OwnerChar && !OwnerChar->IsPlayerControlled())
    {
        NextSlotToEquip = NewSlot;
        PrepareWeaponSwitch(); // 내부에서 슬롯이 바뀌고 무기가 손(Hands) 소켓에 즉시 달라붙습니다!
        FinishEquip();         // 즉시 기능 활성화 및 플래그 정리
        return;                // 💡 AI는 여기서 무기를 완벽히 쥔 채로 즉시 리턴합니다.
    }

    // 👤 이하 플레이어 전용 로직 (기존 가드 및 플레이어 GA 발동 파이프라인)
    if (bIsSwitchingWeapon) return;
    if (CurrentActiveSlot == NewSlot) return;

    bIsSwitchingWeapon = true;
    NextSlotToEquip = NewSlot;

    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    if (ASC)
    {
        FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.WeaponSwitch"));
        if (!ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(SwitchTag)))
        {
            UE_LOG(LogTemp, Error, TEXT("[Inventory 🚨] 플레이어 스왑 GA 발동 실패 -> 강제 동기화 마감"));
            PrepareWeaponSwitch();
            FinishEquip(); 
        }
    }
}

void USRInventoryComponent::PrepareWeaponSwitch()
{
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner());
    
    // 1. 기존 장착 무기의 GAS 어빌리티 및 태그 해제
    if (ASC)
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

    // 2. 기존 무기 액터를 등 뒤(Holster)로 안전하게 돌려보내기
    if (Char && CurrentActiveSlot != EWeaponSlot::None && SpawnedWeapons.Contains(CurrentActiveSlot))
    {
        Char->AttachWeaponToHolster(SpawnedWeapons[CurrentActiveSlot], WeaponLoadout[CurrentActiveSlot]->WeaponData->HolsterSocketName);
    }

    // 3. 활성 슬롯을 목적지 슬롯으로 전환
    CurrentActiveSlot = NextSlotToEquip;

    AActor* NewWeaponActor = SpawnedWeapons.Contains(CurrentActiveSlot) ? SpawnedWeapons[CurrentActiveSlot] : nullptr;
    USRWeaponInstance* NewInstance = WeaponLoadout.Contains(CurrentActiveSlot) ? WeaponLoadout[CurrentActiveSlot] : nullptr;

    if (NewWeaponActor && NewInstance && NewInstance->WeaponData)
    {
        // 🌟 [순서 개편 핵심] 무조건 비주얼 부착과 델리게이트 브로드캐스트를 1순위로 실행합니다!
        // 어빌리티 주입 연산보다 먼저 손에 고정해 두기 때문에, 프레임 드랍이나 애니메이션 레이어 초기화 타이밍에 부착이 씹히지 않습니다.
        OnWeaponChanged.Broadcast(NewInstance->WeaponData); 
        if (Char)
        {
            Char->AttachWeaponToHands(NewWeaponActor, NewInstance->WeaponData->EquipSocketName);
        }

        // 4. 비주얼이 안전하게 달라붙은 직후에 새 무기의 공격 어빌리티를 안심하고 주입합니다.
        if (ASC)
        {
            for (auto& Ability : NewInstance->WeaponData->GrantedAbilities)
            {
                if (Ability.Value == nullptr) continue;
                
                FGameplayAbilitySpec Spec(Ability.Value, 1, static_cast<int32>(Ability.Key), NewInstance);
                CurrentGrantedAbilityHandles.Add(ASC->GiveAbility(Spec));
            }
            ASC->AddLooseGameplayTag(NewInstance->WeaponData->WeaponTypeTag);
        }
    }
}

void USRInventoryComponent::FinishEquip()
{
    if (!bIsSwitchingWeapon) return;

    // 🎯 어빌리티와 태그는 이미 PrepareWeaponSwitch에서 처리되었으므로
    // 여기서는 오직 스왑 중인 플래그 상태만 정리하여 공격 제한을 해제합니다.
    bIsSwitchingWeapon = false; 
}

// ... 하단 함수들 (CycleWeapon, Ammo 관련 등)은 변경 없이 기존 코드 그대로 유지 ...
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