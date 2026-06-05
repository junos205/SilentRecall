#include "SREnemyCharacterBase.h"
#include "Character/SRInventoryComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "AIController.h"
#include "Components/StateTreeComponent.h"
#include "Components/SkeletalMeshComponent.h" // 🌟 무기 메시 주입용 헤더 추가

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
    // 🌲 1. StateTree 셧다운 및 AI 컨트롤러 해제
    // ==========================================================
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UStateTreeComponent* StateTreeComp = AIC->FindComponentByClass<UStateTreeComponent>())
        {
            StateTreeComp->StopLogic(TEXT("Enemy Dead"));
            UE_LOG(LogTemp, Warning, TEXT("[AI_Brain] AIController 내부의 StateTree를 성공적으로 정지했습니다."));
        }

        AIC->ClearFocus(EAIFocusPriority::Gameplay); 
        AIC->StopMovement(); 
        AIC->UnPossess();    
    }

    if (UStateTreeComponent* ActorStateTreeComp = FindComponentByClass<UStateTreeComponent>())
    {
        ActorStateTreeComp->StopLogic(TEXT("Enemy Dead"));
        UE_LOG(LogTemp, Warning, TEXT("[AI_Brain] Actor 본체 내부의 StateTree를 성공적으로 정지했습니다."));
    }

    // ==========================================================
    // 💀 2. 사망 비주얼/물리 셋업 GA 발동
    // ==========================================================
    if (UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(this))
    {
        FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.Death")); 
        LocalASC->TryActivateAbilitiesByTag(FGameplayTagContainer(DeathTag));
    }

    // ==========================================================
    // ⚔️ 3. [수정] 무기 탈착 및 현실적인 래그돌 낙하 물리 가동 (Juice 연출)
    // ==========================================================
    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
    {
        AActor* WeaponActor = InventoryComponent->GetCurrentActiveWeaponActor();
        WeaponActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        
        if (UPrimitiveComponent* WeaponRoot = Cast<UPrimitiveComponent>(WeaponActor->GetRootComponent()))
        {
            // 월드 정적/동적 오브젝트들과 충돌하여 바닥에 팅기도록 설정
            WeaponRoot->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            WeaponRoot->SetCollisionResponseToAllChannels(ECR_Block);
            
            // 💡 플레이어가 지나가다 적 무기 래그돌에 걸려 넘어지거나 덜컹거리지 않도록 폰(Pawn) 채널만 무시합니다.
            WeaponRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
            WeaponRoot->SetSimulatePhysics(true);

            // 💫 [연출 보정] 적이 죽을 때 무기가 아래로 툭 무겁게 떨어지는 대신, 
            // 전방과 위쪽으로 살짝 튕겨 나가듯 떨어지게 미세한 물리 충격(Impulse)을 가합니다.
            FVector DeathDropImpulse = GetActorForwardVector() * 70.0f + FVector::UpVector * 50.0f;
            WeaponRoot->AddImpulse(DeathDropImpulse, NAME_None, true);
        }
    }

    // 4. 3초 타이머 가동
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