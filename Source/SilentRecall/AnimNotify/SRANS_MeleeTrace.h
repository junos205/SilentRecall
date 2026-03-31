// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimNotify_GameplayCue.h"
#include "SRANS_MeleeTrace.generated.h"

/**
 * 
 */
UCLASS()
class SILENTRECALL_API USRANS_MeleeTrace : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	USRANS_MeleeTrace();

	// ⭐️ 몽타주에서 설정할 변수들 (블루프린트 디테일 패널에서 수정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Info")
	FName BaseSocketName = FName("Trace_Start_Socket"); // 손잡이 소켓 이름

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Info")
	FName TipSocketName = FName("Trace_End_Socket");   // 칼끝 소켓 이름

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Info")
	float TraceRadius = 20.0f; // 트레이스 두께

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace Info")
	FGameplayTag HitEventTag; // 타격 성공 시 날릴 무전기 암호! (예: Event.Melee.Hit)

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	// ⭐️ [매우 중요] 한 번 휘두를 때 이미 맞은 녀석들을 기억해둘 '블랙리스트'
	UPROPERTY()
	TArray<AActor*> AlreadyHitActors;
};
