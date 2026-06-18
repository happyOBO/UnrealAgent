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

    /// <summary>
    /// 어시스턴트 메시지 1건의 입력측 컨텍스트 사용량입니다 (input + cache_read + cache_creation).
    /// 해당 요청 시점의 실제 컨텍스트 크기를 나타내며, 턴 내 마지막 값이 현재 컨텍스트 총량입니다.
    /// </summary>
    public sealed record ContextUsage(long ContextTokens) : ClaudeStreamItem;

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

    /// <summary>
    /// 턴 종료(result 이벤트)입니다. 비용/출력 사용량을 담습니다.
    /// 주의: result의 토큰 usage는 턴 내 모든 모델 호출의 <b>누적</b>값이므로
    /// 현재 컨텍스트 크기로 사용하면 안 됩니다 (그 용도로는 <see cref="ContextUsage"/>를 사용).
    /// </summary>
    public sealed record Result(
        bool IsError, string? Text,
        long InputTokens, long OutputTokens,
        long CacheReadTokens, long CacheCreationTokens,
        double CostUsd) : ClaudeStreamItem;

    /// <summary>실행/파싱 실패입니다 (CLI 미발견, 프로세스 오류 등).</summary>
    public sealed record Failure(string Message) : ClaudeStreamItem;

    /// <summary>
    /// 턴을 끝내지 않는 정보성 알림입니다 (레이트리밋/지연 등).
    /// AgentLoop가 <c>ChatEvent.System</c>으로 표면화해 사용자가 "왜 멈춘 듯한가"를 알 수 있게 합니다.
    /// </summary>
    public sealed record Notice(string Message) : ClaudeStreamItem;
}
