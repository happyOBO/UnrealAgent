#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "RunConsoleCommandTool.generated.h"

/**
 * 에디터 콘솔 명령을 실행하는 MCP 도구입니다.
 *
 * `stat fps`, `show collision`, `r.ScreenPercentage 50` 같은 진단/프로파일링/렌더링
 * 콘솔 명령을 GEditor->Exec로 실행하고 출력을 회수합니다. Python으로 우회하지 않고
 * 콘솔 명령을 직접 실행할 수 있게 합니다.
 */
USTRUCT(meta=(McpTool="run_console_command"))
struct FRunConsoleCommandTool : public FMcpTool
{
	GENERATED_BODY()

	/** 실행할 콘솔 명령 */
	UPROPERTY(meta=(ToolParam="command", Required,
		Description="The console command to execute, e.g. 'stat fps', 'show collision', 'r.ScreenPercentage 50'."))
	FString Command;

	virtual FString ToolDescription() const override
	{
		return TEXT(
			"Execute an Unreal Editor console command and return its output.\n"
			"\n"
			"- For diagnostics/profiling/rendering toggles: 'stat unit', 'stat fps',\n"
			"  'show collision', 'r.*' cvars, 'DumpConsoleCommands', etc.\n"
			"- Returns whatever the command prints. Many commands toggle state and\n"
			"  print nothing — pair with get_output_log if you need to see side effects.\n"
			"- Do NOT use for actions that have a dedicated tool (spawning actors,\n"
			"  editing Blueprints). Use this for engine console commands only.");
	}

	virtual FMcpResponse Execute() override;
};
