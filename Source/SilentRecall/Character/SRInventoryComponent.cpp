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

bool USRInventoryComponent::AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, AActor* PickedUpWeaponActor)
{
    // 🚨 [스택 오버플로우 원천 봉쇄 마스터 가드]
    // FinishSpawning 도중 블루프린트 변수 초기화 덮어쓰기로 인해 발생하는 
    // 동기식 무한 오버랩 재귀 호출을 메모리 단에서 원천 차단합니다.
    static bool bReentrancyLock = false;
    if (bReentrancyLock) return false;

    if (!NewInstance || !NewInstance->WeaponData) return false;

    // 방어막 가동! (이 함수가 완전히 끝날 때까지 그 어떤 루팅 재귀 신호도 통과할 수 없습니다)
    bReentrancyLock = true;

    // =======================================================================
    // 🌟 [수정] 기존 무기 드롭 처리
    // =======================================================================
    if (WeaponLoadout.Contains(SlotType) && SpawnedWeapons.Contains(SlotType))
    {
        AActor* OldWeaponVisualActor = SpawnedWeapons[SlotType];
        USRWeaponInstance* OldInstance = WeaponLoadout[SlotType];

        if (OldWeaponVisualActor && OldInstance && OldInstance->WeaponData)
        {
            UClass* PickupClassToDrop = OldInstance->WeaponData->WeaponClass;
            
            FVector CameraLocation;
            FRotator CameraRotation;
            GetOwner()->GetActorEyesViewPoint(CameraLocation, CameraRotation);
            
            // 💡 [겹침 예방] 스폰 위치를 플레이어 몸체 한가운데(60cm)가 아닌, 정면 1.2미터(120cm) 앞 지점으로 안전하게 밀어냅니다.
            FVector SpawnLocation = CameraLocation + (CameraRotation.Vector() * 120.0f);
            FVector ThrowForce = (CameraRotation.Vector() * 450.0f) + (FVector::UpVector * 180.0f);

            AActor* SpawnedActor = GetWorld()->SpawnActorDeferred<AActor>(
                PickupClassToDrop, 
                FTransform(FRotator::ZeroRotator, SpawnLocation), 
                GetOwner(), 
                Cast<APawn>(GetOwner()), 
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn
            );

            if (SpawnedActor)
            {
                if (SpawnedActor->Implements<UItemStateInterface>())
                {
                    IItemStateInterface::Execute_SetDroppedAmmo(SpawnedActor, OldInstance->CurrentAmmoInMag);
                }

                ASRItemPickupBase* PickupBase = Cast<ASRItemPickupBase>(SpawnedActor);
                if (PickupBase)
                {
                    PickupBase->StartPickupCooldown(1.5f);
                }

                if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent()))
                {
                    RootPrim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
                }

                // 🟢 안전하게 액터 스폰 마감 (이 순간 내부적으로 오버랩이 터져도 위의 bReentrancyLock이 칼같이 튕겨냅니다!)
                SpawnedActor->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));

                if (PickupBase)
                {
                    PickupBase->InitDroppedItem(ThrowForce);
                }
            }

            OldWeaponVisualActor->Destroy(); 
        }
    }

    // =======================================================================
    // [2] 무기 비주얼 액터 생성 파트 (기존 동일)
    // =======================================================================
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

    if (CurrentActiveSlot == SlotType)
    {
        CurrentActiveSlot = EWeaponSlot::None;
    }

    RequestSwitchWeapon(SlotType, true); 

    // 모든 작업이 안전하게 끝났으므로 방어막 해제!
    bReentrancyLock = false;
    return true;
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

    // 1. 예비 탄약 데이터 즉시 복구
    AmmoReserve = GI->SavedAmmoReserve;

    // 2. 무기 로드아웃 루프 복구
    for (const auto& Pair : GI->SavedWeaponLoadout)
    {
        EWeaponSlot SlotType = Pair.Key;
        
        USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(this);
        NewInstance->InitializeInstance(Pair.Value.WeaponData, Pair.Value.CurrentAmmoInMag);

        // 💡 우리가 만든 AddWeapon을 호출하면 3P 비주얼 메시 스폰과 소켓 부착이 자동으로 재가동됩니다!
        AddWeapon(SlotType, NewInstance, nullptr);
    }

    // 3. 밟았던 당시 들고 있던 주무기/보조무기 슬롯 활성화 및 GA 재등록 유도
    if (GI->SavedActiveSlot != EWeaponSlot::None && WeaponLoadout.Contains(GI->SavedActiveSlot))
    {
        // bForceOverride = true를 주어 스왑 애니메이션과 함께 GA 주입 연산을 강제 격발시킵니다.
        RequestSwitchWeapon(GI->SavedActiveSlot, true);
    }
}

void USRInventoryComponent::RequestSwitchWeapon(EWeaponSlot NewSlot, bool bForceOverride)
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    
    // 🎯 [AI 전용 즉시 장착 고속도로] (기존 유지)
    if (OwnerChar && !OwnerChar->IsPlayerControlled())
    {
        NextSlotToEquip = NewSlot;
        PrepareWeaponSwitch(); 
        FinishEquip();         
        return;                
    }

    // 👤 이하 플레이어 전용 로직
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());

    // 🌟 [핵심 추가] 강제 스위칭 요청(무기 연속 습득 등)인데 현재 무언가 스왑 중이라면?
    // 기존에 돌고 있던 이전 무기의 스왑 어빌리티를 GAS 시스템에서 즉시 강제 취소시킵니다.
    if (bForceOverride && bIsSwitchingWeapon && ASC)
    {
        FGameplayTagContainer CancelTags;
        CancelTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.WeaponSwitch")));
        
        // 현재 돌고 있는 스왑 GA 강제 중단 격발 (OnSwitchInterrupted가 실행됨)
        ASC->CancelAbilities(&CancelTags);
        
        // 델리게이트 지연으로 인해 플래그가 한 프레임 늦게 꺼지는 것을 방지하기 위해 명시적으로 즉시 리셋
        bIsSwitchingWeapon = false;
    }

    // 💡 일반적인 키 입력 스왑일 때는 기존 가드가 정상 작동합니다.
    if (bIsSwitchingWeapon) return;
    
    // 🌟 [수정] 강제 장착(bForceOverride)일 때는 동일 슬롯이라도 메시 갱신을 위해 통과시킵니다.
    if (CurrentActiveSlot == NewSlot && !bForceOverride) return;

    bIsSwitchingWeapon = true;
    NextSlotToEquip = NewSlot;

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