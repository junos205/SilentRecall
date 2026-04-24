#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "SRAN_ProcceedCombo.generated.h"

UCLASS()
class SILENTRECALL_API USRAN_ProceedCombo : public UAnimNotify
{
	GENERATED_BODY()
    
public:
	USRAN_ProceedCombo();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// GA로 보낼 이벤트 태그 ("Character.Event.CheckCombo")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTag ComboCheckTag;
};