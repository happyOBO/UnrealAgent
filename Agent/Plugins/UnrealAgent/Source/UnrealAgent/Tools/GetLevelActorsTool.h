#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "GetLevelActorsTool.generated.h"

/**
 * 현재 레벨의 액터를 조회하는 읽기 전용 MCP 도구입니다.
 *
 * Python으로 `get_all_level_actors()`를 호출해 전부 print하는 footgun(큰 레벨에서
 * 에디터 프리즈/컨텍스트 폭주)을 방지하기 위해, 클래스/이름 필터와 페이지네이션을
 * 내장한 구조화된 조회를 제공합니다.
 */
USTRUCT(meta=(McpTool="get_level_actors"))
struct FGetLevelActorsTool : public FMcpTool
{
	GENERATED_BODY()

	/** 클래스 이름 부분 일치 필터 (예: StaticMesh, Light) */
	UPROPERTY(meta=(ToolParam="class_filter",
		Description="Optional case-insensitive substring match on the actor's class name (e.g. 'StaticMesh', 'Light')."))
	FString ClassFilter;

	/** 이름/라벨 부분 일치 필터 */
	UPROPERTY(meta=(ToolParam="name_filter",
		Description="Optional case-insensitive substring match on the actor's name or outliner label."))
	FString NameFilter;

	/** 반환할 최대 액터 수 (페이지네이션) */
	UPROPERTY(meta=(ToolParam="limit",
		Description="Max actors to return. Default 50, max 500."))
	int32 Limit = 50;

	/** 페이지네이션 오프셋 */
	UPROPERTY(meta=(ToolParam="offset",
		Description="Pagination offset (skip this many matches). Default 0."))
	int32 Offset = 0;

	virtual FString ToolDescription() const override
	{
		return TEXT(
			"List actors in the current editor level (read-only), with filtering and pagination.\n"
			"\n"
			"- Prefer this over execute_python's get_all_level_actors() for inspecting a level —\n"
			"  it is structured, paginated, and won't freeze the editor on large levels.\n"
			"- Filter by class_filter and/or name_filter to narrow results.\n"
			"- Returns each actor's name, label, class, and location, plus pagination info.");
	}

	virtual FMcpResponse Execute() override;
};
