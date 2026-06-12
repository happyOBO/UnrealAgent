using UnrealAgent.Backend.Model.Attributes;

namespace UnrealAgent.Backend.Model.Models;

/// <summary>
/// Claude Fable 5 모델 정의입니다.
/// 확장 사고가 항상 켜져 있으며 끌 수 없습니다.
/// </summary>
[AgentModel(Order = 0)]
public sealed class Fable5 : IModel
{
    public const string ModelId = "claude-fable-5";
    public string Id => ModelId;
    public string DisplayName => "Fable 5";
    public string Description => "가장 까다로운 추론과 장기 에이전트 작업을 위한 최고 성능 모델입니다.";
    public int MaxOutputTokens => 128_000;
    public int ContextWindow => 1_000_000;
    public ThinkingSupport Thinking => ThinkingSupport.AlwaysOn;
}
