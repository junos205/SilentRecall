#include "SRDamageExecCalc.h"
#include "AttributeSet/SRDefaultAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "SRDefaultAttributeSet.h"

struct FSRDamageStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(AttackRate); // 잡고 싶은 속성 이름

    FSRDamageStatics()
    {
        // Source(공격자)의 USRDefaultAttributeSet에 있는 AttackRate를 캡처하겠다! (마지막 false는 스냅샷 여부)
        DEFINE_ATTRIBUTE_CAPTUREDEF(USRDefaultAttributeSet, AttackRate, Source, false);
    }
};

// 위 구조체를 싱글톤처럼 빠르게 불러오는 헬퍼 함수
static const FSRDamageStatics& DamageStatics()
{
    static FSRDamageStatics Statics;
    return Statics;
}

// ⭐️ 2. 생성자에서 "나는 이 속성을 캡처할 거야!" 라고 엔진에 등록
USRDamageExecCalc::USRDamageExecCalc()
{
    RelevantAttributesToCapture.Add(DamageStatics().AttackRateDef);
}

void USRDamageExecCalc::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    // 1. 공격자(Source)와 방어자(Target)의 ASC 및 액터 가져오기
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
    UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();

    if (!TargetASC || !SourceASC) return;

    AActor* TargetActor = TargetASC->GetAvatarActor();
    AActor* SourceActor = SourceASC->GetAvatarActor();

    // 2. 공격 GA에서 보낸 기본 데미지 값 가져오기 (예: SetByCaller를 사용한다고 가정)
    // 공격력이나 방어력을 수식에 섞고 싶다면 여기서 계산하면 됩니다.
    float BaseDamage = ExecutionParams.GetOwningSpec().GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), false, 10.0f);
    float FinalDamage = BaseDamage;

    // ⭐️ 3. [패링 판정] 타겟이 현재 "패링 버튼을 눌러서 방어 중"인지 태그 확인!
    if (TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Parry.Active"))))
    {
        if (TargetActor && SourceActor)
        {
            // ⭐️ 4. 내적(Dot)을 이용한 45도 전방 방어 판정
            FVector TargetForward = TargetActor->GetActorForwardVector().GetSafeNormal2D();
            FVector DirToSource = (SourceActor->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal2D();
            
            float Dot = FVector::DotProduct(TargetForward, DirToSource);

            // 내적 값이 0.707 이상이면 정면 45도 범위 내에서 공격이 온 것! (패링 성공)
            if (Dot >= 0.707f)
            {
                FinalDamage = 0.0f; // 데미지 무효화!

                FGameplayEventData Payload;
                Payload.Instigator = SourceActor; // 때린 놈 (적 캐릭터)
                Payload.Target = TargetActor;     // 맞은 놈 (나)
                
                // ⭐️ [핵심 추가] 날아온 진짜 물체(투사체 액터)를 OptionalObject에 담아서 보냅니다!
                // 근접 공격이면 무기나 적 캐릭터 자체가 담기고, 원거리면 투사체 액터가 담깁니다.
                Payload.OptionalObject = ExecutionParams.GetOwningSpec().GetContext().GetEffectCauser(); 

                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
                    TargetActor, 
                    FGameplayTag::RequestGameplayTag(FName("Event.Character.ParrySuccess")), 
                    Payload
                );
            }
        }
    }

    // 5. 최종 확정된 데미지(패링 실패 시 원래 데미지, 성공 시 0)를 방어자의 Damage 어트리뷰트에 꽂아 넣습니다!
    if (FinalDamage > 0.0f)
    {
        OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
            USRDefaultAttributeSet::GetDamageAttribute(), // 적용할 속성 (유저님의 Damage 속성)
            EGameplayModOp::Additive,                     // 더하기
            FinalDamage                                   // 최종 데미지 값
        ));
    }
}