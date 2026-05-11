#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "SRAIController.generated.h"

UCLASS()
class SILENTRECALL_API ASRAIController : public AAIController
{
    GENERATED_BODY()

public:
    ASRAIController();

    // ==========================================
    // 🧠 State Tree 전달용 데이터 변수
    // ==========================================
    
    // 현재 추적 중인 타겟 (State Tree의 Evaluator가 퍼갈 변수)
    UPROPERTY(BlueprintReadWrite, Category = "AI|Data")
    TObjectPtr<AActor> CurrentTarget;

    // 수색(Investigate)하러 갈 목표 예측 좌표 (Task에서 퍼갈 변수)
    UPROPERTY(BlueprintReadWrite, Category = "AI|Data")
    FVector LastInvestigateLocation;

    // 외부에서 State Tree로 이벤트를 쏘기 위한 헬퍼 함수
    UFUNCTION(BlueprintCallable, Category = "AI")
    void SendStateTreeEvent(FGameplayTag EventTag);

protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;

    // ASC에서 기절 태그가 붙거나 떨어질 때 호출될 콜백 함수
    virtual void OnStunTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    // 퍼셉션 업데이트 시 호출될 함수 (무언가를 보거나 들었을 때)
    UFUNCTION()
    void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    UFUNCTION()
    void OnTargetForgotten(AActor* Actor);
private:
    // 태그 조작을 편하게 하기 위한 내부 헬퍼 함수
    void SetAIStateTag(FGameplayTag NewStateTag);

    // ==========================================
    // 👁️ AI 컴포넌트 세팅
    // ==========================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "AI|Perception")
    class UAIPerceptionComponent* AIPerceptionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "AI|Perception")
    class UAISenseConfig_Sight* SightConfig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "AI|Perception")
    class UAISenseConfig_Hearing* HearingConfig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "AI|StateTree")
    class UStateTreeComponent* StateTreeComp;
};