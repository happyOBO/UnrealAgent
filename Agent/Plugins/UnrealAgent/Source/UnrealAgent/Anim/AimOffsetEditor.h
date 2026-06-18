#pragma once

#include "CoreMinimal.h"

class UAimOffsetBlendSpace;

/**
 * AimOffset(UAimOffsetBlendSpace) 에셋 편집 헬퍼입니다.
 *
 * Python `unreal` API로 불가능한 BlendSpace 샘플/축 조작을 UE 에디터 API로 수행합니다.
 * 샘플 추가(AddSample)/축 조회(GetBlendParameter)는 WITH_EDITOR 전용 API이며,
 * 본 모듈은 에디터 전용이라 직접 호출할 수 있습니다.
 * 모든 함수는 게임 스레드에서 호출되어야 합니다.
 */
class FAimOffsetEditor
{
public:
	/** AimOffset 에셋을 로드합니다 (`/Game/` 접두어 보정 + cooked 차단). */
	static UAimOffsetBlendSpace* LoadAimOffset(const FString& AimOffsetPath, FString& OutError);

	/**
	 * AimOffset 에셋을 새로 생성합니다.
	 * 스켈레톤은 명시적으로 요구합니다(aim offset은 보통 additive 포즈라 시퀀스 유도가 모호함).
	 */
	static UAimOffsetBlendSpace* CreateAimOffset(const FString& AimOffsetPath, const FString& SkeletonPath, FString& OutError);

	/**
	 * 지정 Yaw/Pitch 좌표에 AnimSequence 샘플을 추가합니다.
	 * 좌표가 현재 축 범위를 벗어나면 범위를 확장한 뒤 추가합니다.
	 */
	static bool AddSample(UAimOffsetBlendSpace* AimOffset, const FString& AnimSequencePath, float Yaw, float Pitch, FString& OutError);

	/** 축 구성/샘플 목록을 사람이 읽을 수 있는 텍스트(JSON 유사)로 반환합니다. */
	static FString GetInfo(UAimOffsetBlendSpace* AimOffset);
};
