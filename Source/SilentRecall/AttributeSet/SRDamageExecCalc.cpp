#include "SRDamageExecCalc.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/SRInventoryComponent.h"
#include "Weapon/SRWeaponInstance.h"
#include "Data/SRWeaponDataAsset.h"
#include "Weapon/SRProjectile.h"

struct FSRDamageStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(AttackRate);

    FSRDamageStatics()
    {
        DEFINE_ATTRIBUTE_CAPTUREDEF(USRDefaultAttributeSet, AttackRate, Source, false);
    }
};

static const FSRDamageStatics& DamageStatics()
{
    static FSRDamageStatics Statics;
    return Statics;
}

USRDamageExecCalc::USRDamageExecCalc()
{
    RelevantAttributesToCapture.Add(DamageStatics().AttackRateDef);
}

void USRDamageExecCalc::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
    UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();

    if (!TargetASC || !SourceASC) return;

    AActor* TargetActor = TargetASC->GetAvatarActor();
    AActor* SourceActor = SourceASC->GetAvatarActor();

    // ==========================================================
    // 🛡️ 0순위: 패링 성공 후 무적(Invincible) 태그 상태라면 데미지 완전 면제
    // ==========================================================
    FGameplayTag InvincibleTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Invincible"));
    if (TargetASC->HasMatchingGameplayTag(InvincibleTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[DamageCalc] 방어자가 현재 패링 무적 상태입니다. 데미지를 무력화합니다."));
        return; 
    }

    // ==========================================================
    // ⚔️ 스탯 및 무기 데이터 동적 추출 단계
    // ==========================================================
    float AttackRate = 0.0f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackRateDef, FAggregatorEvaluateParameters(), AttackRate);

    float WeaponBaseDamage = 10.0f; 
    EWeaponDamageMode CurrentDamageMode = EWeaponDamageMode::Absolute; 
    float ScalingFactor = 1.0f;
    bool bWeaponDataFound = false;

    AActor* EffectCauser = ExecutionParams.GetOwningSpec().GetContext().GetEffectCauser();
    
    // 투사체 여부 확인 (원거리 판정용 변수)
    bool bIsProjectile = false; 

    if (ASRProjectile* Projectile = Cast<ASRProjectile>(EffectCauser))
    {
        bIsProjectile = true;
        if (Projectile->SourceWeaponData)
        {
            WeaponBaseDamage = Projectile->SourceWeaponData->BaseDamage;
            CurrentDamageMode = Projectile->SourceWeaponData->DamageMode;
            ScalingFactor = Projectile->SourceWeaponData->StatScalingFactor;
            bWeaponDataFound = true;
        }
    }

    if (!bWeaponDataFound && SourceActor)
    {
        if (USRInventoryComponent* InvComp = SourceActor->FindComponentByClass<USRInventoryComponent>())
        {
            if (USRWeaponInstance* WeaponInst = InvComp->GetCurrentActiveWeaponInstance())
            {
                if (WeaponInst->WeaponData)
                {
                    WeaponBaseDamage = WeaponInst->WeaponData->BaseDamage;
                    CurrentDamageMode = WeaponInst->WeaponData->DamageMode;
                    ScalingFactor = WeaponInst->WeaponData->StatScalingFactor;
                    bWeaponDataFound = true;
                }
            }
        }
    }

    // 데미지 계산 공식 적용
    float CalculatedDamage = WeaponBaseDamage;
    switch (CurrentDamageMode)
    {
    case EWeaponDamageMode::Absolute: CalculatedDamage = WeaponBaseDamage; break;
    case EWeaponDamageMode::Additive: CalculatedDamage = WeaponBaseDamage + (AttackRate * ScalingFactor); break;
    case EWeaponDamageMode::Multiplicative: CalculatedDamage = WeaponBaseDamage * (1.0f + ((AttackRate * ScalingFactor) / 100.0f)); break;
    }

    float FinalDamage = FMath::Max(CalculatedDamage, 0.0f);

    // ==========================================================
    // 🏓 1순위: 패링 판정 가동 (플레이어 시선 기반 내적 연산)
    // ==========================================================
    FGameplayTag ParryActiveTag = FGameplayTag::RequestGameplayTag(FName("Character.State.Parry.Active"));
    if (TargetASC->HasMatchingGameplayTag(ParryActiveTag))
    {
        if (TargetActor && SourceActor)
        {
            // ⭐️ [유저님 요청 반영] 공격받은 위치 오차를 배제하고, 플레이어의 순수 회전 방향전방(Forward)과 
            // 플레이어의 중심점에서 공격자(Source)를 똑바로 바라보는 방향(DirToSource)만 비교합니다.
            FVector TargetForward = TargetActor->GetActorForwardVector().GetSafeNormal2D();
            FVector DirToSource = (SourceActor->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal2D();
            
            float Dot = FVector::DotProduct(TargetForward, DirToSource);

            // 약 전방 90도 범위 콘 세팅 (Dot >= 0.707f)
            if (Dot >= 0.707f)
            {
                FinalDamage = 0.0f; // 패링 성공했으므로 들어오려던 데미지 즉시 증발

                FGameplayEventData Payload;
                Payload.Instigator = SourceActor;
                Payload.Target = TargetActor;
                Payload.OptionalObject = EffectCauser; 

                // 원거리 공격 판정 규칙 (투사체 액터이거나, 이펙트 사유 태그에 Ranged가 포함되어 있다면 원거리로 간주)
                bool bIsRangedAttack = bIsProjectile || ExecutionParams.GetOwningSpec().CapturedTargetTags.GetActorTags().HasTag(FGameplayTag::RequestGameplayTag(FName("Attack.Type.Ranged")));

                // ⭐️ [세분화 무전] 근접이냐 원거리냐에 따라 어빌리티 시스템에 다른 이벤트 태그를 전파합니다!
                FGameplayTag SuccessEventTag;
                if (bIsRangedAttack)
                {
                    SuccessEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Character.ParrySuccess.Ranged"));
                    UE_LOG(LogTemp, Warning, TEXT("[DamageCalc] 🏓 원거리 패링 성공! (Event.Character.ParrySuccess.Ranged)"));
                }
                else
                {
                    SuccessEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Character.ParrySuccess.Melee"));
                    UE_LOG(LogTemp, Warning, TEXT("[DamageCalc] ⚔️ 근접 패링 성공! (Event.Character.ParrySuccess.Melee)"));
                }

                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, SuccessEventTag, Payload);
            }
        }
    }

    // 최종 데미지 적용
    if (FinalDamage > 0.0f)
    {
        OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
            USRDefaultAttributeSet::GetDamageAttribute(),
            EGameplayModOp::Additive,
            FinalDamage
        ));
    }
}