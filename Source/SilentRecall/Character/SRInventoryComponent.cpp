// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/SRInventoryComponent.h"
#include "Interface/SRCharacterInterface.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Interface/ItemStateInterface.h"
#include "GameFramework/Character.h"
#include "Weapon/SRItemPickupBase.h"
#include "Game/SRGameInstance.h"

USRInventoryComponent::USRInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool USRInventoryComponent::AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, AActor* PickedUpWeaponActor, bool bInitializeQuietly)
{
    static bool bReentrancyLock = false;
    if (bReentrancyLock) return false;
    if (!NewInstance || !NewInstance->WeaponData) return false;

    bReentrancyLock = true;

    // 조용히 초기화하는 상황(세이브 로드)이 아닐 때만 드롭 연산 작동
    if (!bInitializeQuietly && WeaponLoadout.Contains(SlotType) && SpawnedWeapons.Contains(SlotType))
    {
        AActor* OldWeaponVisualActor = SpawnedWeapons[SlotType];
        USRWeaponInstance* OldInstance = WeaponLoadout[SlotType];

        if (OldWeaponVisualActor && OldInstance && OldInstance->WeaponData)
        {
            UClass* PickupClassToDrop = OldInstance->WeaponData->WeaponClass;
            
            FVector CameraLocation; FRotator CameraRotation;
            GetOwner()->GetActorEyesViewPoint(CameraLocation, CameraRotation);
            
            FVector SpawnLocation = CameraLocation + (CameraRotation.Vector() * 120.0f);
            FVector ThrowForce = (CameraRotation.Vector() * 450.0f) + (FVector::UpVector * 180.0f);

            AActor* SpawnedActor = GetWorld()->SpawnActorDeferred<AActor>(
                PickupClassToDrop, FTransform(FRotator::ZeroRotator, SpawnLocation), GetOwner(), Cast<APawn>(GetOwner()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn
            );

            if (SpawnedActor)
            {
                if (SpawnedActor->Implements<UItemStateInterface>())
                {
                    IItemStateInterface::Execute_SetDroppedAmmo(SpawnedActor, OldInstance->CurrentAmmoInMag);
                }

                ASRItemPickupBase* PickupBase = Cast<ASRItemPickupBase>(SpawnedActor);
                if (PickupBase) PickupBase->StartPickupCooldown(1.5f);

                if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent()))
                {
                    RootPrim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
                }

                SpawnedActor->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));

                if (PickupBase) PickupBase->InitDroppedItem(ThrowForce);
            }
            OldWeaponVisualActor->Destroy(); 
        }
    }

    // 3P 무기 비주얼 스폰 및 장부 입력
    FActorSpawnParameters VisualSpawnParams;
    VisualSpawnParams.Owner = GetOwner();
    VisualSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* NewWeaponVisualActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), GetOwner()->GetActorLocation(), FRotator::ZeroRotator, VisualSpawnParams);
    
    if (NewWeaponVisualActor)
    {
        USkeletalMeshComponent* MeshComp = NewObject<USkeletalMeshComponent>(NewWeaponVisualActor, TEXT("WeaponSkeletalMesh"));
        MeshComp->RegisterComponent();
        NewWeaponVisualActor->SetRootComponent(MeshComp);

        if (NewInstance->WeaponData->WeaponMesh)
        {
            MeshComp->SetSkeletalMeshAsset(NewInstance->WeaponData->WeaponMesh);
            MeshComp->SetRelativeScale3D(NewInstance->WeaponData->WeaponScale);
        }
        if (NewInstance->WeaponData->WeaponMeshAnimClass)
        {
            MeshComp->SetAnimInstanceClass(NewInstance->WeaponData->WeaponMeshAnimClass);
        }
        
        MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    }

    WeaponLoadout.Add(SlotType, NewInstance);
    SpawnedWeapons.Add(SlotType, NewWeaponVisualActor);

    if (ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner()))
    {
        Char->AttachWeaponToHolster(NewWeaponVisualActor, NewInstance->WeaponData->HolsterSocketName);
    }

    // 세이브 로드 중에는 실시간 스왑 프로세스를 생략하고 조용히 복구만 마감
    if (bInitializeQuietly)
    {
        bReentrancyLock = false;
        return true;
    }

    if (CurrentActiveSlot == SlotType)
    {
        CurrentActiveSlot = EWeaponSlot::None;
    }

    RequestSwitchWeapon(SlotType, true); 

    bReentrancyLock = false;
    return true;
}

void USRInventoryComponent::RequestSwitchWeapon(EWeaponSlot NewSlot, bool bForceOverride)
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    
    // AI 전용 장착 처리 (인자 주입식 연동)
    if (OwnerChar && !OwnerChar->IsPlayerControlled())
    {
        NextSlotToEquip = NewSlot;
        PrepareWeaponSwitch(NewSlot); 
        FinishEquip();         
        return;                
    }

    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());

    if (bForceOverride && bIsSwitchingWeapon && ASC)
    {
        FGameplayTagContainer CancelTags;
        CancelTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.WeaponSwitch")));
        ASC->CancelAbilities(&CancelTags);
        bIsSwitchingWeapon = false;
    }

    if (bIsSwitchingWeapon) return;
    if (CurrentActiveSlot == NewSlot && !bForceOverride) return;

    bIsSwitchingWeapon = true;
    NextSlotToEquip = NewSlot;

    if (ASC)
    {
        FGameplayTag SwitchTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.WeaponSwitch"));
        if (!ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(SwitchTag)))
        {
            UE_LOG(LogTemp, Error, TEXT("[Inventory 🚨] 플레이어 스왑 GA 발동 실패 -> 강제 동기화 마감"));
            PrepareWeaponSwitch(NewSlot);
            FinishEquip(); 
        }
    }
}

void USRInventoryComponent::PrepareWeaponSwitch(EWeaponSlot TargetSlot)
{
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner());
    
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

    if (Char && CurrentActiveSlot != EWeaponSlot::None && SpawnedWeapons.Contains(CurrentActiveSlot))
    {
        Char->AttachWeaponToHolster(SpawnedWeapons[CurrentActiveSlot], WeaponLoadout[CurrentActiveSlot]->WeaponData->HolsterSocketName);
    }

    // 🌟 원자적 인자 값 강제 갱신
    CurrentActiveSlot = TargetSlot;

    AActor* NewWeaponActor = SpawnedWeapons.Contains(CurrentActiveSlot) ? SpawnedWeapons[CurrentActiveSlot] : nullptr;
    USRWeaponInstance* NewInstance = WeaponLoadout.Contains(CurrentActiveSlot) ? WeaponLoadout[CurrentActiveSlot] : nullptr;

    if (NewWeaponActor && NewInstance && NewInstance->WeaponData)
    {
        // 🌟 [컴파일 완치] 1개짜리 시그니처로 복구되었으므로 에러 없이 정상 브로드캐스트 작동!
        OnWeaponChanged.Broadcast(NewInstance->WeaponData); 
        
        if (Char)
        {
            Char->AttachWeaponToHands(NewWeaponActor, NewInstance->WeaponData->EquipSocketName);
        }

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

    // 장탄수 변경 내역을 4인자 전용 UI 채널로 실시간 송출
    RefreshWeaponHUD();
}

void USRInventoryComponent::FinishEquip()
{
    if (!bIsSwitchingWeapon) return;
    bIsSwitchingWeapon = false; 

    // 🌟 [종속성 완치] 특정 캐릭터 클래스 대신 '인터페이스'로만 소통합니다!
    if (ISRCharacterInterface* CharInterface = Cast<ISRCharacterInterface>(GetOwner()))
    {
        CharInterface->ApplyWeaponAnimLayer();
    }
}

void USRInventoryComponent::SaveToGameInstance(USRGameInstance* GI)
{
    if (!GI) return;

    GI->SavedWeaponLoadout.Empty();
    for (const auto& Pair : WeaponLoadout)
    {
        if (Pair.Value && Pair.Value->WeaponData)
        {
            FSRSavedWeaponInfo Info;
            Info.WeaponData = Pair.Value->WeaponData;
            Info.CurrentAmmoInMag = Pair.Value->CurrentAmmoInMag;
            GI->SavedWeaponLoadout.Add(Pair.Key, Info);
        }
    }
    GI->SavedActiveSlot = CurrentActiveSlot;
    GI->SavedAmmoReserve = AmmoReserve;
}

void USRInventoryComponent::LoadFromGameInstance(USRGameInstance* GI)
{
    if (!GI) return;

    AmmoReserve = GI->SavedAmmoReserve;

    // 🌟 bInitializeQuietly = true를 전송해 스왑 능력 중첩 오버랩 버그 철저히 예방
    for (const auto& Pair : GI->SavedWeaponLoadout)
    {
        EWeaponSlot SlotType = Pair.Key;
        
        USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(this);
        NewInstance->InitializeInstance(Pair.Value.WeaponData, Pair.Value.CurrentAmmoInMag);

        AddWeapon(SlotType, NewInstance, nullptr, true);
    }

    if (GI->SavedActiveSlot != EWeaponSlot::None && WeaponLoadout.Contains(GI->SavedActiveSlot))
    {
        RequestSwitchWeapon(GI->SavedActiveSlot, true);
    }
}

void USRInventoryComponent::RefreshWeaponHUD()
{
    USRWeaponInstance* ActiveWeapon = GetCurrentActiveWeaponInstance();
    
    if (!ActiveWeapon || !ActiveWeapon->WeaponData)
    {
        OnWeaponHUDChanged.Broadcast(nullptr, 0, 0, false);
        return;
    }

    bool bIsRanged = ActiveWeapon->WeaponData->MaxAmmoInMag > 0;
    int32 CurrentAmmo = ActiveWeapon->CurrentAmmoInMag;
    
    // 🌟 장부 개념이 바뀌었으므로, 이 값은 이제 낱발 수가 아닌 "남은 탄창 개수" 그 자체입니다!
    int32 ReserveMags = GetReserveAmmo(ActiveWeapon->WeaponData->WeaponTypeTag);

    // 📡 4인자 무전기로 전송 (세 번째 인자에 남은 탄창 통 개수가 담겨 날아감)
    OnWeaponHUDChanged.Broadcast(ActiveWeapon->WeaponData, CurrentAmmo, ReserveMags, bIsRanged);
}

int32 USRInventoryComponent::GetReserveAmmo(FGameplayTag AmmoTag) const
{
    return AmmoReserve.Contains(AmmoTag) ? AmmoReserve[AmmoTag] : 0;
}

void USRInventoryComponent::AddReserveAmmo(FGameplayTag AmmoTag, int32 Amount)
{
    // 에디터에서 설정할 MaxCap 역시 이제 최대 탄창 보유량(예: 소총 탄창 최대 5개 보유 가능)이 됩니다.
    int32 MaxCap = MaxAmmoCapacity.Contains(AmmoTag) ? MaxAmmoCapacity[AmmoTag] : 9;
    int32 Current = AmmoReserve.Contains(AmmoTag) ? AmmoReserve[AmmoTag] : 0;
    
    // Amount만큼 탄창 개수 누적 추가
    AmmoReserve.Add(AmmoTag, FMath::Clamp(Current + Amount, 0, MaxCap));
    
    RefreshWeaponHUD();
}

void USRInventoryComponent::ReloadCurrentWeapon()
{
    USRWeaponInstance* CurrentWeapon = GetCurrentActiveWeaponInstance();
    if (!CurrentWeapon || !CurrentWeapon->WeaponData) return;

    FGameplayTag AmmoTag = CurrentWeapon->WeaponData->WeaponTypeTag;
    int32 CurrentMag = CurrentWeapon->CurrentAmmoInMag;
    int32 MaxMag = CurrentWeapon->WeaponData->MaxAmmoInMag;

    // 🔒 검문소: 이미 총에 총알이 꽉 찼거나, 인벤토리에 남은 탄asure 탄창 통이 0개라면 장전 불가!
    if (CurrentMag >= MaxMag || GetReserveAmmo(AmmoTag) <= 0) return;

    // 💥 [핵심] 보유 중인 탄창 개수에서 깔끔하게 1개를 차감(소비)합니다.
    AmmoReserve[AmmoTag]--;

    // 총에 꽂힌 현재 장탄수는 무기 원본 데이터의 최대치로 풀 충전!
    CurrentWeapon->CurrentAmmoInMag = MaxMag;

    // 장부 변경 사항을 UI에 즉시 실시간 방송
    RefreshWeaponHUD();
}

void USRInventoryComponent::CycleWeapon(bool bNext)
{
    if (bIsSwitchingWeapon) return;
    TArray<EWeaponSlot> AvailableSlots;
    WeaponLoadout.GetKeys(AvailableSlots);
    if (AvailableSlots.Num() == 0) return;
    if (CurrentActiveSlot == EWeaponSlot::None) { RequestSwitchWeapon(AvailableSlots[0]); return; }
    if (AvailableSlots.Num() <= 1) return;

    int32 CurrentIndex = AvailableSlots.Find(CurrentActiveSlot);
    int32 NextIndex = 0;
    if (CurrentIndex != INDEX_NONE)
    {
        if (bNext) NextIndex = (CurrentIndex + 1) % AvailableSlots.Num();
        else NextIndex = (CurrentIndex - 1 + AvailableSlots.Num()) % AvailableSlots.Num();
    }
    RequestSwitchWeapon(AvailableSlots[NextIndex]);
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
            if (Comp) Comp->SetVisibility(bNewVisibility, true);
        }
    }
}