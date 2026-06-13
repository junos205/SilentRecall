#include "SREnemyCharacterBase.h"
#include "Character/SRInventoryComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "AIController.h"
#include "Components/StateTreeComponent.h"
#include "Components/SkeletalMeshComponent.h" // 🌟 무기 메시 주입용 헤더 추가
#include "Interface/ItemStateInterface.h"
#include "Weapon/SRItemPickupBase.h"
#include "Game/SRGameInstance.h"

ASREnemyCharacterBase::ASREnemyCharacterBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ASREnemyCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    if (InventoryComponent == nullptr)
    {
        InventoryComponent = FindComponentByClass<USRInventoryComponent>();
        
        if (InventoryComponent == nullptr)
        {
            UE_LOG(LogTemp, Error, TEXT("[크래시 방어 💡] %s 의 InventoryComponent가 없습니다!"), *GetName());
        }
    }

    // 🎯 [수정] 메시가 확실히 준비된 후, 플레이어와 동일한 '순수 비주얼 액터' 파이프라인으로 무기를 스폰합니다.
    // 🎯 기존의 복잡하게 직접 무기를 스폰하던 람다 로직 전체를 파괴하고 아래처럼 간소화합니다.
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        if (!IsValid(this) || !InventoryComponent || !DefaultWeaponData) return; 

        // 1. 데이터 인스턴스 생성
        USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(InventoryComponent);
        NewInstance->InitializeInstance(DefaultWeaponData, 999); 

        // ❌ 무기 액터를 여기서 직접 스폰(SpawnActor)하지 마세요! 
        // 어차피 InventoryComponent->AddWeapon 내부에서 알아서 액터를 새로 스폰하고 부착해줍니다.
    
        // 2. 🌟 인벤토리에 데이터만 깔끔하게 넘겨 장착 프로세스를 태웁니다.
        InventoryComponent->AddWeapon(DefaultWeaponData->WeaponSlotType, NewInstance, nullptr); 
    });

    // 부모 멤버 변수 은닉 방지 바인딩 로직 (유지)
    if (UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(this))
    {
        const USRDefaultAttributeSet* EnemyAttributeSet = Cast<USRDefaultAttributeSet>(LocalASC->GetAttributeSet(USRDefaultAttributeSet::StaticClass()));
        if (EnemyAttributeSet)
        {
            EnemyAttributeSet->OnOutOfHealth.AddDynamic(this, &ASREnemyCharacterBase::HandleOutOfHealth);
        }
    }
}

void ASREnemyCharacterBase::HandleOutOfHealth(AActor* TargetActor)
{
    if (bIsDead) return;
    bIsDead = true;

    // 1. 영구 사망자 명단 등록 (유지)
    if (USRGameInstance* GI = Cast<USRGameInstance>(GetGameInstance()))
    {
        GI->DefeatedEnemyNames.Add(GetFName());
    }

    UE_LOG(LogTemp, Error, TEXT("============= [DropDebug] %s 사망 연출 및 드롭 프로세스 가동 ============="), *GetName());

    // 🌲 1. StateTree 셧다운 및 AI 컨트롤러 해제 (유지)
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UStateTreeComponent* StateTreeComp = AIC->FindComponentByClass<UStateTreeComponent>())
        {
            StateTreeComp->StopLogic(TEXT("Enemy Dead"));
        }
        AIC->ClearFocus(EAIFocusPriority::Gameplay); 
        AIC->StopMovement(); 
        AIC->UnPossess();    
    }

    // =======================================================================
    // ❌ [삭제] 이 구역은 이제 완전히 지워버리세요!
    // =======================================================================
    // if (UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(this))
    // {
    //     FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.Death")); 
    //     LocalASC->TryActivateAbilitiesByTag(FGameplayTagContainer(DeathTag));
    //     LocalASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead")));
    // }
    // =======================================================================

    // ⚔️ 3. 전리품 사방 분사 파이프라인 (이하 기존 코드 동일)
    TArray<TSubclassOf<AActor>> FinalDropClasses;

    if (InventoryComponent)
    {
        AActor* VisualWeaponActor = InventoryComponent->GetCurrentActiveWeaponActor();
        UE_LOG(LogTemp, Warning, TEXT("[DropDebug] 인벤토리 컴포넌트 탐색 성공. 현재 장착 무기 액터 포인터: %s"), VisualWeaponActor ? *VisualWeaponActor->GetName() : TEXT("NULL (손에 무기가 없음!)"));
        
        if (VisualWeaponActor)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DropDebug] bDropCurrentWeapon 플래그 상태: %s | DefaultWeaponData 존재 여부: %s"), 
                bDropCurrentWeapon ? TEXT("TRUE") : TEXT("FALSE"), 
                DefaultWeaponData ? TEXT("유효함") : TEXT("NULL"));

            if (bDropCurrentWeapon && DefaultWeaponData)
            {
                UE_LOG(LogTemp, Warning, TEXT("[DropDebug] DefaultWeaponData->WeaponClass 상태: %s"), 
                    DefaultWeaponData->WeaponClass ? *DefaultWeaponData->WeaponClass->GetName() : TEXT("NULL (픽업 클래스가 비어있음!)"));

                if (DefaultWeaponData->WeaponClass)
                {
                    FinalDropClasses.Add(DefaultWeaponData->WeaponClass);
                    UE_LOG(LogTemp, Log, TEXT("[DropDebug] ➕ 기본 장착 무기 클래스 수집 완료."));
                }
            }
            VisualWeaponActor->Destroy();
            UE_LOG(LogTemp, Log, TEXT("[DropDebug] 적 장착 비주얼 무기 액터 월드 제거 완료."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[DropDebug] 🚨 인벤토리 컴포넌트 자체가 NULL입니다! 무기 드롭 탐색 불가."));
    }

    // 아이템 드롭 테이블 추가 수집 로그
    UE_LOG(LogTemp, Log, TEXT("[DropDebug] 추가 전리품 테이블(ItemDropTable) 수색 시작 (슬롯 수: %d)"), ItemDropTable.Num());
    for (int32 i = 0; i < ItemDropTable.Num(); ++i)
    {
        if (ItemDropTable[i])
        {
            FinalDropClasses.Add(ItemDropTable[i]);
            UE_LOG(LogTemp, Log, TEXT("[DropDebug] ➕ 추가 드롭 아이템 [%d] 수집: %s"), i, *ItemDropTable[i]->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[DropDebug] ⚠️ 추가 드롭 아이템 [%d] 슬롯이 None 상태입니다."), i);
        }
    }

    // 📦 중간 정산선
    UE_LOG(LogTemp, Error, TEXT("[DropDebug] 📦 [최종 결과] 드롭 아이템 바구니(FinalDropClasses)에 담긴 총 클래스 개수: %d 개"), FinalDropClasses.Num());

    FVector EnemyForward = GetActorForwardVector();
    FVector DropOrigin = GetActorLocation() + (EnemyForward * 50.0f) + FVector(0.0f, 0.0f, 50.0f); 
    UE_LOG(LogTemp, Warning, TEXT("[DropDebug] 연산된 스폰 기준 공간 좌표(DropOrigin): %s"), *DropOrigin.ToString());
    
    int32 SpawningSuccessCounter = 0;

    for (int32 Index = 0; Index < FinalDropClasses.Num(); ++Index)
    {
        TSubclassOf<AActor> ClassToDrop = FinalDropClasses[Index];
        if (!ClassToDrop)
        {
            UE_LOG(LogTemp, Error, TEXT("[DropDebug] 🚨 루프 에러: [%d]번째 배열 알맹이가 유효하지 않은 클래스(Null)입니다. 스킵합니다."), Index);
            continue;
        }

        UE_LOG(LogTemp, Warning, TEXT("[DropDebug] ▶ [%d]번째 전리품 스폰 집행 돌입 -> 클래스명: %s"), Index, *ClassToDrop->GetName());

        // 🟢 Owner 단절(nullptr) 독립 소환 집행
        AActor* SpawnedDrop = GetWorld()->SpawnActorDeferred<AActor>(
            ClassToDrop, FTransform(FRotator::ZeroRotator, DropOrigin), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (SpawnedDrop)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DropDebug] 🟢 1단계 승인: SpawnActorDeferred 성공! 월드 메모리 객체 명칭: %s"), *SpawnedDrop->GetName());
            SpawningSuccessCounter++;

            if (SpawnedDrop->Implements<UItemStateInterface>() && DefaultWeaponData)
            {
                if (USRWeaponInstance* CurrentInst = InventoryComponent->GetCurrentActiveWeaponInstance())
                {
                    IItemStateInterface::Execute_SetDroppedAmmo(SpawnedDrop, CurrentInst->CurrentAmmoInMag);
                    UE_LOG(LogTemp, Log, TEXT("[DropDebug] 인터페이스 식별 성공: 탄약 수 주입 완료 (%d 발)"), CurrentInst->CurrentAmmoInMag);
                }
            }

            ASRItemPickupBase* PickupBase = Cast<ASRItemPickupBase>(SpawnedDrop);
            if (PickupBase)
            {
                PickupBase->StartPickupCooldown(1.5f);
                UE_LOG(LogTemp, Log, TEXT("[DropDebug] ASRItemPickupBase 타입 검증 성공. 무적 쿨타임 가동."));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[DropDebug] ⚠️ 안내: 스폰된 액터가 ASRItemPickupBase 자식이 아닙니다. 일반 액터로 취급합니다."));
            }

            if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(SpawnedDrop->GetRootComponent()))
            {
                RootPrim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        
                // =======================================================================
                // 🟢 [제 1방어선] 적 본체 및 요동치는 래그돌 메시와 절대 충돌하지 않도록 상호 무시 주입!
                // =======================================================================
                RootPrim->IgnoreActorWhenMoving(this, true); 
                if (GetMesh()) GetMesh()->IgnoreActorWhenMoving(SpawnedDrop, true);
                // =======================================================================
            }

            SpawnedDrop->FinishSpawning(FTransform(FRotator::ZeroRotator, DropOrigin));
            UE_LOG(LogTemp, Warning, TEXT("[DropDebug] 🟢 2단계 승인: FinishSpawning 안전 마감 완료!"));

            float RandomYaw = FMath::FRandRange(0.0f, 360.0f);
            FVector RandomHorizontalDir = FRotator(0.0f, RandomYaw, 0.0f).Vector();
            FVector RandomThrowForce = (RandomHorizontalDir * FMath::FRandRange(250.0f, 400.0f)) + (FVector::UpVector * FMath::FRandRange(200.0f, 350.0f));

            if (PickupBase)
            {
                PickupBase->InitDroppedItem(RandomThrowForce);
                UE_LOG(LogTemp, Log, TEXT("[DropDebug] InitDroppedItem 물리 추진기 작동 완료 (임펄스 량: %s)"), *RandomThrowForce.ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[DropDebug] ❌ 1단계 거절: SpawnActorDeferred가 nullptr을 리턴했습니다!! 엔진이 소환을 전면 거부함. 클래스 에셋 타깃팅 불량 확률 100%%."));
        }
    }

    UE_LOG(LogTemp, Error, TEXT("============= [DropDebug] 전리품 드롭 종료 (최종 결과 스폰 성공 수: %d / 총 요청 수: %d) ============="), SpawningSuccessCounter, FinalDropClasses.Num());

    // 4. 3초 타이머 가동 (본체 소멸)
    GetWorld()->GetTimerManager().SetTimer(
        DeathTimerHandle, 
        this, 
        &ASREnemyCharacterBase::DestroyEnemySequence, 
        3.0f, 
        false
    );
}

void ASREnemyCharacterBase::DestroyEnemySequence()
{
    UE_LOG(LogTemp, Warning, TEXT("[AI_Brain] 3초 경과. 액터를 최종 소멸시킵니다."));
    
    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
    {
        InventoryComponent->GetCurrentActiveWeaponActor()->Destroy();
    }

    Destroy();
}