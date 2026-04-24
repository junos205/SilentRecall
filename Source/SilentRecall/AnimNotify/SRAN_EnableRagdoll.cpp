#include "SRAN_EnableRagdoll.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void USRAN_EnableRagdoll::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	// 1. 메쉬의 콜리전 활성화 (물리 연산을 위해 필수)
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// 2. ⭐️ 유저님의 핵심 요구사항: 살아있는 캐릭터(Pawn)가 시체에 걸려 넘어지지 않게 무시!
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    
	// (보너스 디테일) 카메라가 시체 안으로 파고들 때 화면이 갑자기 줌인되는 현상 방지
	MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); 

	// 3. 대망의 래그돌 켜기! (이 순간 애니메이션이 멈추고 물리 엔진이 관절을 지배합니다)
	MeshComp->SetSimulatePhysics(true);
}