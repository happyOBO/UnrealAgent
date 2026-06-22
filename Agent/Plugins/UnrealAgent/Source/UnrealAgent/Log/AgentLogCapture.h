#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/**
 * 단일 로그 항목의 스냅샷입니다.
 */
struct FAgentLogEntry
{
	/** 로그 심각도 (Error/Warning/Display/Log 등) */
	ELogVerbosity::Type Verbosity = ELogVerbosity::Log;

	/** 로그 카테고리 (예: LogTemp, LogBlueprint) */
	FName Category;

	/** 메시지 본문 */
	FString Message;
};

/**
 * 에디터 로그를 링 버퍼에 캡처하는 전역 FOutputDevice입니다.
 *
 * get_output_log 도구가 심각도(Error/Warning)별로 최근 로그를 회수할 수 있도록,
 * 모듈 시작 시 GLog에 등록되어 모든 로그를 메모리 링 버퍼에 누적합니다.
 * UnrealClaude는 로그 *파일*을 파싱하지만, 여기서는 심각도(ELogVerbosity)를
 * 정확히 보존하기 위해 출력 디바이스 방식을 사용합니다.
 *
 * 스레드 안전: Serialize는 임의 스레드에서 호출될 수 있어 락으로 보호합니다.
 */
class FAgentLogCapture : public FOutputDevice
{
public:
	/** 전역 싱글톤 인스턴스를 반환합니다. */
	static FAgentLogCapture& Get();

	/** GLog에 캡처 디바이스를 등록합니다. 모듈 StartupModule에서 1회 호출합니다. */
	static void Startup();

	/** GLog에서 캡처 디바이스를 제거합니다. 모듈 ShutdownModule에서 호출합니다. */
	static void Shutdown();

	//~ Begin FOutputDevice interface
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	//~ End FOutputDevice interface

	/** 현재 링 버퍼의 항목들을 오래된 순서로 복사하여 반환합니다 (스레드 안전). */
	TArray<FAgentLogEntry> Snapshot() const;

private:
	/** 링 버퍼 용량 (최근 N개 항목 보관). */
	static constexpr int32 Capacity = 4000;

	/** 동시 접근 보호 락. */
	mutable FCriticalSection Lock;

	/** 링 버퍼 저장소. */
	TArray<FAgentLogEntry> Ring;

	/** 다음 기록 위치. */
	int32 Head = 0;

	/** 현재 보관 중인 항목 수 (Capacity로 포화). */
	int32 Count = 0;

	/** GLog에 등록되어 있는지 여부. */
	bool bRegistered = false;
};
