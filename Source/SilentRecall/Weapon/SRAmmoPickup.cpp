// Fill out your copyright notice in the Description page of Project Settings.


#include "SRAmmoPickup.h"
#include "Components/StaticMeshComponent.h"
// ⭐️ 인벤토리 컴포넌트 헤더 포함 (경로 주의)
#include "Character/SRInventoryComponent.h" 

ASRAmmoPickup::ASRAmmoPickup()
{
	// 메쉬 컴포넌트 생성 및 루트로 설정
	AmmoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AmmoMesh"));
	RootComponent = AmmoMesh;

	// 바닥에 자연스럽게 떨어지도록 물리 활성화
	AmmoMesh->SetSimulatePhysics(true);
	AmmoMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    
	// 플레이어가 밟고 넘어지거나 밀리지 않도록 폰(Pawn)과의 충돌은 무시
	AmmoMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void ASRAmmoPickup::Interact_Implementation(AActor* Interactor)
{
	if (!Interactor) return;

	// 1. 상호작용한 액터(주로 플레이어)에서 인벤토리 컴포넌트를 찾습니다.
	USRInventoryComponent* InventoryComp = Interactor->FindComponentByClass<USRInventoryComponent>();

	if (InventoryComp)
	{
		// 2. 인벤토리에 지정된 태그의 예비 탄약을 추가합니다.
		InventoryComp->AddReserveAmmo(AmmoTypeTag, AmmoAmount);

		UE_LOG(LogTemp, Warning, TEXT("[AmmoPickup] Picked up %d ammo of type %s."), AmmoAmount, *AmmoTypeTag.ToString());

		// 3. 탄약을 유저에게 넘겨줬으니, 이 상자는 맵에서 파괴합니다!
		Destroy();
	}
}