using System.Runtime.CompilerServices;
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
    ContextRegistry ContextRegistry)
{
    /// <summary>
    /// dev-block 모드에서 세션 복원 직후 첫 턴에 1회 주입되는 재개-넛지입니다.
    /// 복원된 대화는 --resume으로 이미 들어와 있으므로, 막혔던 작업이 있으면 먼저 재개를 제안하게 합니다.
    /// </summary>
    private const string ResumeCheckReminder = """
        <system-reminder>
        A previous session was just restored and dev-block mode is active. That prior
        session may have stopped because a native MCP tool's capability was blocked.
        Before doing anything else: list the TOP-LEVEL `.unrealagent/dev-blocked/` folder
        (with `Bash`/`Glob`) and read any `status: blocked` record(s) there. EXCLUDE the
        `.unrealagent/dev-blocked/resolved/` subfolder — those are already done. If one or
        more open records represent unfinished work, briefly summarize what was blocked
        (tool + operation) for each and ask the user — in their own language — whether to
        resume now that the tool may be fixed, BEFORE proceeding. If there are no open
        (`status: blocked`) records at the top level, just continue with the user's request
        as normal.
        </system-reminder>
        """;

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

            // dev-block 모드에서 세션 복원 직후 첫 턴이면 재개-넛지를 1회 주입합니다.
            string? ResumeNudge = null;
            if (Session.bResumeCheckPending)
            {
                ResumeNudge = ResumeCheckReminder;
                Session.bResumeCheckPending = false;
            }

            string? Injected = (ResumeNudge, Context) switch
            {
                (not null, not null) => ResumeNudge + "\n\n" + Context,
                (not null, null)     => ResumeNudge,
                (null, not null)     => Context,
                _                    => null,
            };

            if (Injected is not null)
                Input = Input with { InjectedContext = Injected };
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
            AllowCliSlashCommands = Input.bCliCommand,
        };

        // tool_use_id → 도구 이름 (tool_result에 이름을 채우기 위함)
        Dictionary<string, string> ToolNames = new();

        // 턴 내 마지막 어시스턴트 메시지의 컨텍스트 총량 (현재 컨텍스트 크기).
        long LastContextTokens = 0;

        // 이번 턴에 사용자에게 보일 산출물(텍스트/도구 호출)이 하나라도 방출됐는지 추적합니다.
        // 스트리밍 콘텐츠 없이 result만으로 끝나는 턴(빈 응답/에러 result/resume 실패)에서
        // 사용자가 "왜 답장이 없는지" 알 수 있도록, result 처리 시 이 플래그로 분기합니다.
        bool bProducedOutput = false;

        await using ClaudeCodeRunner Runner = new(CliPath);

        await foreach (ClaudeStreamItem Item in Runner.RunAsync(Options, Input, Ct))
        {
            switch (Item)
            {
                case ClaudeStreamItem.Init Init:
                    Session.ClaudeSessionId = Init.SessionId;
                    break;

                case ClaudeStreamItem.AssistantText Text:
                    bProducedOutput = true;
                    yield return new ChatEvent.Assistant(Text.Text);
                    break;

                case ClaudeStreamItem.Thinking Thinking:
                    yield return new ChatEvent.Thinking(Thinking.Text);
                    break;

                case ClaudeStreamItem.ContextUsage Context:
                    LastContextTokens = Context.ContextTokens;
                    break;

                case ClaudeStreamItem.ToolUse Tool:
                    bProducedOutput = true;
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
                    // 스트리밍 콘텐츠 없이 result만으로 끝난 턴은 사용자에게 침묵으로 보입니다.
                    // (resume 실패/에러 result/빈 응답 등) result의 IsError/Text를 표면화하여
                    // "버튼만 돌다 답장 없음"을 없애고 상위 원인을 그대로 노출합니다.
                    if (Done.IsError)
                    {
                        yield return new ChatEvent.System(string.IsNullOrWhiteSpace(Done.Text)
                            ? "에이전트가 오류로 턴을 종료했습니다(빈 응답)."
                            : Done.Text);
                    }
                    else if (!bProducedOutput)
                    {
                        yield return string.IsNullOrWhiteSpace(Done.Text)
                            ? new ChatEvent.System(
                                "응답 없이 턴이 종료되었습니다. (--resume 세션 상태 또는 모델 빈 응답)")
                            : new ChatEvent.Assistant(Done.Text);
                    }

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
