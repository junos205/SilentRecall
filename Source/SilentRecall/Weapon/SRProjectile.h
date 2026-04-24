#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SRProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UAbilitySystemComponent;

UCLASS()
class SILENTRECALL_API ASRProjectile : public AActor
{
	GENERATED_BODY()
    
public: 
	ASRProjectile();

	// ⭐️ GA에서 호출할 스탯 주입 함수!
	void SetSpeed(float InSpeed, FVector ShootDirection);

	void SetImpactForce(float InForce) { ImpactForce = InForce; }

	// ⭐️ 이 총알을 쏜 주인이 누구인지 (나중에 반사될 때 주인이 바뀜!)
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	AActor* InstigatorActor;

	// ⭐️ 무기 데이터 애셋에서 가져온 이 투사체의 데미지
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	float DamageAmount;

	// 패링 당했을 때 호출될 유턴 함수!
	void DeflectProjectile(AActor* NewInstigator);
	
	// ⭐️ GA가 넘겨줄 데미지 이펙트 클래스
	UPROPERTY()
	TSubclassOf<class UGameplayEffect> DamageEffectClass;
	
protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true")) 
	float ImpactForce;
	
	// GA와 통신할 때 쓸 태그 ("Event.Ranged.Fire")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	FGameplayTag HitEventTag;

	// 충돌 판정 함수
	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	
};
