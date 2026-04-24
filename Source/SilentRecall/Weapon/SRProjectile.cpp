#include "SRProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h" // ⭐️ 메쉬 컴포넌트 헤더 추가
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

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
    
    // ⭐️ [핵심] 기존의 "Projectile" 프리셋을 버리고 "Custom"으로 선언합니다!
    CollisionComp->SetCollisionProfileName(TEXT("Custom"));
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly); 
    
    // 1단계: 일단 세상 모든 물체(카메라, 내 캡슐, 내 무기 등)를 다 통과하게 만듭니다.
    CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore); 

    // 2단계: 내가 때려야 할 진짜 목표물들만 겹침(Overlap)으로 열어줍니다.
    CollisionComp->SetCollisionResponseToChannel(ECC_DAMAGEABLE, ECR_Overlap);  // 적 캐릭터 살점
    CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);   // 콘크리트 벽, 바닥
    CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);  // 움직이는 상자, 문
    CollisionComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);   // 래그돌 상태의 시체

    CollisionComp->SetGenerateOverlapEvents(true);
    CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASRProjectile::OnProjectileOverlap);
    RootComponent = CollisionComp;

    // ==========================================================
    // 🎨 2. 껍데기 메쉬 (이 녀석은 진짜 유령이어야 합니다)
    // ==========================================================
    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
    ProjectileMesh->SetCollisionProfileName(TEXT("NoCollision"));
    ProjectileMesh->SetGenerateOverlapEvents(false);

    // ==========================================================
    // 🚀 3. 발사체 무브먼트 (총구 시차 보정용 세팅)
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
        
        // ⭐️ 몸통 방향이 아니라, GA가 계산해준 '진짜 타격 방향'으로 속도를 곱합니다!
        ProjectileMovement->Velocity = ShootDirection.GetSafeNormal() * InSpeed; 
    }
}

void ASRProjectile::DeflectProjectile(AActor* NewInstigator)
{
    // ==========================================================
    // 🛡️ 1. 기존 가해자(적)의 '면책 특권' 박탈! (이제 적을 때릴 수 있음)
    // ==========================================================
    if (InstigatorActor)
    {
        CollisionComp->IgnoreActorWhenMoving(InstigatorActor, false);

        // 기존 가해자의 무기에 대한 무시 판정도 해제
        TArray<AActor*> OldAttachedActors;
        InstigatorActor->GetAttachedActors(OldAttachedActors);
        for (AActor* AttachedActor : OldAttachedActors)
        {
            CollisionComp->IgnoreActorWhenMoving(AttachedActor, false);
        }
    }

    // ==========================================================
    // 👑 2. 새로운 가해자(패링한 플레이어) 등록
    // ==========================================================
    InstigatorActor = NewInstigator;

    // ==========================================================
    // 🛡️ 3. 새로운 가해자(플레이어)에게 '면책 특권' 부여! (자폭 방지)
    // ==========================================================
    if (InstigatorActor)
    {
        CollisionComp->IgnoreActorWhenMoving(InstigatorActor, true);

        // 플레이어의 무기(칼 등)에 닿아서 바로 터지는 것도 방지
        TArray<AActor*> NewAttachedActors;
        InstigatorActor->GetAttachedActors(NewAttachedActors);
        for (AActor* AttachedActor : NewAttachedActors)
        {
            CollisionComp->IgnoreActorWhenMoving(AttachedActor, true);
        }
    }

    // ==========================================================
    // 🚀 4. 방향 반전 및 속도 뻥튀기 (기존 로직 유지)
    // ==========================================================
    if (ProjectileMovement)
    {
        FVector ReverseDir = ProjectileMovement->Velocity.GetSafeNormal() * -1.0f;
        
        // 튕겨나갈 땐 더 빠르고 강하게!
        ProjectileMovement->Velocity = ReverseDir * (ProjectileMovement->InitialSpeed * 1.5f); 
        SetActorRotation(ReverseDir.Rotation());
    }
}

void ASRProjectile::BeginPlay()
{
    Super::BeginPlay();

    // ⭐️ [해결책] 총알의 주인(InstigatorActor)이 설정되어 있다면?
    if (InstigatorActor)
    {
        // 1. 나를 쏜 사람(플레이어)의 몸(Capsule/Mesh)을 절대 때리지 말고 통과해라!
        CollisionComp->IgnoreActorWhenMoving(InstigatorActor, true);

        // 2. 나를 쏜 사람의 손에 들려있는 '무기'도 무시해라! (총구에서 터지는 버그 방지)
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
    UE_LOG(LogTemp, Warning, TEXT("[SRProjectile] Overlapped with Actor: %s / Component: %s"), *OtherActor->GetName(), *OtherComp->GetName());
    if (OtherActor && OtherActor != this && OtherActor != InstigatorActor)
    {
        // 🛡️ 1. 충돌 필터링
        if (OtherComp && OtherComp->GetCollisionResponseToChannel(ECC_DAMAGEABLE) == ECR_Ignore)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SRProjectile] Ignored %s's Component: %s"), *OtherActor->GetName(), *OtherComp->GetName());
            if (OtherComp->GetCollisionObjectType() != ECC_WorldStatic && OtherComp->GetCollisionObjectType() != ECC_WorldDynamic)
            {
                return; // 캡슐 통과
            }
        }

        // 💥 2. 래그돌 물리 밀어내기
        if (OtherComp && OtherComp->IsSimulatingPhysics())
        {
            FVector ForceDirection = ProjectileMovement->Velocity.GetSafeNormal();
            FVector ImpactLoc = bFromSweep ? static_cast<FVector>(SweepResult.ImpactPoint) : GetActorLocation();
            OtherComp->AddImpulseAtLocation(ForceDirection * ImpactForce, ImpactLoc);
        }
        
        // ==========================================================
        // 📡 3. [완전 변경] 플레이어에게 보고하지 말고 직접 데미지를 꽂아라!
        // ==========================================================
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
        
        // 타겟에게 ASC가 있고, 우리가 터뜨릴 데미지 이펙트가 있다면?
        if (TargetASC && DamageEffectClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SRProjectile] Applying damage to %s"), *OtherActor->GetName());
            
            // 이펙트 주머니 만들기
            FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
            
            // ⭐️ [진짜 중요] 가해자는 플레이어(InstigatorActor), 때린 물건은 총알(this)!!
            // 이렇게 넘겨줘야 유저님이 만든 ExecCalc에서 GetEffectCauser()를 불렀을 때 이 투사체가 튀어나와서 패링 반사가 가능해집니다!
            ContextHandle.AddInstigator(InstigatorActor, this); 
            ContextHandle.AddHitResult(SweepResult);

            // 데미지 스펙 만들어서 꽂아버리기!
            FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
            TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }

        // 💣 4. 데미지 줬으니 무조건 폭발!
        UE_LOG(LogTemp, Warning, TEXT("[SRProjectile] Destroying projectile after hitting %s"), *OtherActor->GetName());
        Destroy();
    }
}