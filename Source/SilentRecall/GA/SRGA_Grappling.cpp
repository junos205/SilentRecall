#include "GA/SRGA_Grappling.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "DrawDebugHelpers.h"
#include "Character/SRPlayerCharacter.h" // ⭐️ 캐릭터 클래스 헤더 필수!

USRGA_Grappling::USRGA_Grappling()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void USRGA_Grappling::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 1. 코스트(마나/스태미나) 및 쿨다운 검사 통과 여부 확인
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 2. 캐릭터 정보 가져오기
	ASRPlayerCharacter* SRCharacter = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!SRCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. 레이저(Line Trace) 발사 준비
	FVector StartLocation;
	FRotator ViewRotation;
	SRCharacter->GetActorEyesViewPoint(StartLocation, ViewRotation); // 카메라 시점 기준
	FVector EndLocation = StartLocation + (ViewRotation.Vector() * GrappleRange);
	FVector ViewDir = ViewRotation.Vector().GetSafeNormal();

	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(SRCharacter);
	
	// 4. 레이저 발사!
	// ⭐️ 빔의 두께(반경) 설정: 50.0f면 지름 1m짜리 두꺼운 통나무가 날아가는 셈입니다.
	// 플레이스타일에 맞게 이 반경을 늘리거나 줄여서 난이도를 조절하세요!
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(250.0f);

	

	// 4. 두꺼운 구체 스윕 발사!
	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults, 
		StartLocation, 
		EndLocation, 
		FQuat::Identity, // 구체는 회전이 필요 없으므로 Identity가 완벽함
		ECC_GameTraceChannel2, 
		SphereShape, 
		QueryParams
	);
	
	if (bDrawDebug)
	{
		// 1. 발사한 두꺼운 '빔' 자체를 그립니다. (캡슐 빔 형태)
		// 구체가 A에서 B로 날아간 궤적을 보여줍니다.
		FColor BeamColor = bHit ? FColor::Green : FColor::Red; // 맞았으면 초록, 아니면 빨강

		DrawDebugCapsule(
			GetWorld(),
			(StartLocation + EndLocation) * 0.5f, // 중심점
			FVector::Distance(StartLocation, EndLocation) * 0.5f + SphereShape.GetSphereRadius(), // 반 반경
			SphereShape.GetSphereRadius(), // 빔 두께
			FRotationMatrix::MakeFromZ(EndLocation - StartLocation).ToQuat(), // 빔 방향
			BeamColor,
			false, // 무한 지속 아님
			3.0f, // 3초 동안 보여줌
			0, // 깊이 우선순위
			2.0f // 두께
		);

		// 2. 만약 무언가에 맞았다면, 그 부딪힌 지점(Impact Point)에 구체를 그립니다.
		if (bHit)
		{
			AActor* BestTarget = nullptr;
			FVector BestImpactPoint = FVector::ZeroVector;
			float BestDotProduct = -1.0f; // 가장 높은 내적값을 저장할 변수

			// 4. 빔에 잡힌 모든 오브젝트를 하나씩 검사합니다.
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();
				if (HitActor && HitActor->ActorHasTag(FName("GrappleTarget")))
				{
					// [나 -> 타겟] 방향 벡터 계산
					FVector DirToTarget = (Hit.ImpactPoint - StartLocation).GetSafeNormal();
                
					// 내 시선과 타겟 방향의 내적 계산
					float DotProduct = FVector::DotProduct(ViewDir, DirToTarget);

					// ⭐️ 내적값이 0.5 (약 60도, 내 시야 앞쪽) 이상이고, 
					// 지금까지 찾은 타겟보다 내 크로스헤어 중심(1.0)에 더 가까운 녀석이라면?
					if (DotProduct > 0.5f && DotProduct > BestDotProduct)
					{
						BestDotProduct = DotProduct; // 최고 기록 갱신
						BestTarget = HitActor;       // 타겟 찜하기
						BestImpactPoint = Hit.ImpactPoint;
					}
				}
			}

			// 5. 검사 결과, 조건을 만족하는 '최고의 타겟'이 진짜로 존재한다면 밧줄 발사!
			if (BestTarget != nullptr)
			{
				SRCharacter->StartGrapple(BestImpactPoint);
				return; // 성공했으니 여기서 종료
			}
		}
	}
	

	// 허공에 쏘거나 엉뚱한 벽에 쏘면 아무 일도 안 하고 어빌리티 즉시 종료
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void USRGA_Grappling::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 어빌리티가 끝날 때 (손을 떼거나, 보스한테 맞아서 기절 등 강제 취소될 때 모두 포함!)
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ASRPlayerCharacter* SRCharacter = Cast<ASRPlayerCharacter>(ActorInfo->AvatarActor.Get());
		if (SRCharacter)
		{
			// ⭐️ 핵심 3: 캐릭터에게 "스윙 멈추고 밧줄 당장 회수해!" 라고 명령 하달
			SRCharacter->StopGrapple();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);	
}

void USRGA_Grappling::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	// 유저가 키보드에서 손을 떼서 GASInputReleased가 실행되면 무조건 이 코드가 발동합니다.
	if (IsActive())
	{
		// 당장 어빌리티 종료! (이러면 EndAbility가 불리면서 StopGrapple()이 실행됨)
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void USRGA_Grappling::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
