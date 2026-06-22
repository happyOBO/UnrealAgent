#include "Tools/RunConsoleCommandTool.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/OutputDevice.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(RunConsoleCommandTool)

namespace
{
	// GEditor->Exec 출력을 문자열로 모으는 간단한 출력 디바이스입니다.
	// (FStringOutputDevice의 헤더 위치가 엔진 버전마다 달라 직접 정의합니다.)
	class FConsoleCaptureDevice : public FOutputDevice
	{
	public:
		FString Output;

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override
		{
			Output += V;
			Output += TEXT("\n");
		}
	};
}

FMcpResponse FRunConsoleCommandTool::Execute()
{
	if (!GEditor)
	{
		return FMcpResponse::Failure(TEXT("Editor is not available."));
	}

	const FString Trimmed = Command.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return FMcpResponse::Failure(TEXT("command is empty."));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();

	// 명령 출력을 문자열로 캡처합니다.
	FConsoleCaptureDevice OutputDevice;
	GEditor->Exec(World, *Trimmed, OutputDevice);

	const FString Output = OutputDevice.Output.TrimStartAndEnd();

	if (Output.IsEmpty())
	{
		return FMcpResponse::Success(FString::Printf(
			TEXT("Executed '%s' (no console output; the command may toggle state — check get_output_log or capture_viewport for effects)."),
			*Trimmed));
	}

	return FMcpResponse::Success(FString::Printf(TEXT("Executed '%s':\n%s"), *Trimmed, *Output));
}
