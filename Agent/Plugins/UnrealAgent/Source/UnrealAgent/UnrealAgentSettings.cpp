#include "UnrealAgentSettings.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	/** settings.local.json을 파싱하여 루트 JSON 오브젝트를 반환합니다. 실패하면 null입니다. */
	TSharedPtr<FJsonObject> LoadSettingsObject(const FString& Path)
	{
		if (!FPaths::FileExists(Path))
		{
			return nullptr;
		}

		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return nullptr;
		}

		return Root;
	}

	/** "http://localhost:55559/mcp" 같은 URL에서 포트를 파싱합니다. 실패하면 Fallback을 반환합니다. */
	uint32 ParsePortFromUrl(const FString& Url, const uint32 Fallback)
	{
		// 스킴(://)이 있으면 그 뒤로 자릅니다 → "localhost:55559/mcp"
		FString Rest = Url;
		const int32 SchemeEnd = Url.Find(TEXT("://"));
		if (SchemeEnd != INDEX_NONE)
		{
			Rest = Url.Mid(SchemeEnd + 3);
		}

		// 경로(/) 앞까지만 사용 → "localhost:55559"
		int32 SlashPos = INDEX_NONE;
		if (Rest.FindChar(TEXT('/'), SlashPos))
		{
			Rest = Rest.Left(SlashPos);
		}

		// 마지막 ':' 뒤가 포트입니다.
		int32 ColonPos = INDEX_NONE;
		if (Rest.FindLastChar(TEXT(':'), ColonPos))
		{
			const FString PortStr = Rest.Mid(ColonPos + 1);
			if (!PortStr.IsEmpty() && PortStr.IsNumeric())
			{
				return static_cast<uint32>(FCString::Atoi(*PortStr));
			}
		}

		return Fallback;
	}
}

FString FUnrealAgentSettings::GetSettingsPath()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT(".unrealagent"), TEXT("settings.local.json"));
}

uint32 FUnrealAgentSettings::GetMcpServerPort()
{
	const TSharedPtr<FJsonObject> Root = LoadSettingsObject(GetSettingsPath());
	if (!Root.IsValid())
	{
		return DefaultMcpServerPort;
	}

	const TSharedPtr<FJsonObject>* McpServers = nullptr;
	if (!Root->TryGetObjectField(TEXT("mcpServers"), McpServers) || McpServers == nullptr)
	{
		return DefaultMcpServerPort;
	}

	const TSharedPtr<FJsonObject>* UnrealMcp = nullptr;
	if (!(*McpServers)->TryGetObjectField(TEXT("UnrealMCP"), UnrealMcp) || UnrealMcp == nullptr)
	{
		return DefaultMcpServerPort;
	}

	FString Url;
	if (!(*UnrealMcp)->TryGetStringField(TEXT("url"), Url))
	{
		return DefaultMcpServerPort;
	}

	return ParsePortFromUrl(Url, DefaultMcpServerPort);
}

uint32 FUnrealAgentSettings::GetFrontendPort()
{
	const TSharedPtr<FJsonObject> Root = LoadSettingsObject(GetSettingsPath());
	if (!Root.IsValid())
	{
		return DefaultFrontendPort;
	}

	int32 Port = 0;
	if (Root->TryGetNumberField(TEXT("frontendPort"), Port) && Port > 0)
	{
		return static_cast<uint32>(Port);
	}

	return DefaultFrontendPort;
}
