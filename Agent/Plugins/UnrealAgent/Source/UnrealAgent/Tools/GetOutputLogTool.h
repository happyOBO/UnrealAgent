#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "GetOutputLogTool.generated.h"

/**
 * 에디터 출력 로그에서 최근 항목을 회수하는 MCP 도구입니다.
 *
 * capture_viewport(시각 검증)와 짝을 이루는 텍스트 자가검증 수단입니다.
 * 씬/에셋/블루프린트를 변경하거나 Python을 실행한 뒤, 경고·에러가 발생했는지
 * 확인하여 모델이 결과를 검증할 수 있게 합니다. 로그는 FAgentLogCapture가
 * 메모리에 누적한 것을 심각도/부분문자열로 필터링하여 반환합니다.
 */
USTRUCT(meta=(McpTool="get_output_log"))
struct FGetOutputLogTool : public FMcpTool
{
	GENERATED_BODY()

	/** 심각도 필터 */
	UPROPERTY(meta=(ToolParam="severity",
		Description="Severity filter: 'error' (errors only), 'warning' (warnings + errors, DEFAULT), or 'all' (everything up to Display/Log). Empty = warning."))
	FString Severity;

	/** 반환할 최대 라인 수 (가장 최근 항목 우선) */
	UPROPERTY(meta=(ToolParam="max_lines",
		Description="Maximum number of most-recent matching lines to return. Default 100, max 1000."))
	int32 MaxLines = 100;

	/** 부분 문자열 필터 (대소문자 무시). 비어 있으면 필터 안 함. */
	UPROPERTY(meta=(ToolParam="contains",
		Description="Optional case-insensitive substring filter. Only lines containing this text are returned."))
	FString Contains;

	virtual FString ToolDescription() const override
	{
		return TEXT(
			"Retrieve recent Unreal Editor output log entries (warnings/errors).\n"
			"\n"
			"- Use this to verify the result AFTER an action: spawning/moving actors,\n"
			"  assigning materials, editing Blueprints, or running execute_python.\n"
			"- Pairs with capture_viewport: viewport shows the visual, this shows the log.\n"
			"- Defaults to warnings + errors (the most useful signal). Pass severity='all'\n"
			"  to also see Display/Log lines, or severity='error' for errors only.\n"
			"- Returns the MOST RECENT matching lines. Use `contains` to narrow to a\n"
			"  specific category, asset name, or message.\n"
			"- If nothing matches, the action likely produced no warnings/errors.");
	}

	virtual FMcpResponse Execute() override;
};
