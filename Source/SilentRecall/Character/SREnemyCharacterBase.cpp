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
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        if (!IsValid(this) || !InventoryComponent || !DefaultWeaponData) return; 

        // 1. GAS 무기 인스턴스 데이터 생성
        USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(InventoryComponent);
        NewInstance->InitializeInstance(DefaultWeaponData, 999); 

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // 2. 🌟 [핵심 변경] 픽업 액터 클래스 대신, 빈 AActor를 소환하여 순수 무기 메시만 동적 이식합니다.
        AActor* PureVisualWeaponActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), GetActorTransform(), SpawnParams);

        if (PureVisualWeaponActor)
        {
            USkeletalMeshComponent* WeaponMeshComp = NewObject<USkeletalMeshComponent>(PureVisualWeaponActor, TEXT("WeaponSkeletalMesh"));
            WeaponMeshComp->RegisterComponent();
            PureVisualWeaponActor->SetRootComponent(WeaponMeshComp);

            // 데이터 애셋에 들어있는 원본 3D 무기 메시와 무기 전용 AnimBP 주입
            if (DefaultWeaponData->WeaponMesh)
            {
                WeaponMeshComp->SetSkeletalMeshAsset(DefaultWeaponData->WeaponMesh);
            }
            if (DefaultWeaponData->WeaponMeshAnimClass)
            {
                WeaponMeshComp->SetAnimInstanceClass(DefaultWeaponData->WeaponMeshAnimClass);
            }

            // 적이 들고 있을 때의 불필요한 무기 자체 콜리전은 완벽하게 차단합니다.
            WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            WeaponMeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);

            // 3. 인벤토리에 이 깨끗한 비주얼 액터를 넘겨 정식 장착/스왑 프로세스를 태웁니다.
            InventoryComponent->AddWeapon(DefaultWeaponData->WeaponSlotType, NewInstance, PureVisualWeaponActor);
        }
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

    UE_LOG(LogTemp, Warning, TEXT("[AI_Brain] %s 의 내부 인공지능 로직 및 StateTree를 전면 정지합니다."), *GetName());

    // ==========================================================
    // 🌲 1. StateTree 셧다운 및 AI 컨트롤러 해제 (기존 유지)
    // ==========================================================
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
    if (UStateTreeComponent* ActorStateTreeComp = FindComponentByClass<UStateTreeComponent>())
    {
        ActorStateTreeComp->StopLogic(TEXT("Enemy Dead"));
    }

    // ==========================================================
    // 💀 2. 사망 비주얼/물리 셋업 GA 발동 (기존 유지)
    // ==========================================================
    if (UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(this))
    {
        FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.Death")); 
        LocalASC->TryActivateAbilitiesByTag(FGameplayTagContainer(DeathTag));
    }

    // ==========================================================
    // ⚔️ 3. [개편] 손에 들린 비주얼 무기 즉시 제거 및 360도 무작위 전리품 사방 분사
    // ==========================================================
    TArray<TSubclassOf<AActor>> FinalDropClasses;

    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
    {
        AActor* VisualWeaponActor = InventoryComponent->GetCurrentActiveWeaponActor();
        
        // ① 들고 있던 무기의 원본 픽업 클래스를 드롭 예정 목록에 수집
        if (bDropCurrentWeapon && DefaultWeaponData)
        {
            FinalDropClasses.Add(DefaultWeaponData->WeaponClass);
        }

        // ② 유령처럼 허공에 남지 않도록 적의 장착 무기 비주얼 액터는 즉시 깔끔하게 소멸시킵니다.
        VisualWeaponActor->Destroy();
    }

    // 디테일 창 배열에 기입한 추가 보상 전리품들을 드롭 목록에 병합
    for (auto& DropClass : ItemDropTable)
    {
        if (DropClass) FinalDropClasses.Add(DropClass);
    }

    // 🎲 수집된 모든 아이템들을 360도 사방 랜덤 벡터로 뿜어냅니다!
    FVector DropOrigin = GetActorLocation() + FVector(0.0f, 0.0f, 20.0f); // 허리 높이에서 방출
    
    for (auto& ClassToDrop : FinalDropClasses)
    {
        if (!ClassToDrop) continue;

        // 3차 크래시 완벽 차단용 안전 지연 스폰(Deferred) 가동
        AActor* SpawnedDrop = GetWorld()->SpawnActorDeferred<AActor>(
            ClassToDrop, FTransform(FRotator::ZeroRotator, DropOrigin), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (SpawnedDrop)
        {
            // 탄약 데이터 보존 세팅이 있다면 적용
            if (SpawnedDrop->Implements<UItemStateInterface>() && DefaultWeaponData)
            {
                if (USRWeaponInstance* CurrentInst = InventoryComponent->GetCurrentActiveWeaponInstance())
                {
                    IItemStateInterface::Execute_SetDroppedAmmo(SpawnedDrop, CurrentInst->CurrentAmmoInMag);
                }
            }

            // 1차 방어선: 즉각적인 루팅 플래그 잠금
            ASRItemPickupBase* PickupBase = Cast<ASRItemPickupBase>(SpawnedDrop);
            if (PickupBase)
            {
                PickupBase->StartPickupCooldown(1.5f);
            }

            // 2차 방어선: 스폰 마감 중 동기 오버랩 차단용 폰 채널 이그노어 무력화
            if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(SpawnedDrop->GetRootComponent()))
            {
                RootPrim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
            }

            // 안전하게 스폰 가두리 양식 마감
            SpawnedDrop->FinishSpawning(FTransform(FRotator::ZeroRotator, DropOrigin));

            // 💫 [랜덤 포물선 연산] 360도 전 방향 무작위 수평 각도 계산 + 수직 상승 바이어스
            float RandomYaw = FMath::FRandRange(0.0f, 360.0f);
            FVector RandomHorizontalDir = FRotator(0.0f, RandomYaw, 0.0f).Vector();
            
            // 수평 밀치기 힘(200~350) + 위로 솟구치는 힘(200~350)을 조합하여 역동적인 분수 연출 완성
            FVector RandomThrowForce = (RandomHorizontalDir * FMath::FRandRange(200.0f, 350.0f)) + (FVector::UpVector * FMath::FRandRange(200.0f, 350.0f));

            if (PickupBase)
            {
                PickupBase->InitDroppedItem(RandomThrowForce);
            }
        }
    }

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