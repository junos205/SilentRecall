#include "SRAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/SRPlayerCharacter.h" // 플레이어 식별용
#include "Components/StateTreeComponent.h"
#include "StateTreeEvents.h" // FStateTreeEvent 구조체용

ASRAIController::ASRAIController()
{
    // 1. 퍼셉션 컴포넌트 생성
    AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));

    // 2. 시각(Sight) 세팅
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 1500.0f; // 시야 반경
    SightConfig->LoseSightRadius = 2000.0f; // 시야를 잃는 거리
    SightConfig->PeripheralVisionAngleDegrees = 60.0f; // 시야각 (120도)
    SightConfig->SetMaxAge(5.0f); // 감지 기억 시간 (5초간 기억 유지)
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    AIPerceptionComp->ConfigureSense(*SightConfig);

    // 3. 청각(Hearing) 세팅
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 1000.0f;
    HearingConfig->SetMaxAge(3.0f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    AIPerceptionComp->ConfigureSense(*HearingConfig);

    AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
    
    // 4. State Tree 컴포넌트 생성
    StateTreeComp = CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTreeComp"));
}

void ASRAIController::BeginPlay()
{
    Super::BeginPlay();

    // 퍼셉션 이벤트 바인딩
    if (AIPerceptionComp)
    {
        AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ASRAIController::OnTargetPerceptionUpdated);
        AIPerceptionComp->OnTargetPerceptionForgotten.AddDynamic(this, &ASRAIController::OnTargetForgotten);
    }
}

void ASRAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    SetAIStateTag(FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Idle")));

    // ⭐️ 빙의하는 순간, 내 폰의 ASC를 가져와서 기절(Stun) 태그 감시를 걸어둡니다.
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InPawn);
    if (ASC)
    {
        FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun"));
        ASC->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ASRAIController::OnStunTagChanged);
    }

    // ⭐️ State Tree 실행 시작 (뇌 가동)
    if (StateTreeComp)
    {
        StateTreeComp->StartLogic(); 
    }
}

void ASRAIController::OnStunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    // NewCount가 0보다 크다 = 기절 태그가 방금 붙었다
    if (NewCount > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SRAIController] Stun Tag Detected! Sending Event to State Tree."));
        SendStateTreeEvent(FGameplayTag::RequestGameplayTag(FName("Event.AI.Stun")));
    }
}

void ASRAIController::SendStateTreeEvent(FGameplayTag EventTag)
{
    // State Tree에 즉각적인 전환을 알리는 이벤트를 전송
    if (StateTreeComp)
    {
        FStateTreeEvent Event(EventTag);
        StateTreeComp->SendStateTreeEvent(Event);
    }
}

// ⭐️ [핵심 로직] 시야 및 청각 변화 감지 시 로직
void ASRAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    // ⭐️ [신규 추가] 타겟(플레이어)이 이미 죽었는지 검사합니다!
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
    if (TargetASC && TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"))))
    {
        // 타겟이 죽었다면 내 타겟 정보를 날려버리고 평화(Idle) 상태로 돌아갑니다.
        CurrentTarget = nullptr;
        SetAIStateTag(FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Idle")));
        
        // (필요하다면 State Tree에도 Event.AI.Idle 이벤트를 쏴서 즉시 복귀시킬 수 있습니다)
        // SendStateTreeEvent(FGameplayTag::RequestGameplayTag(FName("Event.AI.Idle")));
        return;
    }
    
    ASRPlayerCharacter* PlayerChar = Cast<ASRPlayerCharacter>(Actor);
    if (!PlayerChar) return;

    // (주의) 이전 코드에 있던 기절 시 return 하는 코드는 삭제했습니다!
    // 기절 중이어도 시야에 보이는 플레이어의 정보(기억)는 업데이트해야 합니다.

    if (Stimulus.WasSuccessfullySensed())
    {
        UE_LOG(LogTemp, Warning, TEXT("AI: Target Detected! Switching to Chase."));
        
        // 1. 현재 타겟 변수 갱신 (Evaluator가 퍼갈 수 있게 세팅)
        CurrentTarget = Actor;
        
        // 2. 상태 태그 갱신 및 이벤트 전송
        SetAIStateTag(FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Chase")));
        SendStateTreeEvent(FGameplayTag::RequestGameplayTag(FName("Event.AI.Chase")));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AI: Target Lost! Predicting location for Investigation."));
        
        // 1. 타겟 상실 위치 예측 알고리즘 (마지막 위치 + 속도 * 1.5초)
        FVector LastLocation = Stimulus.StimulusLocation;
        FVector PlayerVelocity = Actor->GetVelocity();
        LastInvestigateLocation = LastLocation + (PlayerVelocity * 1.5f);

        // 2. 상태 태그 갱신 및 이벤트 전송
        SetAIStateTag(FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Investigate")));
        SendStateTreeEvent(FGameplayTag::RequestGameplayTag(FName("Event.AI.Investigate")));
    }
}

void ASRAIController::OnTargetForgotten(AActor* Actor)
{
    // 5초가 지나 내 머릿속에서 타겟이 지워졌다면 타겟 변수를 비워줍니다.
    if (Actor == CurrentTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("AI: Target completely forgotten. Clearing memory."));
        CurrentTarget = nullptr;
    }
}

// 기존의 AI 상태 태그를 모두 지우고 새 태그만 꽂아주는 헬퍼 함수
void ASRAIController::SetAIStateTag(FGameplayTag NewStateTag)
{
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn());
    if (!ASC) return;

    FGameplayTagContainer AIStateTags;
    AIStateTags.AddTagFast(FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Idle")));
    AIStateTags.AddTagFast(FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Patrol")));
    AIStateTags.AddTagFast(FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Chase")));
    AIStateTags.AddTagFast(FGameplayTag::RequestGameplayTag(FName("Character.State.AI.Investigate")));

    ASC->RemoveLooseGameplayTags(AIStateTags);
    ASC->AddLooseGameplayTag(NewStateTag);
}