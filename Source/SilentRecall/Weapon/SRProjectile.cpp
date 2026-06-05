// Fill out your copyright notice in the Description page of Project Settings.

#include "SRProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h" 
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameFramework/Actor.h" // 안전장치 헤더

// 내 전용 타격 채널 (Damageable) 정의
#define ECC_DAMAGEABLE ECC_GameTraceChannel4

ASRProjectile::ASRProjectile()
{
    HitEventTag = FGameplayTag::RequestGameplayTag(FName("Character.Event.HitReact"));
    PrimaryActorTick.bCanEverTick = false;

    // 🌟 [추가] 3. 투사체의 생애주기를 6초로 설정 (6초 뒤 자동으로 Destroy 호출됨)
    SetLifeSpan(6.0f);

    // ==========================================================
    // 🛡️ 1. 콜리전 (투명 구형 충돌체) 정밀 세팅
    // ==========================================================
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    
    CollisionComp->SetCollisionProfileName(TEXT("Custom"));
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); 
    CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore); 

    // 부딪혀서 터져야 할 채널들 Open
    CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore); 

    // 1. 캐릭터 살점은 정밀 판정(GAS)을 위해 Overlap 유지
    CollisionComp->SetCollisionResponseToChannel(ECC_DAMAGEABLE, ECR_Overlap);    

    // 🌟 2. 지형지물 및 사물은 확실하게 Block으로 변경 (무브먼트 멈춤과 동기화)
    CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);   
    CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);  
    CollisionComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);   

    CollisionComp->SetGenerateOverlapEvents(true);
    
    // 🌟 3. 두 가지 이벤트를 모두 바인딩합니다.
    CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASRProjectile::OnProjectileOverlap); // 캐릭터용
    CollisionComp->OnComponentHit.AddDynamic(this, &ASRProjectile::OnProjectileHit);             // 벽/지형지물용
    
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
    // 1. 기본 필터링: 나 자신, 이그노어 대상 제외
    if (!OtherActor || OtherActor == this || OtherActor == InstigatorActor || OtherActor->GetOwner() == InstigatorActor)
    {
        return;
    }

    // 2. 캐릭터 캡슐 콜리전 예외 처리 탈출선
    // 맞은 컴포넌트가 살점(DAMAGEABLE)을 무시하는 존재(예: 캐릭터 캡슐)일 때
    if (OtherComp && OtherComp->GetCollisionResponseToChannel(ECC_DAMAGEABLE) == ECR_Ignore)
    {
        ECollisionChannel ObjType = OtherComp->GetCollisionObjectType();
        // 그 존재가 벽(Static), 사물(Dynamic), 물리 바디가 아니라면 완전히 통과시킵니다.
        if (ObjType != ECC_WorldStatic && ObjType != ECC_WorldDynamic && ObjType != ECC_PhysicsBody)
        {
            return; 
        }
    }

    // =======================================================================
    // 💥 [여기서부터는 무조건 충돌 판정 완료 구역] (벽 또는 적 메시)
    // =======================================================================
    
    // 충돌 위치 및 이펙트 회전각 산출 (Sweep 결과가 없으면 투사체 현재 위치 기준 계산)
    // SweepResult.ImpactPoint를 FVector로 감싸서 타입을 일치시켜 줍니다.
    FVector ImpactLoc = bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
    FRotator ImpactRot = bFromSweep ? SweepResult.ImpactNormal.Rotation() : (ProjectileMovement->Velocity.GetSafeNormal() * -1.0f).Rotation();

    // 🌟 [추가] 벽이든 적이든 무언가에 가로막혔으므로 나이아가라 폭발 이펙트 재생
    if (WallImpactFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), WallImpactFX, ImpactLoc, ImpactRot);
    }

    // 물리 밀어내기 (래그돌이나 드럼통)
    if (OtherComp && OtherComp->IsSimulatingPhysics())
    {
        FVector ForceDirection = ProjectileMovement->Velocity.GetSafeNormal();
        OtherComp->AddImpulseAtLocation(ForceDirection * ImpactForce, ImpactLoc);
    }
    
    // 데미지 부여 (상대에게 ASC가 존재할 때만 작동하므로 벽에 충돌 시 자동 스킵됨)
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
    if (TargetASC && DamageEffectClass)
    {
        FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
        ContextHandle.AddInstigator(InstigatorActor, this); 
        ContextHandle.AddHitResult(SweepResult);

        FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
        TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }

    // 🌟 [수정] 부딪혔으므로 투사체 무조건 삭제 (벽 충돌 종결)
    Destroy();
}

void ASRProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // 나 자신, 나를 쏜 사람, 나를 쏜 사람의 무기는 무시
    if (!OtherActor || OtherActor == this || OtherActor == InstigatorActor || OtherActor->GetOwner() == InstigatorActor)
    {
        return;
    }

    // 💥 [벽 충돌 종결 구역]
    // 충돌 지점 및 법선 각도 추출
    FVector ImpactLoc = Hit.ImpactPoint;
    FRotator ImpactRot = Hit.ImpactNormal.Rotation();

    // 나이아가라 이펙트 재생
    if (WallImpactFX)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), WallImpactFX, ImpactLoc, ImpactRot);
    }

    // 물리 오브젝트 밀어내기 (드럼통 등)
    if (OtherComp && OtherComp->IsSimulatingPhysics())
    {
        FVector ForceDirection = ProjectileMovement->Velocity.GetSafeNormal();
        OtherComp->AddImpulseAtLocation(ForceDirection * ImpactForce, ImpactLoc);
    }

    // 💣 벽에 부딪힌 순간 단 1프레임도 가만히 있지 않고 즉시 파괴
    Destroy();
}