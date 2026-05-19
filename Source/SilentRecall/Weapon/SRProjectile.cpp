// Fill out your copyright notice in the Description page of Project Settings.

#include "SRProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h" 
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h" // 안전장치 헤더

// 내 전용 타격 채널 (Damageable) 정의
#define ECC_DAMAGEABLE ECC_GameTraceChannel4

ASRProjectile::ASRProjectile()
{
    HitEventTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.HitReact"));
    PrimaryActorTick.bCanEverTick = false;

    // ==========================================================
    // 🛡️ 1. 콜리전 (투명 구형 충돌체) 정밀 세팅
    // ==========================================================
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    
    // "Custom"으로 선언하여 세밀하게 조율
    CollisionComp->SetCollisionProfileName(TEXT("Custom"));
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); 
    
    // 1단계: 일단 세상 모든 물체를 통과(Ignore)하게 만듭니다.
    CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore); 

    // 2단계: 내가 부딪혀서 터져야 할 것들만 겹침(Overlap)으로 열어줍니다.
    CollisionComp->SetCollisionResponseToChannel(ECC_DAMAGEABLE, ECR_Overlap);    // 적 캐릭터 살점(Mesh)
    CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);   // 콘크리트 벽, 바닥
    CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);  // 움직이는 상자, 문
    CollisionComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);   // 래그돌 상태의 시체

    CollisionComp->SetGenerateOverlapEvents(true);
    CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASRProjectile::OnProjectileOverlap);
    RootComponent = CollisionComp;

    // ==========================================================
    // 🎨 2. 껍데기 메쉬 (시각적인 용도만 수행, 충돌 판정 X)
    // ==========================================================
    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
    ProjectileMesh->SetCollisionProfileName(TEXT("NoCollision"));
    ProjectileMesh->SetGenerateOverlapEvents(false);

    // ==========================================================
    // 🚀 3. 발사체 무브먼트
    // ==========================================================
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = 3000.f; 
    ProjectileMovement->MaxSpeed = 3000.f;
    ProjectileMovement->ProjectileGravityScale = 0.0f; 
    ProjectileMovement->bRotationFollowsVelocity = true; 
    ProjectileMovement->bShouldBounce = false; 
}

void ASRProjectile::SetSpeed(float InSpeed, FVector ShootDirection)
{
    if (ProjectileMovement)
    {
        ProjectileMovement->InitialSpeed = InSpeed;
        ProjectileMovement->MaxSpeed = InSpeed;
        
        // 조준한 '진짜 타격 방향'으로 속도 적용
        ProjectileMovement->Velocity = ShootDirection.GetSafeNormal() * InSpeed; 
    }
}

void ASRProjectile::DeflectProjectile(AActor* NewInstigator)
{
    // 🛡️ 기존 가해자(적)의 면책 특권 박탈 (이제 적을 때릴 수 있음)
    if (InstigatorActor)
    {
        CollisionComp->IgnoreActorWhenMoving(InstigatorActor, false);

        TArray<AActor*> OldAttachedActors;
        InstigatorActor->GetAttachedActors(OldAttachedActors);
        for (AActor* AttachedActor : OldAttachedActors)
        {
            CollisionComp->IgnoreActorWhenMoving(AttachedActor, false);
        }
    }

    // 👑 새로운 가해자(패링한 플레이어) 등록
    InstigatorActor = NewInstigator;

    // 🛡️ 새로운 가해자(플레이어)에게 면책 특권 부여 (자폭 방지)
    if (InstigatorActor)
    {
        CollisionComp->IgnoreActorWhenMoving(InstigatorActor, true);

        TArray<AActor*> NewAttachedActors;
        InstigatorActor->GetAttachedActors(NewAttachedActors);
        for (AActor* AttachedActor : NewAttachedActors)
        {
            CollisionComp->IgnoreActorWhenMoving(AttachedActor, true);
        }
    }

    // 🚀 방향 반전 및 속도 뻥튀기
    if (ProjectileMovement)
    {
        FVector ReverseDir = ProjectileMovement->Velocity.GetSafeNormal() * -1.0f;
        ProjectileMovement->Velocity = ReverseDir * (ProjectileMovement->InitialSpeed * 1.5f); 
        SetActorRotation(ReverseDir.Rotation());
    }
}

void ASRProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 발사 즉시: 나를 쏜 사람과 그 사람의 무기는 통과하도록 설정 (총구 폭발 방지)
    if (InstigatorActor)
    {
        CollisionComp->IgnoreActorWhenMoving(InstigatorActor, true);

        TArray<AActor*> AttachedActors;
        InstigatorActor->GetAttachedActors(AttachedActors);
        for (AActor* AttachedActor : AttachedActors)
        {
            CollisionComp->IgnoreActorWhenMoving(AttachedActor, true);
        }
    }
}

void ASRProjectile::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 나 자신이나 나를 쏜 사람이 아닐 때만 판정
    if (OtherActor && OtherActor != this && OtherActor != InstigatorActor)
    {
        // 🛡️ 1. 충돌 필터링: 맞은 부위가 'Damageable'을 무시한다면? (예: 캡슐 콜리전)
        if (OtherComp && OtherComp->GetCollisionResponseToChannel(ECC_DAMAGEABLE) == ECR_Ignore)
        {
            // 단, 그 무시한 부위가 '벽(WorldStatic)'이나 '사물(WorldDynamic)'이 아니라면
            // 캐릭터의 캡슐이므로 그냥 통과(return)합니다! (벽에는 정상적으로 부딪혀 터짐)
            if (OtherComp->GetCollisionObjectType() != ECC_WorldStatic && OtherComp->GetCollisionObjectType() != ECC_WorldDynamic)
            {
                return; 
            }
        }

        // 💥 2. 물리 밀어내기 (래그돌이나 드럼통)
        if (OtherComp && OtherComp->IsSimulatingPhysics())
        {
            FVector ForceDirection = ProjectileMovement->Velocity.GetSafeNormal();
            FVector ImpactLoc = bFromSweep ? static_cast<FVector>(SweepResult.ImpactPoint) : GetActorLocation();
            OtherComp->AddImpulseAtLocation(ForceDirection * ImpactForce, ImpactLoc);
        }
        
        // 📡 3. 데미지 부여 (ASC 적용)
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
        
        if (TargetASC && DamageEffectClass)
        {
            FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
            
            // 패링 반사를 위해 가해자(Instigator)와 타격 매개체(this)를 정확히 넘겨줌
            ContextHandle.AddInstigator(InstigatorActor, this); 
            ContextHandle.AddHitResult(SweepResult);

            FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
            TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }

        // 💣 4. 캐릭터 살점이나 벽에 맞았으므로 투사체 폭발(파괴)
        Destroy();
    }
}