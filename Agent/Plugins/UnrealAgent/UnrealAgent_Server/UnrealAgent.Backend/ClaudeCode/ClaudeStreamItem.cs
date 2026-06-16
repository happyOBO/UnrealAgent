namespace UnrealAgent.Backend.ClaudeCode;

/// <summary>
/// claude CLI의 stream-json 출력을 파싱한 도메인 이벤트입니다.
/// AgentLoop가 이를 ChatEvent로 변환합니다.
/// </summary>
public abstract record ClaudeStreamItem
{
    /// <summary>세션 초기화입니다. session_id를 담습니다 (--resume에 사용).</summary>
    public sealed record Init(string SessionId) : ClaudeStreamItem;

    /// <summary>어시스턴트 텍스트 블록입니다.</summary>
    public sealed record AssistantText(string Text) : ClaudeStreamItem;

    /// <summary>확장 사고 블록입니다.</summary>
    public sealed record Thinking(string Text) : ClaudeStreamItem;

    /// <summary>도구 호출 블록입니다.</summary>
    public sealed record ToolUse(string Id, string Name, string InputJson) : ClaudeStreamItem;

    /// <summary>도구 실행 결과입니다 (user 메시지의 tool_result).</summary>
    public sealed record ToolResult(string ToolUseId, string Content, bool IsError) : ClaudeStreamItem;

    /// <summary>
    /// 도구 사용 권한 요청입니다 (control_request can_use_tool).
    /// 소비자가 <see cref="Decision"/>에 허용/거부를 설정하면 러너가 control_response를 전송합니다.
    /// </summary>
    public sealed record Permission(string RequestId, string ToolName, string InputJson, string? ToolUseId)
        : ClaudeStreamItem
    {
        /// <summary>허용(true)/거부(false) 결정입니다. 소비자(AgentLoop)가 설정합니다.</summary>
        public TaskCompletionSource<bool> Decision { get; } =
            new(TaskCreationOptions.RunContinuationsAsynchronously);
    }

    /// <summary>턴 종료(result 이벤트)입니다. 토큰/비용 사용량을 담습니다.</summary>
    public sealed record Result(
        bool IsError, string? Text,
        long InputTokens, long OutputTokens,
        long CacheReadTokens, long CacheCreationTokens,
        double CostUsd) : ClaudeStreamItem
    {
        /// <summary>입력측 컨텍스트 총량입니다 (input + cache_read + cache_creation).</summary>
        public long ContextTokens => InputTokens + CacheReadTokens + CacheCreationTokens;
    }

    /// <summary>실행/파싱 실패입니다 (CLI 미발견, 프로세스 오류 등).</summary>
    public sealed record Failure(string Message) : ClaudeStreamItem;
}
