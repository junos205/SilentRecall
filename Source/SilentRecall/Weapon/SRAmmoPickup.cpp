#include "SRAmmoPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Character/SRInventoryComponent.h"

ASRAmmoPickup::ASRAmmoPickup()
{
	AmmoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AmmoMesh"));
    
	// 🌟 [중요] 루트 컴포넌트가 아니라, 부모가 둥둥 띄우고 돌려주는 'VisualRoot' 하위 자식으로 입적시킵니다!
	AmmoMesh->SetupAttachment(VisualRoot);
    
	// 트리거 스피어가 충돌을 전담하므로 메쉬 자체의 물리 및 콜리전 계산은 꺼서 최적화합니다.
	AmmoMesh->SetSimulatePhysics(false);
	AmmoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASRAmmoPickup::OnPickedUp(USRInventoryComponent* InventoryComp)
{
	// 묻지도 따지지도 않고 오버랩 시 즉시 탄약 추가 후 소멸
	InventoryComp->AddReserveAmmo(AmmoTypeTag, AmmoAmount);
	UE_LOG(LogTemp, Warning, TEXT("[AmmoPickup] Overlap Picked up %d ammo of type %s."), AmmoAmount, *AmmoTypeTag.ToString());
    
	Destroy();
}