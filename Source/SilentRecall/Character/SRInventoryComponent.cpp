#include "Character/SRInventoryComponent.h"
#include "Interface/SRCharacterInterface.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Interface/ItemStateInterface.h"
#include "GameFramework/Character.h"
#include "Weapon/SRItemPickupBase.h"

USRInventoryComponent::USRInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool USRInventoryComponent::AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, AActor* PickedUpWeaponActor)
{
    if (!NewInstance || !NewInstance->WeaponData) return false;

    // =======================================================================
    // 🌟 [수정] 기존 무기 드롭 처리 (지연 스폰을 통한 스택 오버플로우 크래시 해결)
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
            
            FVector SpawnLocation = CameraLocation + (CameraRotation.Vector() * 60.0f);
            FVector ThrowForce = (CameraRotation.Vector() * 450.0f) + (FVector::UpVector * 180.0f);

            // 🚨 [Stack Overflow 해결 핵심] SpawnActor 대신 SpawnActorDeferred를 사용합니다.
            // 이 함수는 액터의 메모리만 할당하고, BeginPlay나 콜리전 오버랩 검사를 '보류' 상태로 둡니다.
            AActor* SpawnedActor = GetWorld()->SpawnActorDeferred<AActor>(
                PickupClassToDrop, 
                FTransform(FRotator::ZeroRotator, SpawnLocation), 
                GetOwner(), 
                Cast<APawn>(GetOwner()), 
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn
            );

            if (SpawnedActor)
            {
                // 데이터 페이로드 주입 (인터페이스 격발)
                if (SpawnedActor->Implements<UItemStateInterface>())
                {
                    IItemStateInterface::Execute_SetDroppedAmmo(SpawnedActor, OldInstance->CurrentAmmoInMag);
                }

                // 1차 방어선: 플래그 즉시 차단
                ASRItemPickupBase* PickupBase = Cast<ASRItemPickupBase>(SpawnedActor);
                if (PickupBase)
                {
                    PickupBase->StartPickupCooldown(1.5f);
                }

                // 🚨 [2차 가드 핵심 추가] 
                // FinishSpawning(스폰 마감)이 되는 순간 엔진 내부적으로 트리거 오버랩을 동기식으로 즉시 계산합니다.
                // 마감 직전에 루트 컴포넌트의 콜리전 채널에서 'Pawn(플레이어)'을 무시하도록 선제 조치합니다.
                // 이렇게 하면 스폰 마감 중에 오버랩 이벤트 신호 자체가 발생하는 것을 원천 차단합니다.
                if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent()))
                {
                    RootPrim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
                }

                // 🟢 이제 안전하게 액터의 스폰을 마감합니다. (오버랩이 원천 봉쇄되어 절대 터지지 않습니다)
                SpawnedActor->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));

                // 스폰이 완벽히 끝난 후 정면으로 피직스 힘을 가해 던집니다.
                if (PickupBase)
                {
                    PickupBase->InitDroppedItem(ThrowForce);
                }
            }

            OldWeaponVisualActor->Destroy(); 
        }
    }

    // =======================================================================
    // [2] 무기 비주얼 액터 생성 파트 (기존 코드 동일 유지)
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

    RequestSwitchWeapon(SlotType, true); // 연속 획득 오버라이드 지원 유지
    return true;
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