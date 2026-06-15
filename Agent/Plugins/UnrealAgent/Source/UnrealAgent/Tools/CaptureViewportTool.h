#pragma once

#include "CoreMinimal.h"
#include "Mcp/McpTypes.h"
#include "CaptureViewportTool.generated.h"

/**
 * 활성 에디터(또는 PIE) 뷰포트를 캡처하여 JPEG 이미지로 반환하는 MCP 도구입니다.
 *
 * 모델이 씬/에셋을 변경한 뒤 결과를 시각적으로 확인(자가 검증)할 수 있게 합니다.
 * 픽셀을 읽어 종횡비를 유지한 채 다운스케일하고 JPEG로 인코딩한 뒤 base64로 반환하며,
 * McpServer가 이를 MCP image content 블록으로 직렬화하여 Claude에 전달합니다.
 *
 * 파라미터가 없습니다 (현재 활성 뷰포트를 캡처).
 */
USTRUCT(meta=(McpTool="capture_viewport"))
struct FCaptureViewportTool : public FMcpTool
{
	GENERATED_BODY()

	virtual FString ToolDescription() const override
	{
		return TEXT(
			"Capture the active Unreal Editor viewport (or PIE viewport if running) as a JPEG image.\n"
			"\n"
			"- Use this to visually verify the result after spawning/moving actors, assigning\n"
			"  materials, or changing the scene.\n"
			"- Returns an image you can directly look at — no parameters needed.\n"
			"- Captures the currently focused level viewport. Make sure a level is open.");
	}

	virtual FMcpResponse Execute() override;
};
