// Fill out your copyright notice in the Description page of Project Settings.

#include "Gimmick/SRKillVolume.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"

ASRKillVolume::ASRKillVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
    
	// 일반 트리거 프로필로 설정하여 캐릭터 오버랩을 감지합니다.
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void ASRKillVolume::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASRKillVolume::OnOverlapBegin);
}

void ASRKillVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor) return;

	// 1. 닿은 대상(플레이어 혹은 적)의 AbilitySystemComponent를 추출합니다.
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (TargetASC)
	{
		// 🔒 [중복 처리 가드] 이미 죽어서 사망 태그를 달고 있는 시체라면 연산을 무시합니다.
		FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("Character.State.IsDead"));
		if (TargetASC->HasMatchingGameplayTag(DeadTag)) return;

		// 2. 택배 상자(Payload) 조립
		FGameplayEventData Payload;
		Payload.Instigator = this; // 가해자를 이 킬 볼륨으로 지정
		Payload.Target = OtherActor;

		// 3. 🌟 [GAS 치트키] 대상에게 사망 이벤트 태그를 다이렉트로 무전 격발합니다!
		// 어트리뷰트 세트에서 체력이 0이 되었을 때와 똑같은 무전이 가므로, 기존 시스템과 완벽히 융합됩니다.
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			OtherActor, 
			FGameplayTag::RequestGameplayTag(FName("Character.Event.Death")), 
			Payload
		);
        
		UE_LOG(LogTemp, Warning, TEXT("[KillVolume 💀] %s 캐릭터가 즉사 구역에 떨어져 사망 처리되었습니다."), *OtherActor->GetName());
	}
}