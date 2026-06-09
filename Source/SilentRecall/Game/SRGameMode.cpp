#include "Game/SRGameMode.h"
#include "Game/SRGameInstance.h"
#include "Character/SREnemyCharacterBase.h"
#include "Character/SRPlayerCharacter.h"
#include "Gimmick/SRCheckpointVolume.h"
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

    // =======================================================================
    // 🔄 [신규 추가] 사망 시 현재 체크포인트 이후의 휘발성 진행 상황 완벽 롤백
    // =======================================================================
    if (GI->ActiveCheckpointIndex > 0)
    {
        // 1. 컴뱃 클리어 장부를 플레이어가 부활할 체크포인트의 직전 상태로 안전하게 되돌림
        // (예: 2번 체크포인트에서 부활한다면, 2번 구역 전투는 아직 안 깬 상태인 1로 롤백)
        GI->CompletedCombatIndex = GI->ActiveCheckpointIndex - 1;

        // 2. 월드에 배치된 모든 체크포인트 볼륨을 탐색
        TArray<AActor*> FoundCheckpoints;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASRCheckpointVolume::StaticClass(), FoundCheckpoints);

        for (AActor* VolActor : FoundCheckpoints)
        {
            ASRCheckpointVolume* CheckpointVol = Cast<ASRCheckpointVolume>(VolActor);
            if (CheckpointVol)
            {
                // 플레이어가 부활할 체크포인트 번호를 포함하여, 그보다 크거나 같은(이후의) 구역에 
                // 할당되어 있던 적들은 영구 사망 장부(DefeatedEnemyNames)에서 강제로 삭제(부활 대기)
                if (CheckpointVol->CheckpointIndex >= GI->ActiveCheckpointIndex)
                {
                    for (ASREnemyCharacterBase* Enemy : CheckpointVol->AssignedEnemies)
                    {
                        if (Enemy)
                        {
                            GI->DefeatedEnemyNames.Remove(Enemy->GetFName());
                        }
                    }
                }
            }
        }
        
        UE_LOG(LogTemp, Warning, TEXT("[Respawn] %d번 체크포인트로 리셋 시도: 해당 구역 적들 장부 롤백 완료!"), GI->ActiveCheckpointIndex);
    }
    // =======================================================================

    GI->bPendingRespawn = true;

    // 현재 플레이 중인 맵의 이름을 가져와 오픈 레벨을 때려 월드를 완전 청소합니다.
    FString CurrentMapName = GetWorld()->GetMapName();
    CurrentMapName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);
    
    UGameplayStatics::OpenLevel(GetWorld(), FName(*CurrentMapName));
}