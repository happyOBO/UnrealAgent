#include "Log/AgentLogCapture.h"

FAgentLogCapture& FAgentLogCapture::Get()
{
	static FAgentLogCapture Instance;
	return Instance;
}

void FAgentLogCapture::Startup()
{
	FAgentLogCapture& Instance = Get();
	if (!Instance.bRegistered && GLog)
	{
		Instance.Ring.Reserve(Capacity);
		GLog->AddOutputDevice(&Instance);
		Instance.bRegistered = true;
	}
}

void FAgentLogCapture::Shutdown()
{
	FAgentLogCapture& Instance = Get();
	if (Instance.bRegistered && GLog)
	{
		GLog->RemoveOutputDevice(&Instance);
		Instance.bRegistered = false;
	}
}

void FAgentLogCapture::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	// 제어용 verbosity(SetColor 등)는 무시합니다.
	if (Verbosity == ELogVerbosity::SetColor || Message == nullptr)
	{
		return;
	}

	FAgentLogEntry Entry;
	// Verbosity에는 BreakOnLog 등 플래그 비트가 섞일 수 있어 마스킹합니다.
	Entry.Verbosity = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
	Entry.Category = Category;
	Entry.Message = Message;

	FScopeLock ScopeLock(&Lock);

	if (Ring.Num() < Capacity)
	{
		Ring.Add(MoveTemp(Entry));
	}
	else
	{
		Ring[Head] = MoveTemp(Entry);
	}

	Head = (Head + 1) % Capacity;
	Count = FMath::Min(Count + 1, Capacity);
}

TArray<FAgentLogEntry> FAgentLogCapture::Snapshot() const
{
	FScopeLock ScopeLock(&Lock);

	TArray<FAgentLogEntry> Result;
	Result.Reserve(Count);

	// 가장 오래된 항목부터 순서대로 복사합니다.
	const int32 Start = (Count < Capacity) ? 0 : Head;
	for (int32 i = 0; i < Count; ++i)
	{
		Result.Add(Ring[(Start + i) % Capacity]);
	}

	return Result;
}
