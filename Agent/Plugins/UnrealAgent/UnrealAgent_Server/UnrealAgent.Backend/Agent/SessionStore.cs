using System.Text.Json;
using System.Text.Json.Serialization;
using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Mode;

namespace UnrealAgent.Backend.Agent;

/// <summary>
/// 디스크에 직렬화되는 세션 스냅샷입니다.
/// claude CLI 세션 ID(--resume용)와 UI 메시지를 함께 보존합니다.
/// </summary>
public sealed record PersistedSession
{
    /// <summary>claude CLI 세션 ID입니다. 복원 시 --resume으로 모델 컨텍스트를 이어갑니다.</summary>
    public string? ClaudeSessionId { get; init; }

    /// <summary>저장 시점의 에이전트 모드입니다.</summary>
    public AgentMode Mode { get; init; }

    /// <summary>UI에 재표시할 채팅 메시지 목록입니다.</summary>
    public List<ChatUIMessage> Messages { get; init; } = [];

    /// <summary>저장 시각(UTC)입니다.</summary>
    public DateTime SavedAt { get; init; }
}

/// <summary>
/// 세션을 디스크에 저장/복원/삭제하는 서비스입니다.
/// 에디터(백엔드 동반)가 종료되어도 다음 실행에서 직전 세션을 복원할 수 있게 합니다.
/// </summary>
public sealed class SessionStore
{
    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    /// <summary>세션 파일 경로입니다.</summary>
    private static string FilePath => AgentPaths.SessionFilePath;

    /// <summary>저장된 세션 파일이 존재하는지 여부입니다. Restore 버튼 활성화 판단에 사용합니다.</summary>
    public bool HasSaved() => File.Exists(FilePath);

    /// <summary>세션 스냅샷을 디스크에 저장합니다. 실패해도 대화 흐름을 막지 않도록 예외를 삼킵니다.</summary>
    public void Save(PersistedSession Session)
    {
        try
        {
            string? Dir = Path.GetDirectoryName(FilePath);
            if (!string.IsNullOrEmpty(Dir))
                Directory.CreateDirectory(Dir);

            string Json = JsonSerializer.Serialize(Session, Options);
            File.WriteAllText(FilePath, Json);
        }
        catch
        {
            // 저장 실패는 무시합니다(권한/디스크 이슈 등). 다음 턴에서 재시도됩니다.
        }
    }

    /// <summary>디스크에서 세션 스냅샷을 읽어옵니다. 없거나 손상되었으면 null을 반환합니다.</summary>
    public PersistedSession? Load()
    {
        try
        {
            if (!File.Exists(FilePath))
                return null;

            string Json = File.ReadAllText(FilePath);
            return JsonSerializer.Deserialize<PersistedSession>(Json, Options);
        }
        catch
        {
            return null;
        }
    }

    /// <summary>저장된 세션 파일을 삭제합니다. New Session(/clear) 시 호출됩니다.</summary>
    public void Delete()
    {
        try
        {
            if (File.Exists(FilePath))
                File.Delete(FilePath);
        }
        catch
        {
            // 삭제 실패는 무시합니다.
        }
    }
}
