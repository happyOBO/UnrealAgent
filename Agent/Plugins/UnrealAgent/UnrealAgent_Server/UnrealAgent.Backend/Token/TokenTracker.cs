using Anthropic.Models.Messages;

using UnrealAgent.Backend.Auth;
using UnrealAgent.Backend.Model;
using UnrealAgent.Backend.Prompt;
using UnrealAgent.Backend.Tool;

namespace UnrealAgent.Backend.Token;

/// <summary>
/// 시스템 프롬프트와 도구 정의의 고정 토큰을 측정하고 캐싱합니다.
/// 모델별로 캐싱하여 모델 전환 시에도 안전합니다.
/// </summary>
public class TokenTracker(AuthConfig Auth, PromptBuilder PromptBuilder, ToolRegistry ToolRegistry, ModelSettings ModelSettings)
{
    /// <summary>고정 토큰 측정값입니다.</summary>
    public sealed record FixedTokens(long SystemPrompt, long UnrealAgentMd, long Skills, long Tools);
    
    /// <summary>현재 고정 토큰 측정값입니다.</summary>
    public FixedTokens? Fixed { get; private set; }
    
    /// <summary>
    /// 카테고리별 컨텍스트 사용량을 계산합니다.
    /// </summary>
    public TokenUsage GetTokenUsage(long ContextTokens)
    {
        long SystemTokens = Fixed?.SystemPrompt ?? 0;
        long UnrealAgentMdTokens = Fixed?.UnrealAgentMd ?? 0;
        long SkillTokens = Fixed?.Skills ?? 0;
        long Tools = Fixed?.Tools ?? 0;
        long Messages = Math.Max(0, ContextTokens - SystemTokens - UnrealAgentMdTokens - SkillTokens - Tools);

        return new TokenUsage(SystemTokens, UnrealAgentMdTokens, SkillTokens, Tools, Messages, ModelSettings.ContextWindow);
    }

    /// <summary>Count Tokens API로 고정 토큰을 측정합니다.</summary>
    public async Task MeasureAsync()
    {
        if (Auth.Client is null || Fixed is not null)
        {
            return;
        }

        List<MessageParam> DummyMessages =
        [
            new() { Role = Role.User, Content = "." }
        ];

        // 1) 기준선: 더미 메시지만 포함합니다.
        MessageTokensCount Baseline = await Auth.Client.Messages.CountTokens(new MessageCountTokensParams
        {
            Model = ModelSettings.Model,
            Messages = DummyMessages
        });

        // 2) 시스템 프롬프트만 (UnrealAgentMd, Skills 제외)
        MessageTokensCount SystemOnly = await Auth.Client.Messages.CountTokens(new MessageCountTokensParams
        {
            Model = ModelSettings.Model,
            Messages = DummyMessages,
            System = PromptBuilder.BuildWithout(PromptBuilder.Section.UnrealAgentMd | PromptBuilder.Section.Skills)
        });

        // 3) UnrealAgent.md만
        MessageTokensCount MdOnly = await Auth.Client.Messages.CountTokens(new MessageCountTokensParams
        {
            Model = ModelSettings.Model,
            Messages = DummyMessages,
            System = PromptBuilder.BuildOnly(PromptBuilder.Section.UnrealAgentMd)
        });

        // 4) Skills만
        MessageTokensCount SkillsOnly = await Auth.Client.Messages.CountTokens(new MessageCountTokensParams
        {
            Model = ModelSettings.Model,
            Messages = DummyMessages,
            System = PromptBuilder.BuildOnly(PromptBuilder.Section.Skills)
        });

        // 5) 도구만
        MessageTokensCount ToolsOnly = await Auth.Client.Messages.CountTokens(new MessageCountTokensParams
        {
            Model = ModelSettings.Model,
            Messages = DummyMessages,
            Tools = ToolRegistry.GetAllSchemas().Select(S => (MessageCountTokensTool)S).ToList()
        });
        
        Fixed = new FixedTokens(
            SystemPrompt: SystemOnly.InputTokens - Baseline.InputTokens,
            UnrealAgentMd: MdOnly.InputTokens - Baseline.InputTokens,
            Skills: SkillsOnly.InputTokens - Baseline.InputTokens,
            Tools: ToolsOnly.InputTokens - Baseline.InputTokens);
    }
}