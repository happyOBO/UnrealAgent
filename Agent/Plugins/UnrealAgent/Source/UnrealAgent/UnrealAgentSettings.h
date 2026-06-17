#pragma once

#include "CoreMinimal.h"

/**
 * 프로젝트 설정 파일({ProjectDir}/.unrealagent/settings.local.json)에서
 * 포트 등 런타임 설정을 읽는 헬퍼입니다.
 *
 * 포트의 단일 출처는 settings.local.json이며, 백엔드(UnrealAgent_Server)도 같은 파일을 읽습니다.
 * 파일이나 키가 없으면 아래 기본값으로 폴백하여 기존 동작을 보존합니다.
 */
class FUnrealAgentSettings
{
public:
	/** MCP HTTP 서버 기본 포트입니다. settings.local.json이 없을 때의 폴백 값입니다. */
	static constexpr uint32 DefaultMcpServerPort = 55559;

	/** 프론트엔드(Agent Server) 기본 포트입니다. settings.local.json이 없을 때의 폴백 값입니다. */
	static constexpr uint32 DefaultFrontendPort = 55558;

	/**
	 * MCP 서버가 listen할 포트를 반환합니다.
	 * settings.local.json의 mcpServers.UnrealMCP.url에서 포트를 파싱하며,
	 * 읽지 못하면 DefaultMcpServerPort를 반환합니다.
	 */
	static uint32 GetMcpServerPort();

	/**
	 * 프론트엔드 포트를 반환합니다.
	 * settings.local.json의 frontendPort 키를 읽으며, 없으면 DefaultFrontendPort를 반환합니다.
	 */
	static uint32 GetFrontendPort();

private:
	/** settings.local.json 전체 경로를 반환합니다. */
	static FString GetSettingsPath();
};
