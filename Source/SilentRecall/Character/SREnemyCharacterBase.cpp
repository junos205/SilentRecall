#include "SREnemyCharacterBase.h"
#include "Character/SRInventoryComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "AIController.h"
#include "Components/StateTreeComponent.h"

ASREnemyCharacterBase::ASREnemyCharacterBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

// 📄 ASREnemyCharacterBase.cpp

void ASREnemyCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    // 🚨 [널 포인터 가드 라인] 인벤토리 컴포넌트 자체가 완전히 무결한지 먼저 검사합니다.
    if (InventoryComponent == nullptr)
    {
        UE_LOG(LogTemp, Fatal, TEXT("[크래시 방어 🚨] %s 의 InventoryComponent(WeaponComponent)가 널(Null)입니다! 블루프린트 서브오브젝트가 깨졌습니다."), *GetName());
        return;
    }

    if (DefaultWeaponData)
    {
        if (UClass* WeaponClassToSpawn = DefaultWeaponData->WeaponClass)
        {
            USRWeaponInstance* NewInstance = NewObject<USRWeaponInstance>(InventoryComponent);
            NewInstance->InitializeInstance(DefaultWeaponData, 999); 

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = this;

            AActor* SpawnedWeaponActor = GetWorld()->SpawnActor<AActor>(WeaponClassToSpawn, GetActorTransform(), SpawnParams);

            if (SpawnedWeaponActor)
            {
                EWeaponSlot SlotToUse = DefaultWeaponData->WeaponSlotType;
                
                // 이제 인벤토리가 무조건 살아있음이 증명되었으므로 안전하게 통과합니다.
                InventoryComponent->AddWeapon(SlotToUse, NewInstance, SpawnedWeaponActor);
            }
        }
    }

    // 부모 멤버 변수 은닉 방지 바인딩 로직
    if (UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(this))
    {
        const USRDefaultAttributeSet* EnemyAttributeSet = Cast<USRDefaultAttributeSet>(LocalASC->GetAttributeSet(USRDefaultAttributeSet::StaticClass()));
        if (EnemyAttributeSet)
        {
            EnemyAttributeSet->OnOutOfHealth.AddDynamic(this, &ASREnemyCharacterBase::HandleOutOfHealth);
        }
    }
}
// ==========================================================
// ⭐️ [역할 분리] 오직 AI 시스템 내부 로직 정지 및 디스폰만 관리!
// ==========================================================
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
        // ⭐️ [StateTree 정지 - 컨트롤러] 
        // 일반적으로 AI용 StateTree(UStateTreeAIComponent)는 AIController에 부착됩니다.
        // 부모 클래스인 UStateTreeComponent로 캐스팅하여 안전하게 찾아냅니다.
        if (UStateTreeComponent* StateTreeComp = AIC->FindComponentByClass<UStateTreeComponent>())
        {
            StateTreeComp->StopLogic(TEXT("Enemy Dead"));
            UE_LOG(LogTemp, Warning, TEXT("[AI_Brain] AIController 내부의 StateTree를 성공적으로 정지했습니다."));
        }

        AIC->ClearFocus(EAIFocusPriority::Gameplay); 
        AIC->StopMovement(); 
        AIC->UnPossess();    
    }

    // ⭐️ [StateTree 정지 - 액터 방어용]
    // 혹시 프로젝트 기획에 따라 StateTreeComponent를 AIController가 아닌 
    // 적 캐릭터 액터 본체에 직접 붙여두셨을 경우를 대비한 2중 안전망 방어 코드입니다.
    if (UStateTreeComponent* ActorStateTreeComp = FindComponentByClass<UStateTreeComponent>())
    {
        ActorStateTreeComp->StopLogic(TEXT("Enemy Dead"));
        UE_LOG(LogTemp, Warning, TEXT("[AI_Brain] Actor 본체 내부의 StateTree를 성공적으로 정지했습니다."));
    }

    // ==========================================================
    // 💀 2. 사망 비주얼/물리 셋업 GA 발동 (기존 코드 유지)
    // ==========================================================
    if (UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(this))
    {
        FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.Death")); 
        LocalASC->TryActivateAbilitiesByTag(FGameplayTagContainer(DeathTag));
    }

    // ==========================================================
    // ⚔️ 3. 무기 탈착 및 물리 가동 (기존 코드 유지)
    // ==========================================================
    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
    {
        AActor* WeaponActor = InventoryComponent->GetCurrentActiveWeaponActor();
        WeaponActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        
        if (UPrimitiveComponent* WeaponRoot = Cast<UPrimitiveComponent>(WeaponActor->GetRootComponent()))
        {
            WeaponRoot->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            WeaponRoot->SetSimulatePhysics(true);
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
    
    // 무기가 남아있다면 무기 액터 먼저 제거
    if (InventoryComponent && InventoryComponent->GetCurrentActiveWeaponActor())
    {
        InventoryComponent->GetCurrentActiveWeaponActor()->Destroy();
    }

    // AI 본체 완전히 삭제
    Destroy();
}