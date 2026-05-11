// Fill out your copyright notice in the Description page of Project Settings.

#include "LevelDesignManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "UObject/UnrealType.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Misc/Guid.h"
#include "GameFramework/PlayerStart.h"
#include "Components/ArrowComponent.h"

ALevelDesignManager::ALevelDesignManager()
{
    PrimaryActorTick.bCanEverTick = false;
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    RootComponent = DirectionArrow;
}

ALevelDesignManager* ALevelDesignManager::GetAILevelManager(UObject* WorldContextObject)
{
    return Cast<ALevelDesignManager>(UGameplayStatics::GetActorOfClass(WorldContextObject, ALevelDesignManager::StaticClass()));
}

void ALevelDesignManager::RunAIWorkflow()
{
    if (bIsAIRunning) 
    {
       UE_LOG(LogTemp, Warning, TEXT("⚠️ [AI 레벨 디자인] 이미 AI가 작업 중입니다. 잠시만 기다려주세요."));
       return;
    }

    if (CustomPrompt.TrimStartAndEnd().IsEmpty())
    {
       UE_LOG(LogTemp, Error, TEXT("❌ [AI 레벨 디자인] 프롬프트가 비어있습니다."));
       return;
    }

    for (int32 i = ManagedActors.Num() - 1; i >= 0; --i)
    {
       if (!IsValid(ManagedActors[i]) || ManagedActors[i]->IsActorBeingDestroyed()) 
       { 
           ManagedActors.RemoveAt(i); 
       }
    }

    UE_LOG(LogTemp, Warning, TEXT("======================================================"));
    UE_LOG(LogTemp, Warning, TEXT("🤖 [AI 레벨 디자인] 1단계: 맵 데이터 수집 및 파이썬 실행 준비..."));

    FVector StartOrigin = GetActorLocation();
    FVector ForwardDir = GetActorForwardVector();

    FString GridString = FString::Printf(TEXT("[World Orientation]\nForward Vector: X:%f, Y:%f, Z:%f\n\n[Player Relative Location]\n"), ForwardDir.X, ForwardDir.Y, ForwardDir.Z);

    FVector PlayerLoc = FVector::ZeroVector;
    AActor* PlayerStart = UGameplayStatics::GetActorOfClass(GetWorld(), APlayerStart::StaticClass());
    if (PlayerStart) { PlayerLoc = PlayerStart->GetActorLocation() - StartOrigin; }
    GridString += FString::Printf(TEXT("X: %f, Y: %f, Z: %f\n\n[3D Map Data]\n"), PlayerLoc.X, PlayerLoc.Y, PlayerLoc.Z);

    FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(VoxelSize * 0.45f));
    for (int32 z = 0; z < GridDepthZ; ++z)
    {
       for (int32 y = 0; y < GridHeightY; ++y)
       {
          for (int32 x = 0; x < GridWidthX; ++x)
          {
             FVector VoxelCenter = StartOrigin + FVector(x * VoxelSize, y * VoxelSize, z * VoxelSize);
             bool bIsBlocked = GetWorld()->OverlapAnyTestByChannel(VoxelCenter, FQuat::Identity, ECC_WorldStatic, BoxShape);
             GridString += bIsBlocked ? TEXT("1") : TEXT("0");
          }
          GridString += TEXT("\n");
       }
    }

    FString CurrentState = TEXT("\n[Current Managed Actors]\n");
    for (AActor* Actor : ManagedActors)
    {
       if (IsValid(Actor) && Actor->Tags.Num() > 0 && !Actor->IsA<ATextRenderActor>())
       {
          FVector Loc = Actor->GetActorLocation() - StartOrigin;
          
          // 🌟 콜리전 기반 실제 끝점(Edge) 추출 로직
          FVector BoundsOrigin, BoxExtent;
          Actor->GetActorBounds(true, BoundsOrigin, BoxExtent); 
          
          FVector RelativeOrigin = BoundsOrigin - StartOrigin;
          float MaxX = RelativeOrigin.X + BoxExtent.X;
          float MinX = RelativeOrigin.X - BoxExtent.X;
          float MaxY = RelativeOrigin.Y + BoxExtent.Y;
          float MinY = RelativeOrigin.Y - BoxExtent.Y;
          float MaxZ = RelativeOrigin.Z + BoxExtent.Z;
          float MinZ = RelativeOrigin.Z - BoxExtent.Z;

          CurrentState += FString::Printf(TEXT("- ID: %s, Class: %s, Loc: (x:%.0f, y:%.0f, z:%.0f), Scale: (x:%.1f, y:%.1f, z:%.1f), MaxX:%.2f, MinX:%.2f, MaxY:%.2f, MinY:%.2f, MaxZ:%.2f, MinZ:%.2f\n"), 
             *Actor->Tags[0].ToString(), *Actor->GetClass()->GetName(), Loc.X, Loc.Y, Loc.Z, 
             Actor->GetActorScale3D().X, Actor->GetActorScale3D().Y, Actor->GetActorScale3D().Z,
             MaxX, MinX, MaxY, MinY, MaxZ, MinZ);
       }
    }

    FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    FFileHelper::SaveStringToFile(GridString, *(SavedDir + TEXT("3D_MapData.txt")));
    FFileHelper::SaveStringToFile(CurrentState, *(SavedDir + TEXT("CurrentState.txt")));
    FFileHelper::SaveStringToFile(CustomPrompt, *(SavedDir + TEXT("PromptData.txt")));

    FString JsonPath = SavedDir + TEXT("SpawnData_3D.json");
    if (FPaths::FileExists(JsonPath))
    {
        IFileManager::Get().Delete(*JsonPath);
    }

    FString PythonScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() + TEXT("AILevelDesign/LevelGenerator.py"));
    FString Params = FString::Printf(TEXT("\"%s\" \"%s\""), *PythonScriptPath, *SavedDir);
    
    uint32 ProcessID;
    FProcHandle ProcHandle = FPlatformProcess::CreateProc(TEXT("python"), *Params, false, false, true, &ProcessID, 0, nullptr, nullptr, nullptr);
    
    if (ProcHandle.IsValid())
    {
       bIsAIRunning = true;
       AIProcessHandle = ProcHandle;
       UE_LOG(LogTemp, Warning, TEXT("🚀 [AI 레벨 디자인] 2단계: 파이썬 스크립트 실행 중... (Process ID: %u)"), ProcessID);

       TWeakObjectPtr<ALevelDesignManager> WeakThis(this);
       Async(EAsyncExecution::Thread, [ProcHandle, WeakThis]() mutable {
          FPlatformProcess::WaitForProc(ProcHandle);
          AsyncTask(ENamedThreads::GameThread, [WeakThis]() {
             if (WeakThis.IsValid()) { 
                WeakThis->bIsAIRunning = false; 
                UE_LOG(LogTemp, Warning, TEXT("✅ [AI 레벨 디자인] 3단계: 파이썬 작업 종료. 결과 분석을 시작합니다."));
                WeakThis->ProcessSpawnData(); 
             }
          });
       });
    }
    else
    {
       UE_LOG(LogTemp, Error, TEXT("❌ [AI 레벨 디자인] 파이썬 프로세스 실행 실패!"));
    }
}

void SafeGetVec(const TSharedPtr<FJsonObject>* Obj, FVector& Out)
{
    if (Obj && Obj->IsValid()) {
       double x=0, y=0, z=0;
       (*Obj)->TryGetNumberField(TEXT("x"), x); (*Obj)->TryGetNumberField(TEXT("y"), y); (*Obj)->TryGetNumberField(TEXT("z"), z);
       Out = FVector(x, y, z);
    }
}

void SafeGetRot(const TSharedPtr<FJsonObject>* Obj, FRotator& Out)
{
    if (Obj && Obj->IsValid()) {
       double pitch=0, yaw=0, roll=0;
       (*Obj)->TryGetNumberField(TEXT("pitch"), pitch); (*Obj)->TryGetNumberField(TEXT("yaw"), yaw); (*Obj)->TryGetNumberField(TEXT("roll"), roll);
       Out = FRotator(pitch, yaw, roll);
    }
}

AActor* ALevelDesignManager::FindActorByID(const FString& ID)
{
    for (AActor* A : ManagedActors) if (IsValid(A) && A->ActorHasTag(FName(*ID))) return A;
    return nullptr;
}

void ALevelDesignManager::ProcessSpawnData()
{
    FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    FString JsonPath = SavedDir + TEXT("SpawnData_3D.json");
    FString JsonString;

    if (!FFileHelper::LoadFileToString(JsonString, *JsonPath))
    {
       UE_LOG(LogTemp, Error, TEXT("❌ [AI 레벨 디자인] 결과 파일(SpawnData_3D.json)을 찾을 수 없습니다."));
       return;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonArray)) 
    {
       UE_LOG(LogTemp, Error, TEXT("❌ [AI 레벨 디자인] JSON 데이터 파싱 실패!"));
       return;
    }

    FVector Origin = GetActorLocation();
    FName MainFolderName = TEXT("AI_Generated_Level");
    FName DebugFolderName = TEXT("AI_Generated_Level/DebugTexts");

    // 🌟 이번 프롬프트로 생성된 액터들을 담을 스택 묶음
    FGenerationStep CurrentStep;

    for (auto& V : JsonArray)
    {
       auto Obj = V->AsObject(); if (!Obj.IsValid()) continue;
       
       FString Action = TEXT("Create"); Obj->TryGetStringField(TEXT("action"), Action);
       FString TargetID; Obj->TryGetStringField(TEXT("actor_id"), TargetID);
       FString ShortTitle = TEXT("Action"); Obj->TryGetStringField(TEXT("short_title"), ShortTitle);
       FString Reasoning; Obj->TryGetStringField(TEXT("reasoning"), Reasoning);
       FString AssetName; Obj->TryGetStringField(TEXT("asset_name"), AssetName);

       if (!Reasoning.IsEmpty()) {
           UE_LOG(LogTemp, Warning, TEXT("💡 [맥락] %s (%s): %s"), *ShortTitle, *AssetName, *Reasoning);
       }

       FVector TextSpawnLoc = FVector::ZeroVector;
       bool bShouldSpawnText = false;

       if (Action == TEXT("Delete")) {
          if (AActor* A = FindActorByID(TargetID)) { 
             TextSpawnLoc = A->GetActorLocation(); bShouldSpawnText = true;
             ManagedActors.Remove(A); A->Destroy(); 
          }
       }
       else if (Action == TEXT("Modify")) {
          if (AActor* A = FindActorByID(TargetID)) {
             const TSharedPtr<FJsonObject>* L; if (Obj->TryGetObjectField(TEXT("location"), L)) { FVector v; SafeGetVec(L, v); A->SetActorLocation(Origin + v); }
             const TSharedPtr<FJsonObject>* S; if (Obj->TryGetObjectField(TEXT("scale"), S)) { FVector scaleVec; SafeGetVec(S, scaleVec); A->SetActorScale3D(scaleVec); }
             const TSharedPtr<FJsonObject>* R; if (Obj->TryGetObjectField(TEXT("rotation"), R)) { FRotator rot; SafeGetRot(R, rot); A->SetActorRotation(rot); }
             TextSpawnLoc = A->GetActorLocation(); bShouldSpawnText = true;
#if WITH_EDITOR
             A->SetFolderPath(MainFolderName); 
#endif
          }
       }
       else {
          const TSharedPtr<FJsonObject>* L; FVector v=FVector::ZeroVector; if(Obj->TryGetObjectField(TEXT("location"), L)) SafeGetVec(L, v);
          const TSharedPtr<FJsonObject>* R; FRotator rot=FRotator::ZeroRotator; if(Obj->TryGetObjectField(TEXT("rotation"), R)) SafeGetRot(R, rot);

          AActor* A = nullptr;
          if (AssetDictionary.Contains(AssetName) && AssetDictionary[AssetName]) {
             A = GetWorld()->SpawnActor<AActor>(AssetDictionary[AssetName], Origin + v, rot);
          } else {
             AStaticMeshActor* MA = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Origin + v, rot);
             FString P = (AssetName.Contains(TEXT("Sphere"))) ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Cube.Cube");
             if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *P)) MA->GetStaticMeshComponent()->SetStaticMesh(Mesh); 
             A = MA;
          }
          if (A) {
             A->Tags.Add(FName(*(AssetName + TEXT("_") + FGuid::NewGuid().ToString().Left(8))));
#if WITH_EDITOR
             A->SetFolderPath(MainFolderName); 
#endif
             // 🌟 액터 관리 및 현재 스택에 추가
             ManagedActors.Add(A);
             CurrentStep.SpawnedActors.Add(A); 
             
             const TSharedPtr<FJsonObject>* S; if (Obj->TryGetObjectField(TEXT("scale"), S)) { FVector scaleVec; SafeGetVec(S, scaleVec); A->SetActorScale3D(scaleVec); }
             A->SetActorRotation(rot);
             TextSpawnLoc = A->GetActorLocation(); bShouldSpawnText = true;
          }
       }

       if (bShouldSpawnText) {
          ATextRenderActor* T = GetWorld()->SpawnActor<ATextRenderActor>(ATextRenderActor::StaticClass(), TextSpawnLoc + FVector(0,0,150.0f), FRotator(0,180,0));
          if (T && T->GetTextRender()) {
             T->GetTextRender()->SetText(FText::FromString(ShortTitle));
             T->GetTextRender()->SetTextRenderColor(FColor::Yellow); 
             T->GetTextRender()->SetWorldSize(30.0f);
#if WITH_EDITOR
             T->SetFolderPath(DebugFolderName); 
#endif
             T->Tags.Add(FName(*FGuid::NewGuid().ToString())); 
             // 🌟 텍스트 액터도 관리 및 현재 스택에 추가
             ManagedActors.Add(T);
             CurrentStep.SpawnedActors.Add(T);
          }
       }
    }
    
    // 🌟 1회 생성이 끝나면 이번 묶음을 HistoryStack에 푸시
    if (CurrentStep.SpawnedActors.Num() > 0)
    {
        HistoryStack.Add(CurrentStep);
    }

    UE_LOG(LogTemp, Warning, TEXT("✨ [AI 레벨 디자인] 작업 완료! (배치된 액터: %d개, 누적 스택: %d단계)"), ManagedActors.Num(), HistoryStack.Num());
}

void ALevelDesignManager::CancelAIWorkflow() { if (bIsAIRunning) FPlatformProcess::TerminateProc(AIProcessHandle); }

// 🌟 Undo (실행 취소) 기능 구현
void ALevelDesignManager::UndoLastGeneration()
{
    if (HistoryStack.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [AI 레벨 디자인] 되돌릴 작업 스택이 없습니다."));
        return;
    }

    // 가장 마지막 묶음을 꺼냄 (Pop)
    FGenerationStep LastStep = HistoryStack.Pop();

    int32 DestroyedCount = 0;
    for (AActor* Actor : LastStep.SpawnedActors)
    {
        if (IsValid(Actor))
        {
            ManagedActors.Remove(Actor); 
            Actor->Destroy();            
            DestroyedCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("⏪ [AI 레벨 디자인] 직전 작업 실행 취소 완료! (삭제된 액터: %d개, 남은 스택: %d)"), DestroyedCount, HistoryStack.Num());
}

// 🌟 전체 초기화 기능 구현
void ALevelDesignManager::ClearAllGeneratedLevel()
{
    int32 DestroyedCount = 0;
    for (AActor* Actor : ManagedActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
            DestroyedCount++;
        }
    }
    ManagedActors.Empty();
    HistoryStack.Empty();

    UE_LOG(LogTemp, Warning, TEXT("🗑️ [AI 레벨 디자인] 맵 전체 초기화 완료! (삭제된 액터: %d개)"), DestroyedCount);
}