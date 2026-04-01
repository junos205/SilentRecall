// ItemStateInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemStateInterface.generated.h"

UINTERFACE(MinimalAPI)
class UItemStateInterface : public UInterface { GENERATED_BODY() };

class SILENTRECALL_API IItemStateInterface
{
	GENERATED_BODY()

public:
	// ⭐️ 바닥에 버려질 때, 가방이 이 함수를 호출해서 데이터를 넘겨줄 겁니다!
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item State")
	void SetDroppedAmmo(int32 AmmoAmount);
};