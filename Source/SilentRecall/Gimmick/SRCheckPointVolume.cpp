#include "Gimmick/SRCheckpointVolume.h"
#include "Game/SRGameInstance.h"
#include "Character/SRPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Gimmick/SRObjectiveMarker.h"

ASRCheckpointVolume::ASRCheckpointVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

    SpawnTransformArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnTransformArrow"));
    SpawnTransformArrow->SetupAttachment(RootComponent);
}

void ASRCheckpointVolume::BeginPlay()
{
    Super::BeginPlay();
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASRCheckpointVolume::OnOverlapBegin);

    // [GAS 태그 실시간 바인딩] 적들의 ASC가 완전히 준비되면 사망 감지 센서를 부착합니다.
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        for (ASREnemyCharacterBase* Enemy : AssignedEnemies)
        {
            if (!IsValid(Enemy)) continue;

            UAbilitySystemComponent* EnemyASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
            if (EnemyASC)
            {
                FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"));
                
                EnemyASC->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved)
                    .AddUObject(this, &ASRCheckpointVolume::OnEnemyDeathTagChanged);
            }
        }

        // 🌟 [버그 수정] 시작점인 1번 체크포인트가 적이 없다면 최초 목적지 마커를 즉시 가동합니다.
        // 2번 이후의 구역들은 맵 로딩 때 실행되지 않고, 플레이어가 실제로 진입했을 때만 정산되도록 타이밍을 격리합니다.
        if (CheckpointIndex == 1 && AssignedEnemies.Num() == 0)
        {
            EvaluateEnemiesSanity();
        }
    });
}

void ASRCheckpointVolume::OnEnemyDeathTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    // 적이 쓰러져 사망 태그가 실시간으로 부여되면 즉시 정산을 격발합니다.
    EvaluateEnemiesSanity();
}

void ASRCheckpointVolume::EvaluateEnemiesSanity()
{
    if (bAllEnemiesDefeatedFired) return;

    // 6번 구역처럼 할당된 적이 존재하는 경우, 생존자가 단 한 마리라도 있는지 철저히 검사합니다.
    if (AssignedEnemies.Num() > 0)
    {
        FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"));

        for (int32 i = 0; i < AssignedEnemies.Num(); ++i)
        {
            ASREnemyCharacterBase* Enemy = AssignedEnemies[i];
            if (!IsValid(Enemy)) continue; // 완전히 소멸한 시체는 죽은 것으로 인정하고 패스

            UAbilitySystemComponent* EnemyASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
            if (EnemyASC)
            {
                if (!EnemyASC->HasMatchingGameplayTag(DeadTag))
                {
                    // 🔒 2명 중 단 1명이라도 아직 살아있다면 다음 마커를 켜지 않고 가드를 유지합니다.
                    return; 
                }
            }
            else return;
        }
    }

    // 🎉 [목적 달성 타이밍: 적 전멸 혹은 프리패스 진입 완료]
    bAllEnemiesDefeatedFired = true;

    // GameInstance 데이터 창고에 현재 구역 클리어 완료 도장을 찍습니다.
    if (USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance()))
    {
        if (GI->CompletedCombatIndex < CheckpointIndex)
        {
            GI->CompletedCombatIndex = CheckpointIndex;
        }

        for (ASREnemyCharacterBase* Enemy : AssignedEnemies)
         {
            if (Enemy) GI->DefeatedEnemyNames.Add(Enemy->GetFName());
        }
    }

    if (OnAllEnemiesDefeated.IsBound())
    {
        OnAllEnemiesDefeated.Broadcast();
    }

    // 🎯 [유저님 핵심 요구사항 반영] 2명을 죽이자마자 "즉시" 다음 세이브 가능 지점을 알려주는 마커를 띄웁니다!
    if (NextObjectiveMarker)
    {
        if (ASRObjectiveMarker* Marker = Cast<ASRObjectiveMarker>(NextObjectiveMarker))
        {
            Marker->ActivateMarker();
        }
        UE_LOG(LogTemp, Warning, TEXT("[Checkpoint 🎯] %d번 구역 미션 완료! 다음 세이브 라인 목적지 마커 즉시 발동!"), CheckpointIndex);
    }
}

void ASRCheckpointVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || !OtherActor->IsA(ASRPlayerCharacter::StaticClass())) return;

    USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance());
    if (!GI) return;

    // 1번 체크포인트가 아닐 때만 순서 및 직전 전투 올킬 검문소를 가동합니다.
    if (CheckpointIndex != 1)
    {
        // 1단계 순서 검증: 위로 딱 1개까지만 세이브 체크가 가능하도록 필터링
        if (GI->ActiveCheckpointIndex != CheckpointIndex - 1) return; 

        // 2단계 미션 검증: 직전 구역의 미션(올킬)을 완수했는지 검사
        if (GI->CompletedCombatIndex < CheckpointIndex - 1)
        {
            UE_LOG(LogTemp, Error, TEXT("[Checkpoint 🔒] 세이브 거부! 직전 구역(%d번)의 미션을 아직 완수하지 못했습니다!"), CheckpointIndex - 1);
            return; 
        }
    }

    // 🎉 모든 관문을 통과했으므로 안전하게 현재 위치 세이브 등록
    GI->ActiveCheckpointIndex = CheckpointIndex;
    GI->SavedLocation = SpawnTransformArrow->GetComponentLocation();
    GI->SavedRotation = SpawnTransformArrow->GetComponentRotation();

    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(OtherActor))
    {
        PlayerChar->SaveCharacterState(GI);
    }

    UE_LOG(LogTemp, Warning, TEXT("[Checkpoint 🟢] %d번 세이브 포인트 등록 성공!"), CheckpointIndex);

    // =======================================================================
    // 🟢 [유저님 핵심 요구사항 반영: 적 없음 프리패스 시스템]
    // 이 구역에 감시할 적을 애초에 배치하지 않았다면(0명), 밟아서 세이브하자마자 
    // 즉시 이 구역 전투도 클리어 처리하고 다음 마커를 논스톱으로 바로 발동시킵니다!
    // =======================================================================
    if (AssignedEnemies.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Checkpoint 🟢] %d번 구역은 빈 안전지대입니다. 즉시 다음 퀘스트 라인을 엽니다.") , CheckpointIndex);
        EvaluateEnemiesSanity(); // 즉시 CompletedCombatIndex를 올리고 다음 목적지 마커를 활성화함
    }
}