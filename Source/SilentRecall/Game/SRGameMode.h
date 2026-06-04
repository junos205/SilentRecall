#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SRGameMode.generated.h"

UCLASS()
class SILENTRECALL_API ASRGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	/** 플레이어 사망 시 사망 GA가 호출해 줄 함수 */
	void OnPlayerCharacterDeath(ACharacter* DeadPlayer);

	/** 실제로 레벨을 새로고침하여 부활을 집행하는 함수 */
	void ExecuteRespawnReset();

private:
	FTimerHandle AutoRespawnTimerHandle;
};