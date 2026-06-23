using System.Text.Json;

namespace UnrealAgent.Backend.Core;

/// <summary>
/// settings.local.json에서 런타임 설정(포트 등)을 읽는 정적 헬퍼입니다.
/// 포트의 단일 출처는 이 파일이며, UE 플러그인(C++)도 같은 파일을 읽습니다.
/// 파일이나 키가 없으면 기본값으로 폴백하여 기존 동작을 보존합니다.
/// </summary>
public static class AppSettings
{
    /// <summary>프론트엔드(Agent Server) 기본 포트입니다.</summary>
    public const int DefaultFrontendPort = 55558;

    /// <summary>
    /// 프론트엔드 포트를 반환합니다.
    /// settings.local.json의 frontendPort 키를 읽으며, 없으면 DefaultFrontendPort를 반환합니다.
    /// </summary>
    public static int GetFrontendPort()
    {
        string SettingsPath = Path.Combine(AgentPaths.ConfigDir, "settings.local.json");

        if (!File.Exists(SettingsPath))
            return DefaultFrontendPort;

        try
        {
            using JsonDocument Doc = JsonDocument.Parse(File.ReadAllText(SettingsPath));

            if (Doc.RootElement.TryGetProperty("frontendPort", out JsonElement Port)
                && Port.TryGetInt32(out int Value)
                && Value > 0)
            {
                return Value;
            }
        }
        catch
        {
            // 파싱 실패 시 기본값으로 폴백합니다.
        }

        return DefaultFrontendPort;
    }

    /// <summary>
    /// dev-block 모드 여부를 반환합니다. settings.local.json의 devBlockMode 키(bool)를 읽으며,
    /// 키가 없거나 파일이 없으면 false입니다. 개발 단계 전용 토글로, 켜지면 에이전트가 도구 한계에
    /// 막혔을 때 우회하지 않고 기록 후 중단합니다.
    /// </summary>
    public static bool IsDevBlockModeEnabled()
    {
        string SettingsPath = Path.Combine(AgentPaths.ConfigDir, "settings.local.json");

        if (!File.Exists(SettingsPath))
            return false;

        try
        {
            using JsonDocument Doc = JsonDocument.Parse(File.ReadAllText(SettingsPath));

            return Doc.RootElement.TryGetProperty("devBlockMode", out JsonElement Value)
                   && Value.ValueKind == JsonValueKind.True;
        }
        catch
        {
            // 파싱 실패 시 비활성으로 폴백합니다.
            return false;
        }
    }
}
