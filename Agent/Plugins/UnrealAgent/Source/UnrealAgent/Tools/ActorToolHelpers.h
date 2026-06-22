#pragma once

#include "CoreMinimal.h"

class AActor;
class UClass;
class UWorld;

/**
 * 액터 관련 MCP 도구(get_level_actors, actor_modify)가 공유하는 헬퍼입니다.
 */
namespace ActorToolHelpers
{
	/** 에디터 월드를 반환합니다. 없으면 nullptr. */
	UWorld* GetEditorWorld();

	/**
	 * 이름 또는 아웃라이너 라벨로 액터를 찾습니다 (대소문자 무시, 부분 일치 폴백).
	 * 정확히 일치하는 이름/라벨을 우선하고, 없으면 부분 일치 첫 항목을 반환합니다.
	 */
	AActor* FindActorByNameOrLabel(UWorld* World, const FString& NameOrLabel);

	/**
	 * 클래스 경로/짧은 이름으로 액터 UClass를 해석합니다.
	 * - "/Game/BP_Door" 같은 블루프린트 경로 → generated class
	 * - "StaticMeshActor" / "PointLight" 같은 엔진 클래스 짧은 이름
	 * 실패 시 nullptr과 OutError를 설정합니다.
	 */
	UClass* ResolveActorClass(const FString& ClassPathOrName, FString& OutError);
}
