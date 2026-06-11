using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.Conversation;

namespace UnrealAgent.Backend.Agent.Middleware;

/// <summary>
/// 미들웨어 체인을 조립하여 실행하는 에이전트 파이프라인입니다.
/// 미들웨어는 Use() 호출 순서대로 실행됩니다.
/// </summary>
public sealed class AgentPipeline
{
    private readonly List<IAgentMiddleware> Middlewares = [];
    private AgentDelegate? Pipeline;
    
    /// <summary>미들웨어를 파이프라인에 추가합니다.</summary>
    public AgentPipeline Use(IAgentMiddleware Middleware)
    {
        Middlewares.Add(Middleware);
        return this;
    }
    
    /// <summary>파이프라인의 최종 단계(에이전트 루프)를 설정하고 빌드합니다.</summary>
    public AgentPipeline Run(AgentDelegate Terminal)
    {
        AgentDelegate Current = Terminal;

        // 역순으로 체이닝합니다. 마지막 미들웨어의 Next가 Terminal을 가리킵니다.
        for (int I = Middlewares.Count - 1; I >= 0; I--)
        {
            Middlewares[I].SetNext(Current);
            Current = Middlewares[I].InvokeAsync;
        }

        Pipeline = Current;
        return this;
    }
    
    /// <summary>파이프라인을 실행합니다.</summary>
    public IAsyncEnumerable<ChatEvent> RunAsync(UserInput Input, AgentSession Session, CancellationToken Ct)
        => (Pipeline ?? throw new InvalidOperationException("Run()으로 파이프라인을 빌드해야 합니다."))
            (Input, Session, Ct);
}