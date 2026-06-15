// Fill out your copyright notice in the Description page of Project Settings.

#include "SRProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h" 
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Perception/AISense_Hearing.h" 
#include "GameFramework/Actor.h" 
#include "Kismet/GameplayStatics.h"
#include "Data/SRWeaponDataAsset.h" // 🧱 무기 데이터 에셋 변수 참조를 위한 헤더 파일 추가

#define ECC_DAMAGEABLE ECC_GameTraceChannel4

ASRProjectile::ASRProjectile()
{
    HitEventTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.HitReact"));
    PrimaryActorTick.bCanEverTick = false;
    SetLifeSpan(6.0f);

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    
    CollisionComp->SetCollisionProfileName(TEXT("Custom"));
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); 
    CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore); 

    CollisionComp->SetCollisionResponseToChannel(ECC_DAMAGEABLE, ECR_Overlap);    
    CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);   
    CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);  
    CollisionComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);   

    CollisionComp->SetGenerateOverlapEvents(true);
    
    CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASRProjectile::OnProjectileOverlap); 
    CollisionComp->OnComponentHit.AddDynamic(this, &ASRProjectile::OnProjectileHit);             
    
    RootComponent = CollisionComp;

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
    ProjectileMesh->SetCollisionProfileName(TEXT("NoCollision"));
    ProjectileMesh->SetGenerateOverlapEvents(false);

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
        ProjectileMovement->Velocity = ShootDirection.GetSafeNormal() * InSpeed; 
    }
}

void ASRProjectile::DeflectProjectile(AActor* NewInstigator)
{
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

    InstigatorActor = NewInstigator;

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
    if (!OtherActor || OtherActor == this || OtherActor == InstigatorActor || OtherActor->GetOwner() == InstigatorActor)
    {
        return;
    }

    if (OtherComp && OtherComp->GetCollisionResponseToChannel(ECC_DAMAGEABLE) == ECR_Ignore)
    {
        ECollisionChannel ObjType = OtherComp->GetCollisionObjectType();
        if (ObjType != ECC_WorldStatic && ObjType != ECC_WorldDynamic && ObjType != ECC_PhysicsBody)
        {
            return; 
        }
    }
    
    FVector ImpactLoc = bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
    FRotator ImpactRot = bFromSweep ? SweepResult.ImpactNormal.Rotation() : (ProjectileMovement->Velocity.GetSafeNormal() * -1.0f).Rotation();

    // 🔊 충돌 지점에서 실제 오디오 사운드 재생 (다중 랜덤 풀 정산)
    if (ImpactSounds.Num() > 0)
    {
        int32 RandomIndex = FMath::RandRange(0, ImpactSounds.Num() - 1);
        if (ImpactSounds[RandomIndex])
        {
            UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSounds[RandomIndex], ImpactLoc);
        }
    }

    // AI 청각 리포터
    UAISense_Hearing::ReportNoiseEvent(
        GetWorld(), 
        ImpactLoc, 
        0.8f, 
        InstigatorActor ? InstigatorActor : this, 
        1200.0f, 
        TEXT("BulletImpact")
    );

    if (WallImpactFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), WallImpactFX, ImpactLoc, ImpactRot);
    }

    // 🧱 오버랩 대상이 환경 지형(Static/Dynamic)일 경우 노말 방향 스폰
    if (OtherComp)
    {
        ECollisionChannel ObjType = OtherComp->GetCollisionObjectType();
        if (ObjType == ECC_WorldStatic || ObjType == ECC_WorldDynamic)
        {
            if (EnvironmentImpactVFX)
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), EnvironmentImpactVFX, ImpactLoc, ImpactRot);
            }
        }
    }

    // =======================================================================
    // 💥 [데이터 에셋 연동] 오버랩 타격 시 무기 데이터의 ImpactForce 수치로 밀어내기
    // =======================================================================
    if (OtherComp && OtherComp->IsSimulatingPhysics())
    {
        FVector ForceDirection = ProjectileMovement->Velocity.GetSafeNormal();
        
        // 데이터 에셋이 존재하면 세팅된 수치를 적용하고, 없으면 생성자 기본 수치(ExposeOnSpawn)로 가드
        float FinalForce = (SourceWeaponData) ? SourceWeaponData->ImpactForce : ImpactForce;
        OtherComp->AddImpulseAtLocation(ForceDirection * FinalForce, ImpactLoc);
    }
    // =======================================================================
    
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
    if (TargetASC && DamageEffectClass)
    {
        FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
        ContextHandle.AddInstigator(InstigatorActor, this); 
        ContextHandle.AddHitResult(SweepResult);

        FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
        TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }

    Destroy();
}

void ASRProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!OtherActor || OtherActor == this || OtherActor == InstigatorActor || OtherActor->GetOwner() == InstigatorActor)
    {
        return;
    }

    FVector ImpactLoc = Hit.ImpactPoint;
    FRotator ImpactRot = Hit.ImpactNormal.Rotation(); 

    // 🔊 충돌 지점에서 실제 오디오 사운드 재생 (다중 랜덤 풀 정산)
    if (ImpactSounds.Num() > 0)
    {
        int32 RandomIndex = FMath::RandRange(0, ImpactSounds.Num() - 1);
        if (ImpactSounds[RandomIndex])
        {
            UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSounds[RandomIndex], ImpactLoc);
        }
    }

    // [1] AI 청각 소음 발생
    UAISense_Hearing::ReportNoiseEvent(GetWorld(), ImpactLoc, 1.0f, InstigatorActor ? InstigatorActor : this, 1500.0f, TEXT("BulletImpact"));

    // [2] 기본 공용 이펙트
    if (WallImpactFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), WallImpactFX, ImpactLoc, ImpactRot);
    }

    // 🧱 환경 지형 필터링 이펙트
    if (OtherComp)
    {
        ECollisionChannel ObjType = OtherComp->GetCollisionObjectType();
        if (ObjType == ECC_WorldStatic || ObjType == ECC_WorldDynamic)
        {
            if (EnvironmentImpactVFX)
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), EnvironmentImpactVFX, ImpactLoc, ImpactRot);
            }
        }
    }

    // =======================================================================
    // 💥 [데이터 에셋 연동] 블로킹 히트 시 무기 데이터의 ImpactForce 수치로 밀어내기
    // =======================================================================
    if (OtherComp && OtherComp->IsSimulatingPhysics())
    {
        FVector ForceDirection = ProjectileMovement->Velocity.GetSafeNormal();
        
        float FinalForce = (SourceWeaponData) ? SourceWeaponData->ImpactForce : ImpactForce;
        OtherComp->AddImpulseAtLocation(ForceDirection * FinalForce, ImpactLoc);
    }
    // =======================================================================

    Destroy();
}