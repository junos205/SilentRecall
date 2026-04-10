// Fill out your copyright notice in the Description page of Project Settings.


#include "SRInventoryComponent.h"
#include "Data/SRWeaponDataAsset.h"
#include "Weapon/SRWeaponInstance.h"
#include "GameplayAbilitySpecHandle.h"
#include "Interface/ItemStateInterface.h"
#include "Weapon/SRWeaponPickup.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"


USRInventoryComponent::USRInventoryComponent()
{
    
}

bool USRInventoryComponent::AddWeapon(EWeaponSlot SlotType, USRWeaponInstance* NewInstance, AActor* PickedUpWeaponActor)
{
    if (SlotType == EWeaponSlot::None) return false;

    // 1. 이미 해당 슬롯에 무기가 있다면 바닥으로 던지기
    if (WeaponLoadout.Contains(SlotType) && SpawnedWeapons.Contains(SlotType))
    {
       AActor* OldWeapon = SpawnedWeapons[SlotType];
       USRWeaponInstance* OldInstance = WeaponLoadout[SlotType]; 

       if (OldWeapon && OldInstance)
       {
          if (OldWeapon->Implements<UItemStateInterface>())
          {
              IItemStateInterface::Execute_SetDroppedAmmo(OldWeapon, OldInstance->CurrentAmmoInMag);
          }

          OldWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); 
            
          if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(OldWeapon->GetRootComponent()))
          {
             RootComp->SetSimulatePhysics(true);
             RootComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
             
             TArray<UMeshComponent*> Meshes;
             OldWeapon->GetComponents<UMeshComponent>(Meshes);
             for (UMeshComponent* Mesh : Meshes)
             {
                 Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
             }

             if (AActor* OwnerActor = GetOwner())
             {
                 FVector ThrowDirection = OwnerActor->GetActorForwardVector() + FVector(0.0f, 0.0f, 0.5f);
                 ThrowDirection.Normalize(); 
                 float ThrowForce = 100.0f; 
                 RootComp->AddImpulse(ThrowDirection * ThrowForce, NAME_None, true); 
             }
          }

          if (SlotType == CurrentActiveSlot)
          {
              UnEquipWeapon(); 
              CurrentActiveSlot = EWeaponSlot::None; 
          }
       }
    }

    WeaponLoadout.Add(SlotType, NewInstance);

    // 3. 새 무기 액터 처리 및 등에 숨기기
    if (PickedUpWeaponActor)
    {
       if (UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(PickedUpWeaponActor->GetRootComponent()))
       {
          RootComp->SetSimulatePhysics(false);
          RootComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
       }

       TArray<UMeshComponent*> PickedMeshes;
       PickedUpWeaponActor->GetComponents<UMeshComponent>(PickedMeshes);
       for (UMeshComponent* Mesh : PickedMeshes)
       {
           Mesh->SetSimulatePhysics(false);
           Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
       }

       // ⭐️ [수정] 픽업 액터 캐스팅 삭제! 데이터 애셋에서 다이렉트로 홀스터 소켓 가져오기
       FName HolsterSocket = FName("HolsterSocket");
       if (NewInstance && NewInstance->WeaponData)
       {
           HolsterSocket = NewInstance->WeaponData->HolsterSocketName;
       }
       
       if (USkeletalMeshComponent* OwnerMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
       {
           PickedUpWeaponActor->AttachToComponent(OwnerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocket);
       }
        
       SpawnedWeapons.Add(SlotType, PickedUpWeaponActor);
    }

    // 4. 즉시 장착
    EquipWeapon(SlotType);

    return true;
}

void USRInventoryComponent::EquipWeapon(EWeaponSlot SlotToEquip)
{
    if (CurrentActiveSlot == SlotToEquip || !SpawnedWeapons.Contains(SlotToEquip)) return;

    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    USkeletalMeshComponent* PlayerMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();

    // 1. 기존 무기 숨기기 & 스킬 뺏기
    if (CurrentActiveSlot != EWeaponSlot::None)
    {
       AActor* CurrentWeapon = SpawnedWeapons[CurrentActiveSlot];
       USRWeaponInstance* CurrentInstance = WeaponLoadout[CurrentActiveSlot]; // ⭐️ 현재 무기 인스턴스 가져오기
       
       // ⭐️ [수정] 픽업 캐스팅 삭제! 데이터 애셋에서 홀스터 소켓 가져오기
       FName HolsterSocket = FName("HolsterSocket"); 
       if (CurrentInstance && CurrentInstance->WeaponData)
       {
           HolsterSocket = CurrentInstance->WeaponData->HolsterSocketName;
       }

       CurrentWeapon->AttachToComponent(PlayerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocket);
        
       if (ASC)
       {
          for (FGameplayAbilitySpecHandle Handle : CurrentGrantedAbilityHandles)
          {
             ASC->ClearAbility(Handle); 
          }
          CurrentGrantedAbilityHandles.Empty(); 
       }
    }
    
    // 새 무기 꺼내기
    USRWeaponInstance* WeaponInstance = WeaponLoadout[SlotToEquip];
    AActor* WeaponToEquip = SpawnedWeapons[SlotToEquip]; 

    if (!WeaponInstance || !WeaponInstance->WeaponData || !WeaponToEquip || !ASC) return;

    // ⭐️ [수정] 픽업 캐스팅 삭제! 데이터 애셋에서 장착 소켓 다이렉트로 가져오기
    FName EquipSocket = WeaponInstance->WeaponData->EquipSocketName;

    WeaponToEquip->AttachToComponent(PlayerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, EquipSocket);
    OnWeaponChanged.Broadcast(WeaponInstance->WeaponData); // 캐릭터야 옷 갈아입어라!

    // 2. GAS 스킬 주입 
    for (const TTuple<EInputAction, TSubclassOf<UGameplayAbility>>& AbilityPair : WeaponInstance->WeaponData->GrantedAbilities)
    {
       EInputAction InputID = AbilityPair.Key;
       TSubclassOf<UGameplayAbility> AbilityClass = AbilityPair.Value;

       if (AbilityClass)
       {
          FGameplayAbilitySpec Spec(AbilityClass, 1, static_cast<int32>(InputID), WeaponInstance);
          FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
          CurrentGrantedAbilityHandles.Add(Handle);
       }
    }

    CurrentActiveSlot = SlotToEquip;
}

void USRInventoryComponent::UnEquipWeapon()
{
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    
    if (CurrentActiveSlot == EWeaponSlot::None || !ASC) return;

    // 1. GAS 스킬 영수증 취소
    for (const FGameplayAbilitySpecHandle& Handle : CurrentGrantedAbilityHandles)
    {
       ASC->ClearAbility(Handle);
    }
    CurrentGrantedAbilityHandles.Empty(); 

    // 2. 등(Holster)으로 보내기
    if (SpawnedWeapons.Contains(CurrentActiveSlot))
    {
       AActor* WeaponToHide = SpawnedWeapons[CurrentActiveSlot];
       USRWeaponInstance* HideInstance = WeaponLoadout[CurrentActiveSlot]; // ⭐️ 현재 무기 인스턴스 가져오기

       if (WeaponToHide && HideInstance && HideInstance->WeaponData)
       {
          // ⭐️ [수정] 데이터 애셋에서 다이렉트로 홀스터 소켓 가져오기
          FName HolsterSocket = HideInstance->WeaponData->HolsterSocketName;
            
          if (USkeletalMeshComponent* OwnerMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
          {
             WeaponToHide->AttachToComponent(OwnerMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HolsterSocket);
          }
       }
       
       OnWeaponChanged.Broadcast(nullptr); // 캐릭터야 맨손으로 돌아가라!
    }

    CurrentActiveSlot = EWeaponSlot::None;
}