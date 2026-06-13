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
	// 💥 [핵심] 오버랩 시 즉시 '탄창 n통'을 추가 (기본 1통)
	InventoryComp->AddReserveAmmo(AmmoTypeTag, MagazineAmount);
    
	// 로그도 탄창 단위로 변경!
	UE_LOG(LogTemp, Warning, TEXT("[AmmoPickup] 획득 완료! 탄약 타입: %s | 추가된 탄창 수: %d 통"), *AmmoTypeTag.ToString(), MagazineAmount);
    
	Destroy();
}