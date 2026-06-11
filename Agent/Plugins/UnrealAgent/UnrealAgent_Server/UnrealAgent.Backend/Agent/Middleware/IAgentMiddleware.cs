using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.Conversation;

namespace UnrealAgent.Backend.Agent.Middleware;

/// <summary>
/// 에이전트 파이프라인의 다음 단계를 실행하는 델리게이트입니다.
/// </summary>
public delegate IAsyncEnumerable<ChatEvent> AgentDelegate(UserInput Input, AgentSession Session, CancellationToken Ct);

//-----------------------------------------------------------------------------
// AgentMiddleware
//-----------------------------------------------------------------------------

/// <summary>
/// 에이전트 파이프라인 미들웨어 기본 클래스입니다.
/// 요청 전후에 로직을 삽입하거나, 요청을 가로채서 단락할 수 있습니다.
/// </summary>
public abstract class IAgentMiddleware
{
    /// <summary>파이프라인의 다음 단계입니다.</summary>
    protected AgentDelegate Next { get; private set; } = null!;
    
    /// <summary>다음 단계를 설정합니다. AgentPipeline이 빌드 시 호출합니다.</summary>
    internal void SetNext(AgentDelegate Delegate) => Next = Delegate;
    
    /// <summary>미들웨어 로직을 실행합니다.</summary>
    public abstract IAsyncEnumerable<ChatEvent> InvokeAsync(UserInput Input, AgentSession Session, CancellationToken Ct);
}