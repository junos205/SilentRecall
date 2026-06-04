// Fill out your copyright notice in the Description page of Project Settings.

#include "Gimmick/SRCheckpointVolume.h"
#include "Game/SRGameInstance.h"
#include "Character/SRPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"

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
}

void ASRCheckpointVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 플레이어가 아닌 액터가 닿으면 즉시 무시
    if (!OtherActor || !OtherActor->IsA(ASRPlayerCharacter::StaticClass())) return;

    USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance());
    if (!GI) return;

    // 🔒 [가드 1] 순차성 검증 (1 -> 2 -> 3)
    if (GI->ActiveCheckpointIndex != CheckpointIndex - 1) return; 

    // 🔒 [가드 2] 🌟 오직 GAS 태그만을 이용한 생사 확인 로직
    for (ASREnemyCharacterBase* Enemy : AssignedEnemies)
    {
        if (Enemy)
        {
            // 적의 AbilitySystemComponent를 바로 추출합니다.
            UAbilitySystemComponent* EnemyASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
            if (EnemyASC)
            {
                // 사망 시 부여되는 GAS 상태 태그 확보
                FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"));
                
                // 🎯 [핵심] 만약 이 적이 아직 사망 태그를 가지고 있지 않다면? = 아직 살아있음!
                if (!EnemyASC->HasMatchingGameplayTag(DeadTag))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Checkpoint] 🔒 아직 구역에 살아있는 적(%s)이 있어 세이브가 불가능합니다."), *Enemy->GetName());
                    return; // 함수를 즉시 종료하여 세이브를 거부합니다.
                }
            }
        }
    }

    // 🎉 구역 내 모든 적이 무사히 'Character.State.IsDead' 태그를 가지고 있다면 세이브 통과!
    GI->ActiveCheckpointIndex = CheckpointIndex;
    GI->SavedLocation = SpawnTransformArrow->GetComponentLocation();
    GI->SavedRotation = SpawnTransformArrow->GetComponentRotation();

    // 🌟 [이 줄을 추가!] 플레이어의 체력, 무기, 총알 상태를 주머니에 담아 얼려버립니다.
    if (ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(OtherActor))
    {
        PlayerChar->SaveCharacterState(GI);
    }

    // 부활 시 다시 스폰되지 않도록 영구 처치 목록에 등록 (기존 코드 동일)
    for (ASREnemyCharacterBase* Enemy : AssignedEnemies)
    {
        if (Enemy) GI->DefeatedEnemyNames.Add(Enemy->GetFName());
    }

    UE_LOG(LogTemp, Error, TEXT("[Checkpoint 🟢] 배정된 적 전멸 확인! 세이브 포인트 %d 번 등록 완료."), CheckpointIndex);
}