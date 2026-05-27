using UnrealAgent.Backend.Core;

namespace UnrealAgent.Backend.Conversation;

/// <summary>
/// Claude API 호출 1회의 결과입니다.
/// 어시스턴트 응답 블록과 도구 실행 결과를 포함합니다.
/// </summary>
public sealed class AssistantSpan
{
    /// <summary>어시스턴트 응답 블록 목록입니다.</summary>
    public required IReadOnlyList<Block> AssistantBlocks { get; init; }
}