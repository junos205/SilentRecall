#include "SREnemyCharacterBase.h"
#include "Character/SRInventoryComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"

ASREnemyCharacterBase::ASREnemyCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ASREnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// ⭐️ 데이터 애셋 하나만으로 인스턴스 생성 + 초기화를 픽업과 똑같이 처리합니다!
	if (DefaultWeaponData && InventoryComponent && DefaultWeaponActorClass)
	{
		// 1. 인스턴스 생성 및 데이터 애셋으로 초기화 (픽업 방식과 동일)
		USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(InventoryComponent);
        
		// AI니까 총알은 데이터 애셋의 최대치(MaxAmmo)로 꽉 채워서 줍니다.
		NewInstance->InitializeInstance(DefaultWeaponData, 999); 

		// 2. 캐릭터 손에 들려줄 실제 무기 액터 스폰 (픽업 액터와 동일한 역할)
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
        
		AActor* SpawnedWeaponActor = GetWorld()->SpawnActor<AActor>(
			DefaultWeaponActorClass, // 데이터 애셋 안에 클래스 정보가 있다면 DefaultWeaponData->WeaponClass 로 변경하세요!
			GetActorTransform(), 
			SpawnParams
		);

		// 3. 인벤토리 컴포넌트에 슬롯, 인스턴스, 액터를 한방에 등록!
		if (SpawnedWeaponActor)
		{
			EWeaponSlot SlotToUse = DefaultWeaponData->WeaponSlotType;
			InventoryComponent->AddWeapon(SlotToUse, NewInstance, SpawnedWeaponActor);
            
			// AI가 스폰되자마자 이 무기를 들게 하려면 교체 요청을 보냅니다.
			InventoryComponent->RequestSwitchWeapon(SlotToUse);
		}
	}
}