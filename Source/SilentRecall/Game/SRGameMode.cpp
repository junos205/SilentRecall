#include "Game/SRGameMode.h"
#include "Game/SRGameInstance.h"
#include "Character/SREnemyCharacterBase.h"
#include "Character/SRPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"

void ASRGameMode::BeginPlay()
{
    Super::BeginPlay();

    USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance());
    if (!GI) return;

    // 1. 플레이어가 세이브 포인트에서 부활해야 하는 상태라면 위치를 강제 동기화합니다.
    if (GI->bPendingRespawn && GI->ActiveCheckpointIndex > 0)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC && PC->GetPawn())
        {
            PC->GetPawn()->SetActorLocation(GI->SavedLocation);
            PC->GetPawn()->SetActorRotation(GI->SavedRotation);
            PC->SetControlRotation(GI->SavedRotation);

            if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(PC->GetPawn()))
            {
                PlayerChar->LoadCharacterState(GI);
            }
        }
        GI->bPendingRespawn = false; 
    }

    // 2. 이미 완벽하게 죽였던 적들은 레벨이 로드되자마자 월드에서 흔적도 없이 삭제합니다.
    if (GI->ActiveCheckpointIndex > 0)
    {
        TArray<AActor*> FoundEnemies;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASREnemyCharacterBase::StaticClass(), FoundEnemies);

        for (AActor* Actor : FoundEnemies)
        {
            if (Actor && GI->DefeatedEnemyNames.Contains(Actor->GetFName()))
            {
                Actor->Destroy();
            }
        }
    }
}

void ASRGameMode::OnPlayerCharacterDeath(ACharacter* DeadPlayer)
{
    // 3.5초 뒤 자동으로 레벨 리셋 부활이 발동하도록 타이머 세팅
    GetWorldTimerManager().SetTimer(AutoRespawnTimerHandle, this, &ASRGameMode::ExecuteRespawnReset, 3.5f, false);
}

void ASRGameMode::ExecuteRespawnReset()
{
    USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance());
    if (!GI) return;

    GetWorldTimerManager().ClearTimer(AutoRespawnTimerHandle);

    GI->bPendingRespawn = true;

    // 현재 플레이 중인 맵의 이름을 가져와 오픈 레벨을 때려 월드를 완전 청소합니다.
    FString CurrentMapName = GetWorld()->GetMapName();
    CurrentMapName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);
    
    UGameplayStatics::OpenLevel(GetWorld(), FName(*CurrentMapName));
}