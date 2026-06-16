#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "WidgetModifyTool.generated.h"

/**
 * UMG 위젯 블루프린트의 위젯 트리(UWidgetTree)를 편집하는 MCP 도구입니다.
 *
 * Python `unreal` API에서는 WidgetTree가 protected라 위젯 추가/삭제가 불가능합니다.
 * 이 도구는 C++ 에디터 API로 위젯 트리를 네이티브 편집합니다. 위젯 BP의 이벤트
 * 그래프(이벤트/노드)는 blueprint_modify로 다루며, 여기서는 트리 구조만 담당합니다.
 */
USTRUCT(meta=(McpTool="widget_modify"))
struct FWidgetModifyTool : public FMcpTool
{
	GENERATED_BODY()

	UPROPERTY(meta=(ToolParam="operation", Required,
		Description="One of: add_widget, remove_widget, set_widget_property, reparent_widget, list_widgets"))
	FString Operation;

	UPROPERTY(meta=(ToolParam="blueprint_path", Required,
		Description="Widget Blueprint asset path, e.g. /Game/UI/WBP_Menu"))
	FString BlueprintPath;

	UPROPERTY(meta=(ToolParam="widget_type",
		Description="[add_widget] UWidget subclass name, e.g. Button, TextBlock, VerticalBox, Image, CanvasPanel"))
	FString WidgetType;

	UPROPERTY(meta=(ToolParam="widget_name",
		Description="[add_widget/remove_widget/set_widget_property/reparent_widget] Widget name"))
	FString WidgetName;

	UPROPERTY(meta=(ToolParam="parent",
		Description="[add_widget] Parent panel widget name. Empty = set as root (only if no root yet)."))
	FString Parent;

	UPROPERTY(meta=(ToolParam="new_parent",
		Description="[reparent_widget] New parent panel widget name"))
	FString NewParent;

	UPROPERTY(meta=(ToolParam="property",
		Description="[set_widget_property] Property name on the widget (e.g. ToolTipText)"))
	FString Property;

	UPROPERTY(meta=(ToolParam="value",
		Description="[set_widget_property] New value (string; parsed by property type)"))
	FString Value;

	virtual FString ToolDescription() const override;
	virtual FMcpResponse Execute() override;
};
