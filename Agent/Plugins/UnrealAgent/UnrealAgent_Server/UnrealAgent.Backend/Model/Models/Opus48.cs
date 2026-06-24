using UnrealAgent.Backend.Model.Attributes;

namespace UnrealAgent.Backend.Model.Models;

/// <summary>
/// Claude Opus 4.8 모델 정의입니다.
/// </summary>
[AgentModel(Order = 1)]
public sealed class Opus48 : IModel
{
    public const string ModelId = "claude-opus-4-8";
    public string Id => ModelId;
    public string DisplayName => "Opus 4.8";
    public string Description => "에이전트 구축과 코딩에 최적화된 최신 최고 지능 모델입니다.";
    public int MaxOutputTokens => 128_000;
    public int ContextWindow => 200_000;
}
