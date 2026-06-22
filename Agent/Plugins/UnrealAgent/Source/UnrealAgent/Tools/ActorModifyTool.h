#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "ActorModifyTool.generated.h"

/**
 * 레벨 액터를 변경하는 MCP 도구입니다 (operation 디스패치).
 *
 * spawn/move/delete/set_property를 구조화된 평탄 파라미터로 수행하여, execute_python
 * 우회 없이 안정적인 결과를 반환합니다. blueprint_modify와 동일하게 모든 파라미터를
 * flat 스칼라로 선언해 MCP inputSchema에 노출합니다(중첩 객체 직렬화 불일치 방지).
 */
USTRUCT(meta=(McpTool="actor_modify"))
struct FActorModifyTool : public FMcpTool
{
	GENERATED_BODY()

	/** 수행할 연산 */
	UPROPERTY(meta=(ToolParam="operation", Required,
		Description="One of: spawn, move, delete, set_property"))
	FString Operation;

	/** [spawn] 액터 클래스 경로 또는 짧은 이름 */
	UPROPERTY(meta=(ToolParam="class_path",
		Description="[spawn] Actor class: engine short name ('StaticMeshActor', 'PointLight') or Blueprint path ('/Game/BP_Door')."))
	FString ClassPath;

	/** [move/delete/set_property] 대상 액터 이름 또는 라벨 */
	UPROPERTY(meta=(ToolParam="actor_name",
		Description="[move/delete/set_property] Target actor name or outliner label (case-insensitive, partial match fallback)."))
	FString ActorName;

	/** [spawn] 생성할 액터의 원하는 이름 (선택) */
	UPROPERTY(meta=(ToolParam="name",
		Description="[spawn] Optional desired name/label for the new actor."))
	FString NewName;

	// ── Transform (spawn / move) ──

	UPROPERTY(meta=(ToolParam="loc_x", Description="[spawn/move] Location X (cm)."))
	double LocX = 0.0;
	UPROPERTY(meta=(ToolParam="loc_y", Description="[spawn/move] Location Y (cm)."))
	double LocY = 0.0;
	UPROPERTY(meta=(ToolParam="loc_z", Description="[spawn/move] Location Z (cm)."))
	double LocZ = 0.0;

	UPROPERTY(meta=(ToolParam="rot_pitch", Description="[spawn/move] Rotation pitch (deg)."))
	double RotPitch = 0.0;
	UPROPERTY(meta=(ToolParam="rot_yaw", Description="[spawn/move] Rotation yaw (deg)."))
	double RotYaw = 0.0;
	UPROPERTY(meta=(ToolParam="rot_roll", Description="[spawn/move] Rotation roll (deg)."))
	double RotRoll = 0.0;

	UPROPERTY(meta=(ToolParam="scale_x", Description="[spawn/move] Scale X (default 1)."))
	double ScaleX = 1.0;
	UPROPERTY(meta=(ToolParam="scale_y", Description="[spawn/move] Scale Y (default 1)."))
	double ScaleY = 1.0;
	UPROPERTY(meta=(ToolParam="scale_z", Description="[spawn/move] Scale Z (default 1)."))
	double ScaleZ = 1.0;

	UPROPERTY(meta=(ToolParam="apply_location",
		Description="[move] If true, apply loc_x/y/z. Spawn always uses location."))
	bool bApplyLocation = false;
	UPROPERTY(meta=(ToolParam="apply_rotation",
		Description="[move] If true, apply rot_pitch/yaw/roll."))
	bool bApplyRotation = false;
	UPROPERTY(meta=(ToolParam="apply_scale",
		Description="[move] If true, apply scale_x/y/z."))
	bool bApplyScale = false;

	UPROPERTY(meta=(ToolParam="relative",
		Description="[move] If true, location/rotation are added to current, scale is multiplied."))
	bool bRelative = false;

	// ── set_property ──

	UPROPERTY(meta=(ToolParam="property",
		Description="[set_property] Property path. Optional single component prefix, e.g. 'StaticMeshComponent.CastShadow' or just 'bHidden'."))
	FString Property;

	UPROPERTY(meta=(ToolParam="value",
		Description="[set_property] Value as text. Numbers/bools/strings, UE struct text '(X=1,Y=2,Z=3)', or an asset path for object properties ('/Game/Meshes/SM_Rock')."))
	FString Value;

	virtual FString ToolDescription() const override
	{
		return TEXT(
			"Modify level actors: spawn, move, delete, or set a property.\n"
			"\n"
			"- spawn: needs class_path (+ optional name and transform). Scale defaults to 1.\n"
			"- move: needs actor_name + apply_location/apply_rotation/apply_scale flags for\n"
			"  which transforms to change. Set relative=true for an offset.\n"
			"- delete: needs actor_name.\n"
			"- set_property: needs actor_name, property path, and value. Use a component\n"
			"  prefix for component properties (e.g. 'StaticMeshComponent.CastShadow').\n"
			"- Prefer this over execute_python for these operations — it returns structured\n"
			"  results and avoids deprecated EditorLevelLibrary patterns.\n"
			"- After modifying, call get_output_log to check for warnings/errors.");
	}

	virtual FMcpResponse Execute() override;

private:
	FMcpResponse ExecuteSpawn();
	FMcpResponse ExecuteMove();
	FMcpResponse ExecuteDelete();
	FMcpResponse ExecuteSetProperty();
};
