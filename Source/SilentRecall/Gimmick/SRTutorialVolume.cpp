// Fill out your copyright notice in the Description page of Project Settings.

#include "Gimmick/SRTutorialVolume.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Game/SRGameInstance.h"
#include "Character/SRPlayerCharacter.h"

ASRTutorialVolume::ASRTutorialVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    
    // 플레이어 감지용 트리거 콜리전 세팅
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ASRTutorialVolume::BeginPlay()
{
    Super::BeginPlay();
    
    // 델리게이트 무전 연결
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASRTutorialVolume::OnVolumeOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ASRTutorialVolume::OnVolumeOverlapEnd);
}

void ASRTutorialVolume::OnVolumeOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 1. 들어온 대상이 플레이어인지 검문
    if (!OtherActor || !OtherActor->IsA(ASRPlayerCharacter::StaticClass())) return;

    // 2. 영구 생존 장부(GameInstance) 호출
    USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance());
    if (!GI) return;

    // 🔥 [핵심 방어선] 이미 장부에 이 TutorialID가 등록되어 있다면 무조건 연산 무시 패스!
    if (GI->ViewedTutorialIDs.Contains(TutorialID))
    {
        UE_LOG(LogTemp, Log, TEXT("[TutorialVolume] 이미 시청 완료한 튜토리얼 존입니다 (%s). 가동을 거부합니다."), *TutorialID.ToString());
        return;
    }

    bIsCurrentlyActive = true;

    // 3. 월드 슬로우 모션 주입
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), TutorialTimeDilation);

    // 4. UI 텍스트 출력을 위해 블루프린트단에 무전 발송 (텍스트 배달)
    ReceiveOnTutorialActivated(TutorialText);
    UE_LOG(LogTemp, Warning, TEXT("[TutorialVolume] 튜토리얼 가동 -> ID: %s, 슬로우모션 시작"), *TutorialID.ToString());
}

void ASRTutorialVolume::OnVolumeOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || !OtherActor->IsA(ASRPlayerCharacter::StaticClass()) || !bIsCurrentlyActive) return;

    // 1. 월드 시간 속도 원상복구 (1.0배속)
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);

    // 2. UI 제거를 위한 무전 발송
    ReceiveOnTutorialDeactivated();

    // 3. ⭐ [결정타] 탈출 성공 시 장부에 이 ID를 완전히 박제하여 다신 안 켜지게 차단락(Lock)을 겁니다.
    if (USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance()))
    {
        GI->ViewedTutorialIDs.Add(TutorialID);
        UE_LOG(LogTemp, Warning, TEXT("[TutorialVolume] 시청 완료 -> 영구 영수증 장부에 기록 완료: %s"), *TutorialID.ToString());
    }

    bIsCurrentlyActive = false;
}