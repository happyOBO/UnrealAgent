using UnrealAgent.Backend.Agent.Middleware;
using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.Conversation;
using UnrealAgent.Backend.Mode;
using UnrealAgent.Backend.Security;

namespace UnrealAgent.Backend.Agent;

/// <summary>
/// 에이전트 세션입니다.
/// 프로세스 아이덴티티, 대화 상태, 미들웨어 파이프라인을 통합합니다.
/// </summary>
public sealed class AgentSession
{
    /// <summary>이 세션의 대화 히스토리입니다.</summary>
    public Conversation.Conversation Conversation { get; } = new();

    /// <summary>팀 정보입니다.</summary>
    public Team.Team Team { get; } = new();
    
    /// <summary>이 세션의 도구 실행 권한 엔진입니다.</summary>
    public PermissionEngine PermissionEngine { get; } 
    
    /// <summary>현재 에이전트 모드입니다.</summary>
    public AgentMode Mode { get; set; } = AgentMode.Normal;

    /// <summary>claude CLI 세션 ID입니다. 매 턴 --resume으로 대화를 이어가며, /clear 시 초기화됩니다.</summary>
    public string? ClaudeSessionId { get; set; }

    // <summary>미들웨어 체인을 통해 메시지를 처리하는 파이프라인입니다.</summary>
    private readonly AgentPipeline Pipeline;
    
    public AgentSession(AgentLoop Loop, SlashCommandMiddleware SlashCommandMiddleware)
    {
        PermissionEngine = new PermissionEngine(Team);
        
        Pipeline = new AgentPipeline()
            .Use(SlashCommandMiddleware)
            .Run(Loop.RunAsync);
    }

    /// <summary>사용자 메시지를 처리합니다. </summary>
    public IAsyncEnumerable<ChatEvent> ProcessMessage(UserInput Input, CancellationToken Ct = default) 
        => Pipeline.RunAsync(Input, this, Ct);
}