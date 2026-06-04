#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SRItemPickupBase.generated.h"

UCLASS(Abstract)
class SILENTRECALL_API ASRItemPickupBase : public AActor
{
    GENERATED_BODY()
    
public:    
    ASRItemPickupBase();

protected:
    virtual void BeginPlay() override;
    
    /** 자식 클래스들이 구체적인 획득 로직을 구현할 가상 함수 */
    virtual void OnPickedUp(class USRInventoryComponent* InventoryComp) {}

    void AdjustVisualOffset();
    
    // 🌟 [추가] 버려진 아이템을 즉시 다시 줍지 않도록 막는 활성화 플래그
    bool bCanPickup = true;

    // 🌟 [추가] 외부(인벤토리)에서 원거리 드롭 시 쿨타임을 먹이고 스폰할 수 있도록 개방
public:
    void StartPickupCooldown(float CooldownTime);

    void InitDroppedItem(const FVector& ThrowForce);

private:
    void EnablePickup();
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class USphereComponent* CollisionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class UNiagaraComponent* BaseVFXComponent;

    // 🌟 [추가] 내장 무브먼트 컴포넌트들
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class URotatingMovementComponent* RotatingMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class UInterpToMovementComponent* InterpToMovement;

protected:
    // 🌟 [추가] 무기가 바닥에 굴러다니다가 완전히 멈췄는지 실시간 감시하는 타이머용 함수
    void CheckPhysicsRest();

    // 🌟 [추가] 물리 연산을 끄고 위아래가 똑바로 선 '게임적 호버 상태'로 복구하는 함수
    void ActivateHoverState();

    FTimerHandle PhysicsCheckTimer;
    int32 PhysicsCheckCounter = 0; // 무한 구름 방지용 안전 가드 카운터
    
    /** 🌟 [신규 추가] 오직 위아래(인터프) 이동만 안전하게 격리해서 받아낼 퓨어 위치 피벗 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USceneComponent* HoverRoot;

    /** 자식 메쉬들이 부착되어 오직 제자리 회전(로테이팅)만 전담할 비주얼 루트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USceneComponent* VisualRoot;
    

private:
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};