using System.Runtime.CompilerServices;
using UnrealAgent.Backend.Auth;
using UnrealAgent.Backend.Chat;
using UnrealAgent.Backend.ClaudeCode;
using UnrealAgent.Backend.Conversation;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Model;
using UnrealAgent.Backend.Prompt;
using UnrealAgent.Backend.Prompt.Context;
using UnrealAgent.Backend.Security;

namespace UnrealAgent.Backend.Agent;

/// <summary>
/// 에이전트 루프입니다.
/// claude CLI(Claude Code)를 stream-json 모드로 구동하고, NDJSON 이벤트를 ChatEvent로 변환합니다.
/// 에이전트 지능(도구 실행/대화 관리)은 CLI가 소유하며, 본 루프는 오케스트레이션과 권한 중계만 담당합니다.
/// </summary>
public sealed class AgentLoop(
    PromptBuilder PromptBuilder,
    ModelSettings ModelSettings,
    ClaudeCliLocator Locator,
    CliMcpConfig McpConfig,
    AuthConfig Auth,
    ContextRegistry ContextRegistry)
{
    /// <summary>
    /// 사용자 메시지 1건에 대한 에이전트 루프를 실행합니다.
    /// 호출 1회가 claude CLI 프로세스 1개(1턴)에 대응하며, 세션은 --resume으로 이어집니다.
    /// </summary>
    public async IAsyncEnumerable<ChatEvent> RunAsync(UserInput Input, AgentSession Session, [EnumeratorCancellation] CancellationToken Ct = default)
    {
        string? CliPath = Locator.Find();
        if (CliPath is null)
        {
            yield return new ChatEvent.System(
                "claude CLI를 찾을 수 없습니다. 'npm install -g @anthropic-ai/claude-code'로 설치하세요.");
            yield return new ChatEvent.Done();
            yield break;
        }

        // 사용자 메시지 키워드에 맞는 UE 도메인 컨텍스트를 골라 이 턴에 주입합니다.
        // CLI 내장 커맨드(/compact 등) 턴은 주입하지 않습니다.
        if (!Input.bCliCommand && Input.InjectedContext is null)
        {
            string? Context = ContextRegistry.MatchByKeywords(Input.Text);
            if (Context is not null)
                Input = Input with { InjectedContext = Context };
        }

        ClaudeRunOptions Options = new()
        {
            SystemPrompt = PromptBuilder.BuildSystemPrompt(Session),
            Model = ModelSettings.Model,
            Effort = ModelSettings.bSupportsEffort ? ModelSettings.GetCliEffort() : null,
            ThinkingEnabled = ModelSettings.GetEffectiveThinking(),
            Mode = Session.Mode,
            McpConfigPath = McpConfig.Path,
            ResumeSessionId = Session.ClaudeSessionId,
            WorkingDirectory = string.IsNullOrEmpty(AgentPaths.RootPath) ? null : AgentPaths.RootPath,
            ApiKey = Auth.ApiKey,
            AllowCliSlashCommands = Input.bCliCommand,
        };

        // tool_use_id → 도구 이름 (tool_result에 이름을 채우기 위함)
        Dictionary<string, string> ToolNames = new();

        // 턴 내 마지막 어시스턴트 메시지의 컨텍스트 총량 (현재 컨텍스트 크기).
        long LastContextTokens = 0;

        await using ClaudeCodeRunner Runner = new(CliPath);

        await foreach (ClaudeStreamItem Item in Runner.RunAsync(Options, Input, Ct))
        {
            switch (Item)
            {
                case ClaudeStreamItem.Init Init:
                    Session.ClaudeSessionId = Init.SessionId;
                    break;

                case ClaudeStreamItem.AssistantText Text:
                    yield return new ChatEvent.Assistant(Text.Text);
                    break;

                case ClaudeStreamItem.Thinking Thinking:
                    yield return new ChatEvent.Thinking(Thinking.Text);
                    break;

                case ClaudeStreamItem.ContextUsage Context:
                    LastContextTokens = Context.ContextTokens;
                    break;

                case ClaudeStreamItem.ToolUse Tool:
                    ToolNames[Tool.Id] = Tool.Name;
                    yield return new ChatEvent.ToolStart(Tool.Id, Tool.Name, Tool.InputJson);
                    break;

                case ClaudeStreamItem.ToolResult Result:
                    string Name = ToolNames.GetValueOrDefault(Result.ToolUseId, "");
                    yield return new ChatEvent.ToolEnd(Result.ToolUseId, Name, Result.Content);
                    break;

                case ClaudeStreamItem.Permission Perm:
                {
                    // 권한 엔진 판정: Edit/팀/허용목록은 자동 허용, 그 외 Ask
                    Block.ToolUse ToolCall = new(Perm.ToolUseId ?? Perm.RequestId, Perm.ToolName, Perm.InputJson);
                    ToolPermission Decision = await Session.PermissionEngine.GetPermissionAsync(ToolCall, Session.Mode);

                    if (Decision == ToolPermission.Ask)
                    {
                        ChatEvent.ToolPermissionRequest Request = new(ToolCall.Id, Perm.ToolName, Perm.InputJson);
                        yield return Request;

                        Decision = await Request.Tcs.Task.WaitAsync(Ct);

                        if (Decision == ToolPermission.AlwaysAllow)
                            Session.PermissionEngine.Allow(Perm.ToolName);
                    }

                    // CLI에 control_response 전송 (runner가 Decision 설정을 기다리는 중)
                    Perm.Decision.SetResult(Decision != ToolPermission.Deny);
                    break;
                }

                case ClaudeStreamItem.Result Done:
                    // 컨텍스트 총량은 result의 누적 usage가 아니라 마지막 어시스턴트 메시지의 값을 사용합니다.
                    yield return new ChatEvent.Usage(LastContextTokens, Done.OutputTokens, Done.CostUsd);
                    yield return new ChatEvent.Done();
                    yield break;

                case ClaudeStreamItem.Notice Notice:
                    // 턴을 끝내지 않는 정보성 알림 (레이트리밋/지연 등) — System으로 표면화만 합니다.
                    yield return new ChatEvent.System(Notice.Message);
                    break;

                case ClaudeStreamItem.Failure Failure:
                    yield return new ChatEvent.System(Failure.Message);
                    yield return new ChatEvent.Done();
                    yield break;
            }
        }

        // result 없이 스트림이 끝난 경우의 안전망
        yield return new ChatEvent.Done();
    }
}
