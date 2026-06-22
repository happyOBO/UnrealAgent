#include "Tools/GetOutputLogTool.h"
#include "Log/AgentLogCapture.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(GetOutputLogTool)

namespace
{
	// max_lines의 절대 상한 (컨텍스트 폭주 방지).
	constexpr int32 AbsoluteMaxLines = 1000;

	// 심각도 문자열을 "이 verbosity까지 포함" 임계값으로 변환합니다.
	// ELogVerbosity는 값이 작을수록 심각합니다 (Fatal=1 ... Log=5).
	ELogVerbosity::Type ParseSeverityThreshold(const FString& Severity)
	{
		const FString Normalized = Severity.TrimStartAndEnd().ToLower();

		if (Normalized == TEXT("error"))
		{
			return ELogVerbosity::Error;
		}
		if (Normalized == TEXT("all"))
		{
			// Display/Log까지 포함하되 Verbose/VeryVerbose는 제외하여 노이즈를 줄입니다.
			return ELogVerbosity::Log;
		}

		// 기본값: 경고 + 에러.
		return ELogVerbosity::Warning;
	}

	// verbosity를 사람이 읽는 라벨로 변환합니다.
	const TCHAR* VerbosityLabel(ELogVerbosity::Type Verbosity)
	{
		switch (Verbosity)
		{
		case ELogVerbosity::Fatal:   return TEXT("Fatal");
		case ELogVerbosity::Error:   return TEXT("Error");
		case ELogVerbosity::Warning: return TEXT("Warning");
		case ELogVerbosity::Display: return TEXT("Display");
		default:                     return TEXT("Log");
		}
	}
}

FMcpResponse FGetOutputLogTool::Execute()
{
	const ELogVerbosity::Type Threshold = ParseSeverityThreshold(Severity);
	const int32 Limit = FMath::Clamp(MaxLines <= 0 ? 100 : MaxLines, 1, AbsoluteMaxLines);

	const TArray<FAgentLogEntry> Entries = FAgentLogCapture::Get().Snapshot();

	// 오래된 순서로 필터링한 뒤, 마지막 Limit개(가장 최근)만 취합니다.
	TArray<FString> Matched;
	for (const FAgentLogEntry& Entry : Entries)
	{
		// verbosity 값이 임계값보다 크면(덜 심각하면) 제외합니다.
		// VeryVerbose/Verbose는 'all'에서도 제외합니다.
		if (Entry.Verbosity > Threshold || Entry.Verbosity > ELogVerbosity::Log)
		{
			continue;
		}

		if (!Contains.IsEmpty() && !Entry.Message.Contains(Contains, ESearchCase::IgnoreCase))
		{
			continue;
		}

		Matched.Add(FString::Printf(TEXT("[%s] %s: %s"),
			*Entry.Category.ToString(), VerbosityLabel(Entry.Verbosity), *Entry.Message));
	}

	const int32 TotalMatched = Matched.Num();
	if (TotalMatched > Limit)
	{
		Matched.RemoveAt(0, TotalMatched - Limit, EAllowShrinking::No);
	}

	if (Matched.Num() == 0)
	{
		return FMcpResponse::Success(FString::Printf(
			TEXT("No log lines matched (severity='%s'%s). No warnings/errors likely means the action succeeded cleanly."),
			Severity.IsEmpty() ? TEXT("warning") : *Severity,
			Contains.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", contains='%s'"), *Contains)));
	}

	const FString Header = FString::Printf(
		TEXT("Showing %d of %d matching log line(s) (severity='%s', most recent last):\n"),
		Matched.Num(), TotalMatched, Severity.IsEmpty() ? TEXT("warning") : *Severity);

	return FMcpResponse::Success(Header + FString::Join(Matched, TEXT("\n")));
}
