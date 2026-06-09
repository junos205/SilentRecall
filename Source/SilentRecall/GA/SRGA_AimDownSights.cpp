#include "GA/SRGA_AimDownSights.h"
#include "AbilitySystemComponent.h"
#include "Character/SRInventoryComponent.h" // 🌟 추가
#include "Weapon/SRWeaponInstance.h"       // 🌟 추가
#include "Data/SRWeaponDataAsset.h"         // 🌟 추가

USRGA_AimDownSights::USRGA_AimDownSights()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTag AimAbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Action.AimDownSights"));
    SetAssetTags(FGameplayTagContainer(AimAbilityTag));

    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Aiming")));

    // 대시, 파쿠르, 벽타기 시 자동 캔슬 가드 유지
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.Stun")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Dash")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Movement.WallRunning")));

    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Debuff.HitReact")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Vaulting")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Action.Dash")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Character.State.Movement.WallRunning")));
}

// =======================================================================
// 🧱 [원거리 전용 무결성 검문소]
// 버튼을 아무리 길게 눌러도 이 필터를 통과하지 못하면 어빌리티 자체가 아예 켜지지 않습니다.
// =======================================================================
bool USRGA_AimDownSights::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;

    AActor* Avatar = ActorInfo->AvatarActor.Get();
    if (!Avatar) return false;

    // 플레이어의 인벤토리 컴포넌트 장부 수색
    USRInventoryComponent* InvComp = Avatar->FindComponentByClass<USRInventoryComponent>();
    if (!InvComp) return false;

    // 현재 손에 들고 있는 실시간 무기 인스턴스 장부 획득
    USRWeaponInstance* ActiveWeapon = InvComp->GetCurrentActiveWeaponInstance();
    if (!ActiveWeapon || !ActiveWeapon->WeaponData) return false;

    // 🚨 [결정타] 최대 장탄수가 0 이하다? 무조건 근접 무기(칼/맨손)이므로 조준 능력 실행을 칼같이 거부(False)한다!
    if (ActiveWeapon->WeaponData->MaxAmmoInMag <= 0)
    {
        return false;
    }

    return true;
}

void USRGA_AimDownSights::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    UE_LOG(LogTemp, Log, TEXT("[ADS 어빌리티] 🎯 원거리 무기 검증 통과! 조준 상태 돌입."));
}

void USRGA_AimDownSights::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    UE_LOG(LogTemp, Log, TEXT("[ADS 어빌리티] ❌ 조준 해제."));
}