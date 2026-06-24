#include "Tools/WidgetModifyTool.h"
#include "Blueprint/WidgetTreeEditor.h"
#include "Blueprint/BlueprintGraphEditor.h"
#include "WidgetBlueprint.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetModifyTool)

FString FWidgetModifyTool::ToolDescription() const
{
	return TEXT(
		"Edit a UMG Widget Blueprint's widget tree natively (Python cannot — WidgetTree is protected).\n"
		"After any modifying op the widget blueprint is compiled and marked dirty.\n"
		"For the widget BP's event graph (events, Cast, delegate binding, logic), use blueprint_modify instead.\n"
		"\n"
		"- add_widget: widget_type, [widget_name], [parent]\n"
		"    widget_type = UWidget subclass (Button|TextBlock|VerticalBox|HorizontalBox|CanvasPanel|Image|...)\n"
		"    parent empty = set as root (only when the tree has no root yet); otherwise parent must be a panel widget.\n"
		"- remove_widget: widget_name\n"
		"- set_widget_property: widget_name, property, value.\n"
		"    property supports dotted struct paths (e.g. Font.Size=48, Font.TypefaceFontName=Bold) and\n"
		"    layout-slot props via a 'Slot.' prefix (e.g. Slot.Anchors=(0.5,0.5), Slot.Padding=20, Slot.Alignment=(0.5,0.5)).\n"
		"    Slot.* targets the widget's parent-panel slot (CanvasPanelSlot/VerticalBoxSlot/...).\n"
		"- reparent_widget: widget_name, new_parent (a panel widget)\n"
		"- list_widgets: (read-only) returns the widget tree hierarchy JSON\n"
		"\n"
		"Tip: to wire a button click, add_widget the Button here, then blueprint_modify\n"
		"add_node node_type=ComponentBoundEvent component=<ButtonName> delegate_property=OnClicked.");
}

FMcpResponse FWidgetModifyTool::Execute()
{
	if (Operation.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'operation'"));
	if (BlueprintPath.IsEmpty())
		return FMcpResponse::Failure(TEXT("Missing 'blueprint_path'"));

	FString Error;
	UWidgetBlueprint* WidgetBP = FWidgetTreeEditor::LoadWidgetBlueprint(BlueprintPath, Error);
	if (!WidgetBP)
		return FMcpResponse::Failure(Error);

	// list_widgets: 읽기 전용.
	if (Operation.Equals(TEXT("list_widgets"), ESearchCase::IgnoreCase))
	{
		return FMcpResponse::Success(FWidgetTreeEditor::ListWidgets(WidgetBP));
	}

	FString ResultSummary;

	if (Operation.Equals(TEXT("add_widget"), ESearchCase::IgnoreCase))
	{
		if (WidgetType.IsEmpty())
			return FMcpResponse::Failure(TEXT("add_widget requires 'widget_type'"));
		if (!FWidgetTreeEditor::AddWidget(WidgetBP, WidgetType, WidgetName, Parent, Error))
			return FMcpResponse::Failure(Error);
		ResultSummary = FString::Printf(TEXT("Added %s%s%s."), *WidgetType,
			WidgetName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" '%s'"), *WidgetName),
			Parent.IsEmpty() ? TEXT(" as root") : *FString::Printf(TEXT(" under '%s'"), *Parent));
	}
	else if (Operation.Equals(TEXT("remove_widget"), ESearchCase::IgnoreCase))
	{
		if (WidgetName.IsEmpty())
			return FMcpResponse::Failure(TEXT("remove_widget requires 'widget_name'"));
		if (!FWidgetTreeEditor::RemoveWidget(WidgetBP, WidgetName, Error))
			return FMcpResponse::Failure(Error);
		ResultSummary = FString::Printf(TEXT("Removed widget '%s'."), *WidgetName);
	}
	else if (Operation.Equals(TEXT("set_widget_property"), ESearchCase::IgnoreCase))
	{
		if (WidgetName.IsEmpty() || Property.IsEmpty())
			return FMcpResponse::Failure(TEXT("set_widget_property requires 'widget_name' and 'property'"));
		if (!FWidgetTreeEditor::SetWidgetProperty(WidgetBP, WidgetName, Property, Value, Error))
			return FMcpResponse::Failure(Error);
		ResultSummary = FString::Printf(TEXT("Set %s.%s = '%s'."), *WidgetName, *Property, *Value);
	}
	else if (Operation.Equals(TEXT("reparent_widget"), ESearchCase::IgnoreCase))
	{
		if (WidgetName.IsEmpty() || NewParent.IsEmpty())
			return FMcpResponse::Failure(TEXT("reparent_widget requires 'widget_name' and 'new_parent'"));
		if (!FWidgetTreeEditor::ReparentWidget(WidgetBP, WidgetName, NewParent, Error))
			return FMcpResponse::Failure(Error);
		ResultSummary = FString::Printf(TEXT("Reparented '%s' under '%s'."), *WidgetName, *NewParent);
	}
	else
	{
		return FMcpResponse::Failure(FString::Printf(TEXT("Unknown operation '%s'"), *Operation));
	}

	// 컴파일 + 저장 마크 (UWidgetBlueprint IS-A UBlueprint이므로 재사용).
	bool bCompiled = false;
	const FString CompileMsg = FBlueprintGraphEditor::CompileAndSave(WidgetBP, bCompiled);

	const FString Full = ResultSummary + TEXT("\n") + CompileMsg;
	return bCompiled ? FMcpResponse::Success(Full) : FMcpResponse::Failure(Full);
}
