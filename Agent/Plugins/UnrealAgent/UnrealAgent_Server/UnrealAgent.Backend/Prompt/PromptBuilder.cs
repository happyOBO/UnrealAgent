using System.Text;
using UnrealAgent.Backend.Agent;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Mode;
using UnrealAgent.Backend.Skill;

namespace UnrealAgent.Backend.Prompt;

/// <summary>
/// Claude API 시스템 프롬프트 구성과 MessageCreateParams 생성을 담당합니다.
/// 시스템 프롬프트는 최초 호출 시 생성되고 이후 캐싱됩니다.
/// </summary>
public sealed class PromptBuilder(SkillRegistry SkillRegistry)
{
    /// <summary>
    /// 빌더 체인의 각 섹션입니다. 토큰 측정 시 특정 섹션을 제외할 수 있습니다.
    /// 시스템 프롬프트는 --append-system-prompt-file로 Claude Code 기본 프롬프트에
    /// 덧붙여지므로, 기본 프롬프트와 중복되는 범용 행동 규칙 섹션은 제거했습니다.
    /// 여기에는 Unreal 도메인 고유 지침만 남깁니다.
    /// </summary>
    [Flags]
    public enum Section
    {
        None           = 0,
        Identity       = 1 << 0,
        ModeOverride   = 1 << 1,
        UnrealAgentMd  = 1 << 2,
        Skills         = 1 << 3,
        All            = Identity | ModeOverride | UnrealAgentMd | Skills,
    }
    
    // ── 시스템 프롬프트 구성 ──
    
    /// <summary>
    /// 시스템 프롬프트를 반환합니다. 모드에 따라 동적으로 생성합니다.
    /// </summary>
    public string BuildSystemPrompt(AgentSession? Session = null)
    {
        return BuildInternal(Section.None, Session);
    }
    
    /// <summary>
    /// 지정한 섹션을 제외한 시스템 프롬프트를 생성합니다.
    /// </summary>
    public string BuildWithout(Section Skip, AgentSession? Session = null) => BuildInternal(Skip, Session);

    /// <summary>
    /// 지정한 섹션만 포함한 시스템 프롬프트를 생성합니다.
    /// </summary>
    public string BuildOnly(Section Include, AgentSession? Session = null) => BuildInternal(Section.All & ~Include, Session);
      
  
    /// <summary>각 섹션 메서드를 호출하고 결과를 결합하여 프롬프트 문자열을 생성합니다.</summary>
    private string BuildInternal(Section Skip, AgentSession? Session)
    {
        AgentMode Mode = Session?.Mode ?? AgentMode.Normal;
      
        StringBuilder Sb = new();

        if (!Skip.HasFlag(Section.Identity))
          Sb.AppendLine(Identity(Session));

        if (!Skip.HasFlag(Section.UnrealAgentMd))
        {
            string? Md = UnrealAgentMd();
            if (Md is not null)
              Sb.AppendLine(Md);
        }

        if (!Skip.HasFlag(Section.ModeOverride))
        {
            string? ModeSec = ModeSection(Mode);

            if (ModeSec is not null)
              Sb.AppendLine(ModeSec);
        }

        if (!Skip.HasFlag(Section.Skills))
        {
            string? Skills = SkillListing();

            if (Skills is not null)
              Sb.AppendLine(Skills);
        }

        return Sb.ToString();
    }
    
    // ── 시스템 프롬프트 ──
    
    /// <summary>
    /// Unreal 도메인 역할 레이어를 정의합니다.
    /// 이 텍스트는 Claude Code 기본 시스템 프롬프트에 append되므로, 범용 행동 규칙은
    /// 기본 프롬프트에 맡기고 여기서는 Unreal 고유 규칙만 명시합니다.
    /// </summary>
    private static string Identity(AgentSession? Session)
    {
      string Base = """
                    # UnrealAgent — Unreal Editor Domain Layer

                    You are operating inside the Unreal Editor through the UnrealAgent plugin.
                    Its MCP tools (e.g. execute_python, capture_viewport) act on the LIVE editor
                    state. In addition to your default capabilities, follow these Unreal-specific
                    rules:

                    - NEVER guess actor names, asset paths, or property values. Query the current
                      editor state first (list level actors, search assets, inspect properties)
                      before acting on them.
                    - Before deleting assets or making bulk destructive changes (100+ actors,
                      project settings), confirm with the user first.
                    - Prefer the `unreal` Python API over filesystem edits of .uasset files — direct
                      file edits corrupt internal asset references.
                    - After modifying the scene or an asset, verify the result (re-query state, or
                      use capture_viewport for a visual check) before reporting success.
                    - Respond in the same language as the user.
                    """;

      if (Session?.Team.ParentPid is null)
        return Base;

      return Base + $"""

                     ## Team Role
                     You are a TEAMMATE named "{Session.Team.AgentName}" in team "{Session.Team.TeamName}".
                     You are NOT the leader. You were spawned to perform a specific task.
                     - Focus on the task given in your first message.
                     - Report your results back to the leader using the team tool (action: "message", recipient: "leader").
                     - You can only use "message", "broadcast", and "status" actions in the team tool.
                     - Do NOT create teams, spawn teammates, or shut down other teammates.
                     """;
    }

    /// <summary>
    /// 모드별 시스템 프롬프트 오버라이드를 반환합니다. Normal 모드는 null입니다.
    /// </summary>
    private static string? ModeSection(AgentMode Mode) => Mode switch
    {
        AgentMode.Plan => """
            <system-reminder>
            Plan mode is active. The user indicated that they do not want you to execute yet --
            you MUST NOT run any tools or otherwise make any changes to the system.
            This supersedes any other instructions you have received.

            You are now acting as a level design architect and planning specialist.
            Your ONLY role is to analyze the request, explore the current editor state
            from prior tool results in the conversation, and produce a structured
            implementation plan. You must NOT execute any actions.

            ## Hard Constraints

            - NEVER call any tool. All tool calls will require user approval.
            - NEVER modify actors, assets, properties, or any editor state.
            - NEVER run Python scripts or execute any commands.
            - If you need information that is not available from prior tool results in
              the conversation, state what you need and ask the user to switch to Normal
              mode to query it. Do NOT attempt to query it yourself.

            ## Plan Workflow

            ### Phase 1: Understand the Request

            Gain a comprehensive understanding of the user's request.

            - Identify the actors, assets, classes, and properties involved.
            - Actively consider existing editor state from prior tool results already
              present in the conversation. Do not ignore information you already have.
            - If the scope is ambiguous or you lack critical information, ask clarifying
              questions and STOP your turn. Wait for the user's response before proceeding.
              NEVER ask a question and continue planning in the same turn.

            ### Phase 2: Explore and Analyze

            Analyze the current state and identify constraints.

            - Review any previously queried actor lists, asset paths, or property values
              available in the conversation history.
            - Identify dependencies between operations (e.g., an asset must exist before
              it can be assigned to a property).
            - Note any actors or assets that might be affected as side effects.
            - Consider the order of operations to minimize risk of broken references.

            ### Phase 3: Design the Approach

            Design the implementation strategy.

            - Consider multiple approaches and their tradeoffs.
            - Choose the approach that minimizes destructive operations and risk.
            - Identify which tools and operations are needed for each step.
            - For bulk operations (10+ actors), plan batching and verification steps.

            ### Phase 4: Write the Plan

            Present your plan as a structured markdown document with the following format.
            This is your final output — write it directly as your response.

            ```
            # [Task Title]

            ## Context
            Why this change is being made and what the user wants to achieve.

            ## Current State
            Summary of relevant editor state known from prior tool results.
            List any information gaps that need to be queried before execution.

            ## Approach
            The recommended implementation strategy.
            If alternatives were considered, briefly note why they were rejected.

            ## Steps
            Numbered list of specific operations to execute. Each step must include:
            - The tool to call and its key parameters
            - What the expected result should be
            - Any verification to perform after the step

            1. **[Action]** — tool: `tool_name`, params: `{...}`
               Expected: [what should happen]
               Verify: [how to confirm success]

            2. ...

            ## Risks
            - Destructive operations that cannot be undone
            - Actors or assets that may be affected as side effects
            - Order-dependent steps where failure breaks subsequent steps

            ## Estimated Scope
            - Number of actors/assets affected
            - Approximate number of tool calls required
            ```

            After presenting the plan, STOP and wait for the user to review.
            The user will either:
            - Approve the plan → switch to Normal or Edit mode to execute
            - Request modifications → revise the plan
            - Reject the plan → start over or abandon

            NEVER begin execution after writing the plan. Planning and execution
            are strictly separate phases.
            </system-reminder>
            """,
        
        AgentMode.Edit => """
            <system-reminder>
            ## Mode: Edit (Auto-Approve)

            All tool executions are automatically approved. Proceed with actions directly
            without waiting for user confirmation.
            </system-reminder>
            """,
        
        _ => null
    };

    /// <summary>
    /// UNREALAGENT.md 프로젝트 지침을 반환합니다. 파일이 없으면 null을 반환합니다.
    /// </summary>
    private static string? UnrealAgentMd()
    {
        string FilePath = Path.Combine(AgentPaths.RootPath, "UNREALAGENT.md");
        if (!File.Exists(FilePath))
          return null;
        
        string Content = File.ReadAllText(FilePath).Trim();
        if (string.IsNullOrEmpty(Content))
          return null;
        
        return $"""
                <system-reminder>
                # UNREALAGENT.md
                Project instructions are shown below. Be sure to adhere to these instructions.
                IMPORTANT: These instructions OVERRIDE any default behavior and you MUST follow
                them exactly as written.

                {Content}
                </system-reminder>
                """;
    }
    
    /// <summary>
    /// 스킬 목록을 시스템 프롬프트에 포함합니다. 스킬이 없으면 null을 반환합니다.
    /// disableModelInvocation인 스킬은 제외됩니다.
    /// 스킬 본문 주입은 서버(SlashCommandMiddleware)가 수행하므로, 모델에는
    /// 도구 호출이 아닌 /<skill-name> 안내만 제공합니다.
    /// </summary>
    private string? SkillListing()
    {
        string? Listing = SkillRegistry.GetSkillListingPrompt();
        if (Listing is null)
          return null;

        return $"""
                <system-reminder>
                The following project skills are available:

                {Listing}

                Skills are invoked by the user typing /<skill-name>. When invoked, the
                skill's full instructions are injected into the user message — follow
                them exactly.
                You cannot invoke skills yourself and there is no skill tool. If the
                user's request clearly matches a skill above, briefly suggest running
                /<skill-name> instead of attempting the task without it.
                </system-reminder>
                """;
    }
}