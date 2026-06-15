#include "Gimmick/SRTutorialVolume.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h" 
#include "Kismet/GameplayStatics.h"
#include "Game/SRGameInstance.h"
#include "Character/SRPlayerCharacter.h"

ASRTutorialVolume::ASRTutorialVolume()
{
    PrimaryActorTick.bCanEverTick = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    TutorialPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("TutorialPostProcess"));
    TutorialPostProcess->SetupAttachment(RootComponent);
    
    TutorialPostProcess->bUnbound = true;
    TutorialPostProcess->BlendWeight = 0.0f; 
    TutorialPostProcess->Priority = 10.0f;   

    // 후처리 1: 채도 제어 (흑백화)
    TutorialPostProcess->Settings.bOverride_ColorSaturation = true;
    TutorialPostProcess->Settings.ColorSaturation = FVector4(0.1f, 0.1f, 0.1f, 1.0f);

    // 후처리 2: 크로매틱 애버레이션 수치 유지 (인게임 퀄리티가 Epic 이상일 때 체감됩니다)
    TutorialPostProcess->Settings.bOverride_SceneFringeIntensity = true;
    TutorialPostProcess->Settings.SceneFringeIntensity = 4.0f; 
}

void ASRTutorialVolume::BeginPlay()
{
    Super::BeginPlay();
    
    // [방어선 1] 세이브 로드 시 이미 깬 구역이면 원천 봉쇄
    if (USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance()))
    {
        if (GI->ViewedTutorialIDs.Contains(TutorialID))
        {
            if (TutorialPostProcess)
            {
                TutorialPostProcess->Deactivate(); 
            }
            SetActorTickEnabled(false); 
            return; 
        }
    }

    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASRTutorialVolume::OnVolumeOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ASRTutorialVolume::OnVolumeOverlapEnd);
}

void ASRTutorialVolume::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 실시간 페이드 인/아웃 가중치 보간 연산
    if (TutorialPostProcess && !FMath::IsNearlyEqual(CurrentBlendWeight, TargetBlendWeight, 0.001f))
    {
        CurrentBlendWeight = FMath::FInterpTo(CurrentBlendWeight, TargetBlendWeight, DeltaTime, FadeSpeed);
        TutorialPostProcess->BlendWeight = CurrentBlendWeight;
    }

    // =======================================================================
    // 🛡️ [완치] 첫 프레임 먹통 버그 완벽 박멸
    // 게임 시작 시점이 아니라, 플레이어가 볼륨을 '나갔기 때문에 종료 예약 스위치'가 
    // 정상 작동하고 페이드 아웃이 완료된 시점에만 안전하게 봉인을 집행합니다.
    // =======================================================================
    if (bWantsToDeactivatePP && CurrentBlendWeight <= 0.01f)
    {
        if (TutorialPostProcess)
        {
            TutorialPostProcess->BlendWeight = 0.0f;
            TutorialPostProcess->bEnabled = false;
            TutorialPostProcess->Deactivate(); 
        }
        bWantsToDeactivatePP = false; // 스위치 리셋
        SetActorTickEnabled(false);   // 이 액터의 틱 연산을 영구 종료하여 최적화
        UE_LOG(LogTemp, Warning, TEXT("[TutorialVolume] 색감 복원 완료 -> 안전하게 실시간 영구 폐기 완료"));
    }
    // =======================================================================
}

void ASRTutorialVolume::OnVolumeOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || !OtherActor->IsA(ASRPlayerCharacter::StaticClass())) return;

    USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance());
    if (!GI) return;

    if (GI->ViewedTutorialIDs.Contains(TutorialID))
    {
        if (TutorialPostProcess) TutorialPostProcess->Deactivate();
        SetActorTickEnabled(false);
        return;
    }

    bIsCurrentlyActive = true;
    bWantsToDeactivatePP = false; // 혹시 모를 종료 예약 상태 초기화

    // 시간 지연 및 채도 다운 페이드 인 지시
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), TutorialTimeDilation);
    TargetBlendWeight = 1.0f; 

    ReceiveOnTutorialActivated(TutorialText);
    UE_LOG(LogTemp, Warning, TEXT("[TutorialVolume] 튜토리얼 가동 -> ID: %s, 슬로우모션 시작"), *TutorialID.ToString());
}

void ASRTutorialVolume::OnVolumeOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor || !OtherActor->IsA(ASRPlayerCharacter::StaticClass()) || !bIsCurrentlyActive) return;

    // 시간 복원 및 UI 텍스트 제거
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
    TargetBlendWeight = 0.0f; // 원래 화면 색감으로 복원 페이드 아웃 지시

    // =======================================================================
    // 🌟 탈출 시점에 틱에게 후처리 장치 종료를 정식으로 "예약" 인계합니다.
    // =======================================================================
    bWantsToDeactivatePP = true; 
    // =======================================================================

    ReceiveOnTutorialDeactivated();

    // 장부에 영구 기록
    if (USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance()))
    {
        GI->ViewedTutorialIDs.Add(TutorialID);
        UE_LOG(LogTemp, Warning, TEXT("[TutorialVolume] 시청 완료 -> 영구 장부에 기록 완료: %s"), *TutorialID.ToString());
    }

    bIsCurrentlyActive = false; 
}