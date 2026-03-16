#pragma once

#include "CoreMinimal.h"
#include "SRCharacterData.generated.h"

UENUM(BlueprintType)
enum class EInputAction : uint8
{
	Sprint,
	Dash,
	Slide
};