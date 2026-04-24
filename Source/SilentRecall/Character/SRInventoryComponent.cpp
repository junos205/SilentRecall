#include "SRInventoryComponent.h"
#include "Interface/SRCharacterInterface.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"

USRInventoryComponent::USRInventoryComponent() {}

bool USRInventoryComponent::AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, AActor* PickedUpWeaponActor)
{
    if (!PickedUpWeaponActor || !NewInstance) return false;

    // 1. 기존 무기 바닥에 버리기 (완벽 개선본)
    if (WeaponLoadout.Contains(SlotType) && SpawnedWeapons.Contains(SlotType))
    {
        AActor* OldWeapon = SpawnedWeapons[SlotType];
        if (OldWeapon)
        {
            // ⭐️ 1-1. 버릴 무기의 클래스 정보(픽업 블루프린트)를 기억해둡니다.
            UClass* PickupClassToDrop = OldWeapon->GetClass();
            
            // ⭐️ 1-2. 눈앞에 떨어뜨릴 위치 계산 (앞으로 1미터, 위로 살짝)
            FVector DropLoc = GetOwner()->GetActorLocation() + (GetOwner()->GetActorForwardVector() * 100.0f) + FVector(0, 0, 50.0f);
            
            // ⭐️ 1-3. 내 손에 있던 구형 무기는 깔끔하게 흔적도 없이 파괴!
            OldWeapon->Destroy(); 

            // ⭐️ 1-4. 바닥에 완전히 깨끗한 새 픽업 액터를 스폰!
            AActor* NewDrop = GetWorld()->SpawnActor<AActor>(PickupClassToDrop, DropLoc, FRotator::ZeroRotator);
            
            if (NewDrop)
            {
                // 새 픽업 액터에 물리와 힘을 가해서 자연스럽게 떨어지게 만듭니다.
                if (NewDrop)
                {
                    if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(NewDrop->GetRootComponent()))
                    {
                        Root->SetSimulatePhysics(true);
                    
                        // ⭐️ [삭제] 엔진 기본 프로필로 덮어쓰는 이 줄을 아예 지워버리세요!
                        // Root->SetCollisionProfileName(TEXT("PhysicsActor")); 

                        Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                    
                        // ⭐️ [추가] C++에서 확실하게 "폰(플레이어)은 무시해라" 라고 명령을 박아버립니다.
                        Root->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
                        // (만약 카메라와 부딪혀서 화면이 흔들리는 것도 막고 싶다면 아래 줄도 추가!)
                        Root->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); 
                    
                        // 앞으로 던지는 힘 추가
                        Root->AddImpulse(GetOwner()->GetActorForwardVector() * 100.0f, NAME_None, true);
                    }
                }
            }
        }
    }

    // 2. 새 무기 줍기 (물리 끄기)
    // (이 아래부터는 이전 코드와 동일합니다!)
    if (UPrimitiveComponent* NewRoot = Cast<UPrimitiveComponent>(PickedUpWeaponActor->GetRootComponent()))
    {
        NewRoot->SetSimulatePhysics(false); 
        NewRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    // 데이터 등록
    WeaponLoadout.Add(SlotType, NewInstance);
    SpawnedWeapons.Add(SlotType, PickedUpWeaponActor);

    // 일단 캐릭터 등에 부착
    if (ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner()))
    {
        Char->AttachWeaponToHolster(PickedUpWeaponActor, NewInstance->WeaponData->HolsterSocketName);
    }

    // ⭐️ [이 부분이 핵심 해결책입니다]
    // 1. 만약 지금 들고 있는 슬롯과 똑같은 타입의 무기를 주웠다면 (예: 총 들고 있는데 새 총 주움)
    // 2. 현재 슬롯을 잠시 None으로 비워서 RequestSwitchWeapon이 "교체 필요함"을 인지하게 만듭니다.
    if (CurrentActiveSlot == SlotType)
    {
        CurrentActiveSlot = EWeaponSlot::None;
    }

    // 3. 묻지도 따지지도 않고 새로 먹은 무기를 꺼내라고 명령합니다!
    RequestSwitchWeapon(SlotType);

    return true;
}

void USRInventoryComponent::RequestSwitchWeapon(EWeaponSlot NewSlot)
{
    if (bIsSwitchingWeapon || CurrentActiveSlot == NewSlot) return;
    
    bIsSwitchingWeapon = true;
    NextSlotToEquip = NewSlot;
    BeginUnEquip();
}

void USRInventoryComponent::BeginUnEquip()
{
    // 스킬 즉시 회수
    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
    {
        for (auto& Handle : CurrentGrantedAbilityHandles) ASC->ClearAbility(Handle);
        CurrentGrantedAbilityHandles.Empty();
    }

    // 2. 집어넣는 애니메이션 요청
    if (CurrentActiveSlot != EWeaponSlot::None)
    {
        if (ISRCharacterInterface* Char = Cast<ISRCharacterInterface>(GetOwner()))
        {
            // ⭐️ [버그 해결] 데이터 애셋에 몽타주를 진짜로 넣었는지 확인!
            UAnimMontage* MontageToPlay = WeaponLoadout[CurrentActiveSlot]->WeaponData->UnEquipMontage;
            
            if (MontageToPlay)
            {
                Char->PlayWeaponMontage(MontageToPlay);
                return; // 데이터가 있을 때만 애니메이션을 틀고 대기! (블루프린트에 노티파이 필수)
            }
        }
    }
    
    // ⭐️ 데이터가 비어있으면 멈추지 말고 즉시 다음 무기로 교체!
    FinishUnEquip();
}
void USRInventoryComponent::FinishUnEquip()
{
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
        OnWeaponChanged.Broadcast(NewInstance->WeaponData); // ⭐️ 이게 실행되어야 ABP가 정상 교체됩니다!

        // ⭐️ [버그 2 해결] 여기서도 꺼내는 몽타주가 있을 때만 return(대기) 합니다.
        UAnimMontage* EquipMontage = NewInstance->WeaponData->EquipMontage;
        if (EquipMontage)
        {
            Char->PlayWeaponMontage(EquipMontage);
            return; 
        }
    }
    FinishEquip();
}

void USRInventoryComponent::FinishEquip()
{
    // ⭐️ [핵심 방어막] 꺼내는 애니메이션(Equip)도 두 번 불리는 것을 방지!
    // 스위칭 상태가 이미 끝났다면 두 번째 노티파이는 무시합니다.
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
        }
    }
    
    // 교체 완전 종료
    bIsSwitchingWeapon = false;
}

void USRInventoryComponent::CycleWeapon(bool bNext)
{
    if (WeaponLoadout.Num() <= 1 || bIsSwitchingWeapon) return;
    // 순환 로직 생략 (기존 코드와 동일)
}