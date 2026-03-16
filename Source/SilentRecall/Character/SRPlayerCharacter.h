// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SRBaseCharacter.h"
#include "SRPlayerCharacter.generated.h"

UCLASS()
class SILENTRECALL_API ASRPlayerCharacter : public ASRBaseCharacter
{
	GENERATED_BODY()

public:
	ASRPlayerCharacter(const FObjectInitializer& ObjectInitializer);
	
	// 매 프레임 속도를 체크하기 위해 Tick 함수 오버라이드
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<class UInputMappingContext> InputMappingContext;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCameraComponent> Camera;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = InputAbilities)
	TMap<EInputAction, TSubclassOf<UGameplayAbility>> InputAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TObjectPtr<class UInputAction> DashAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TObjectPtr<class UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> LookAction;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	virtual void Jump() override;

public:
	virtual void PossessedBy(AController* NewController) override;

	// GAS관련 입력 바인딩 함수
	void SetupGASInputComponent();

	// GAS관련 입력 핸들 함수
	void GASInputPressed(int32 InputId);
	void GASInputReleased(int32 InputId);

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// 스피드 라인 VFX 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX", meta = (AllowPrivateAccess = "true"))
	class UNiagaraComponent* SpeedLinesVFX;

	// FOV 및 속도 연출 설정값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float SpeedVFXThreshold = 800.0f; // 이 속도를 넘으면 연출 시작

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float BaseFOV = 90.0f; // 기본 시야각

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float SprintFOV = 115.0f; // 질주 시 시야각 (넓어질수록 속도감 극대화)

};
