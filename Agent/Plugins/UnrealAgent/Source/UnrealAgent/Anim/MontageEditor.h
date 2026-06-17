#pragma once

#include "CoreMinimal.h"

class UAnimMontage;

/**
 * AnimMontage 에셋 편집 헬퍼입니다.
 *
 * Python `unreal` API로 불가능한 몽타주 슬롯 트랙(SlotAnimTracks) 조작을 UE 에디터 API로 수행합니다.
 * UAnimMontage::SlotAnimTracks 는 C++에서 public 멤버라 직접 읽고 쓸 수 있습니다(Python 바인딩에서만 protected로 차단됨).
 * 모든 함수는 게임 스레드에서 호출되어야 합니다.
 */
class FMontageEditor
{
public:
	/** 몽타주 에셋을 로드합니다. */
	static UAnimMontage* LoadMontage(const FString& MontagePath, FString& OutError);

	/**
	 * 몽타주 에셋을 새로 생성합니다.
	 * 스켈레톤은 SkeletonPath 또는 AnimSequencePath(시퀀스의 스켈레톤)에서 결정합니다.
	 * AnimSequencePath가 주어지면 기본 슬롯에 해당 시퀀스를 세그먼트로 채웁니다.
	 */
	static UAnimMontage* CreateMontage(const FString& MontagePath, const FString& SkeletonPath, const FString& AnimSequencePath, FString& OutError);

	/** 슬롯 이름을 변경합니다 (SlotAnimTracks[i].SlotName). */
	static bool RenameSlot(UAnimMontage* Montage, const FString& OldSlotName, const FString& NewSlotName, FString& OutError);

	/** 새 슬롯 트랙을 추가합니다. */
	static bool AddSlot(UAnimMontage* Montage, const FString& SlotName, FString& OutError);

	/** 지정 슬롯에 AnimSequence 세그먼트를 추가합니다. */
	static bool AddSegment(UAnimMontage* Montage, const FString& SlotName, const FString& AnimSequencePath, float StartTime, FString& OutError);

	/** 슬롯/세그먼트 구성을 사람이 읽을 수 있는 텍스트(JSON 유사)로 반환합니다. */
	static FString GetInfo(UAnimMontage* Montage);
};
