// Fill out your copyright notice in the Description page of Project Settings.

#include "Gimmick/SRGrapplePoint.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Blueprint/UserWidget.h" 
#include "Kismet/GameplayStatics.h"

ASRGrapplePoint::ASRGrapplePoint()
{
    PrimaryActorTick.bCanEverTick = true;

    BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("SphereComponent"));
    RootComponent = BoxComponent;
    
    BoxComponent->SetBoxExtent(FVector(10.0f, 100.0f, 100.0f));
    BoxComponent->SetCollisionProfileName(TEXT("Custom"));
    BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoxComponent->SetCollisionObjectType(ECC_WorldStatic);
    BoxComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    GrappleWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("GrappleWidget"));
    GrappleWidget->SetupAttachment(RootComponent);
    
    GrappleWidget->SetWidgetSpace(EWidgetSpace::World);
    GrappleWidget->SetDrawSize(FVector2D(250.0f, 250.0f));
    
    GrappleWidget->SetUsingAbsoluteScale(true);
    GrappleWidget->SetRelativeScale3D(FVector(0.0f, 0.0f, 0.0f)); // 🌟 초기 스케일 0으로 안전 시작

    GrappleWidget->SetVisibility(false);
    Tags.Add(FName("GrappleTarget"));
}

void ASRGrapplePoint::BeginPlay()
{
    Super::BeginPlay();
    
    CurrentAlpha = 0.0f;
    CurrentScale = 0.0f;
    TargetAlpha = 0.0f;

    if (GrappleWidget)
    {
       GrappleWidget->SetVisibility(false);
       GrappleWidget->SetRelativeScale3D(FVector::ZeroVector);

       if (UUserWidget* UserWidget = GrappleWidget->GetUserWidgetObject())
       {
          UserWidget->SetRenderOpacity(0.0f);
       }
    }
}

void ASRGrapplePoint::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 오파시티(투명도)와 내부 스케일 값의 부드러운 보간 정산
    CurrentAlpha = FMath::FInterpTo(CurrentAlpha, TargetAlpha, DeltaTime, FadeSpeed);
    
    // 목표치 알파가 1이면 스케일 목표도 1, 꺼지는 중이면 스케일 목표도 0
    float TargetScaleVal = (TargetAlpha > 0.0f) ? 1.0f : 0.0f;
    CurrentScale = FMath::FInterpTo(CurrentScale, TargetScaleVal, DeltaTime, FadeSpeed);

    if (GrappleWidget)
    {
        if (UUserWidget* UserWidget = GrappleWidget->GetUserWidgetObject())
        {
            UserWidget->SetRenderOpacity(CurrentAlpha);
        }

        // 완전히 사라지면 가시성 Off 하여 CPU 드로우 콜 최적화
        if (TargetAlpha == 0.0f && CurrentAlpha <= 0.01f)
        {
            GrappleWidget->SetVisibility(false);
        }
        
        if (GrappleWidget->IsVisible())
        {
            APlayerController* PC = GetWorld()->GetFirstPlayerController();
            if (PC && PC->PlayerCameraManager)
            {
                FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
                FVector WidgetLocation = GrappleWidget->GetComponentLocation();
                
                // 1. 항상 정면을 보게 만드는 빌보드 회전 정산
                FRotator BillboardRotation = (CameraLocation - WidgetLocation).Rotation();
                GrappleWidget->SetWorldRotation(BillboardRotation);

                // =======================================================================
                // 📐 [신규 연출] 1. 원거리 화면 크기 유지 보정 메커니즘
                // 거리가 멀어질수록 원근법으로 작아지는 만큼 역산해서 크기를 고정 유지합니다.
                // =======================================================================
                float Distance = FVector::Distance(CameraLocation, WidgetLocation);

                // 멀어질수록 원근법을 이기고 화면상 크기를 유지하기 위한 공식 (기존 유지)
                float DistanceCompensation = Distance / 1500.0f;

                // =======================================================================
                // 📐 [정밀 보정] 거리 보정 스케일 하한선 조율
                // 기존 하한선(0.4f)은 가까워질수록 원래 리소스 크기의 40%까지 강제로 줄여버렸습니다.
                // 이를 최소 1.0f로 조여놓으면, 멀어질 때는 크기가 일정하게 보존되지만 
                // 코앞까지 접근할 때는 일반 3D 물체처럼 자연스럽게 화면에 꽉 차게 커져 시인성이 폭발합니다.
                // =======================================================================
                DistanceCompensation = FMath::Clamp(DistanceCompensation, 1.0f, 3.5f);
                // =======================================================================

                // 팝인 스케일과 펄스 웨이브 결합부 (기존 유지)
                float FinalCalculatedScale = CurrentScale * DistanceCompensation;
                if (TargetAlpha > 0.5f)
                {
                    float PulseWave = FMath::Sin(GetWorld()->GetTimeSeconds() * 5.5f) * 0.07f;
                    FinalCalculatedScale *= (1.0f + PulseWave);
                }

                GrappleWidget->SetRelativeScale3D(FVector(FinalCalculatedScale));
            }
        }
    }
}

void ASRGrapplePoint::SetWidgetActive(bool bActivate)
{
    if (bActivate)
    {
        TargetAlpha = 1.0f;
        if (GrappleWidget)
        {
            GrappleWidget->SetVisibility(true);
        }
    }
    else
    {
        TargetAlpha = 0.0f;
    }
}