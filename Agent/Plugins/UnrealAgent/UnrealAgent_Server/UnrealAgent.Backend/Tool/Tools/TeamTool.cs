using System.ComponentModel;
using System.Text;
using System.Text.Json.Serialization;
using UnrealAgent.Backend.Agent;
using UnrealAgent.Backend.Team;
using UnrealAgent.Backend.Tool.Attributes;

namespace UnrealAgent.Backend.Tool.Tools;

/// <summary>
/// 에이전트 팀 관리 도구입니다.
/// 팀 생성/삭제, 팀원 스폰/종료, 메시지 전송을 하나의 도구로 처리합니다.
/// </summary>
[AgentTool("team", """
                   Manage agent teams for parallel work.

                   IMPORTANT: Only create a team when the user explicitly asks.
                   Do NOT create teams on your own initiative.

                   ## Before Creating a Team
                   1. Present a team plan to the user:
                      - How many teammates and what each will do
                      - How tasks are divided to avoid conflicts
                   2. Wait for user approval, then proceed.

                   ## Actions
                   - "create": Create a new team. Requires 'name'.
                     Name is used as a directory name — use only alphanumeric, hyphen, underscore (e.g. "my-team").
                   - "spawn": Spawn a teammate. Requires 'name', 'prompt'.
                     'prompt' defines the teammate's role and task (e.g. who they are, what to do, constraints).
                     Sent as the first message on spawn. MUST be written in English.
                   - "message": Send a message to a teammate. Requires 'recipient', 'content'.
                   - "broadcast": Send a message to ALL teammates. Requires 'content'.
                   - "shutdown": Shut down a teammate. Requires 'name'.
                   - "delete": Shut down all teammates and delete the team.
                   - "status": Show current team status.

                   ## Permissions
                   - Leader only: create, spawn, broadcast, shutdown, status, delete
                   - Everyone: message

                   ## Workflow
                   1. "create" a team.
                   2. "spawn" teammates with clear prompts.
                   3. Teammates report via messages injected into your conversation.
                   4. When work is done, ask the user before deleting — they may have more tasks.
                   """)]
public class TeamTool : AgentTool<TeamTool.Input>
{
    /// <summary>team 도구의 입력 파라미터입니다.</summary>
    public sealed record Input(
        [property: JsonPropertyName("action")]
        [property: Description("create | spawn | message | broadcast | shutdown | delete | status")]
        string Action,

        [property: JsonPropertyName("name")]
        [property: Description("Team name (create) or teammate name (spawn/shutdown)")]
        string? Name = null,

        [property: JsonPropertyName("prompt")]
        [property: Description("Teammate's role and task definition (spawn only, English)")]
        string? Prompt = null,

        [property: JsonPropertyName("recipient")]
        [property: Description("Teammate name to send the message to (message only)")]
        string? Recipient = null,

        [property: JsonPropertyName("content")]
        [property: Description("Message body (message/broadcast)")]
        string? Content = null);
    
    /// <summary>액션에 따라 팀 관련 작업을 실행합니다.</summary>
    protected override Task<ToolResult> ExecuteAsync(Input Args, AgentSession Session, CancellationToken Ct)
    {
        Team.Team Team = Session.Team;
        bool IsLeader = Team.ParentPid is null;

        return Args.Action.ToLowerInvariant() switch
        {
            "create"    => IsLeader ? CreateTeam(Team, Args) : LeaderOnly(),
            "spawn"     => IsLeader ? SpawnTeammate(Team, Args) : LeaderOnly(),
            "shutdown"  => IsLeader ? ShutdownTeammate(Team, Args) : LeaderOnly(),
            "delete"    => IsLeader ? DeleteTeam(Team) : LeaderOnly(),
            "broadcast" => IsLeader ? Broadcast(Team, Args) : LeaderOnly(),
            "status"    => IsLeader ? Task.FromResult(GetStatus(Team)) : LeaderOnly(),
            "message"   => SendMessage(Team, Args),
            
            _ => Task.FromResult(ToolResult.Error(
                $"Unknown action: '{Args.Action}'. Use: create, spawn, message, broadcast, shutdown, delete, status"))
        };
    }
    
    // ── 리더 전용 ──

    /// <summary>새 팀을 생성합니다.</summary>
    private static Task<ToolResult> CreateTeam(Team.Team Team, Input Args)
    {
        if (string.IsNullOrWhiteSpace(Args.Name))
            return Task.FromResult(ToolResult.Error("'name' is required for create."));

        try
        {
            Team.CreateTeam(Args.Name);
            return Task.FromResult(ToolResult.Success($"Team '{Args.Name}' created."));
        }
        catch (InvalidOperationException Ex)
        {
            return Task.FromResult(ToolResult.Error(Ex.Message));
        }
    }
    
    /// <summary>팀원 프로세스를 스폰합니다.</summary>
    private static async Task<ToolResult> SpawnTeammate(Team.Team Team, Input Args)
    {
        if (string.IsNullOrWhiteSpace(Args.Name))
            return ToolResult.Error("'name' is required for spawn.");
        
        if (string.IsNullOrWhiteSpace(Args.Prompt))
            return ToolResult.Error("'prompt' is required for spawn.");

        try
        {
            await Team.SpawnTeammateAsync(Args.Name, Args.Prompt);
            return ToolResult.Success($"Teammate '{Args.Name}' spawned.");
        }
        catch (InvalidOperationException Ex)
        {
            return ToolResult.Error(Ex.Message);
        }
    }
    
    /// <summary>특정 팀원 프로세스를 종료합니다.</summary>
    private static async Task<ToolResult> ShutdownTeammate(Team.Team Team, Input Args)
    {
        if (string.IsNullOrWhiteSpace(Args.Name))
            return ToolResult.Error("'name' is required for shutdown.");

        try
        {
            await Team.ShutdownTeammateAsync(Args.Name);
            return ToolResult.Success($"Teammate '{Args.Name}' shut down.");
        }
        catch (InvalidOperationException Ex)
        {
            return ToolResult.Error(Ex.Message);
        }
    }
    
    /// <summary>팀 전체를 삭제합니다.</summary>
    private static async Task<ToolResult> DeleteTeam(Team.Team Team)
    {
        try
        {
            await Team.DeleteTeamAsync();
            return ToolResult.Success("Team deleted.");
        }
        catch (InvalidOperationException Ex)
        {
            return ToolResult.Error(Ex.Message);
        }
    }
    
    /// <summary>모든 팀원에게 메시지를 브로드캐스트합니다.</summary>
    private static async Task<ToolResult> Broadcast(Team.Team Team, Input Args)
    {
        if (string.IsNullOrWhiteSpace(Args.Content))
            return ToolResult.Error("'content' is required for broadcast.");

        try
        {
            await Team.BroadcastAsync(Args.Content);
            return ToolResult.Success("Message broadcast to all teammates.");
        }
        catch (InvalidOperationException Ex)
        {
            return ToolResult.Error(Ex.Message);
        }
    }
    
    /// <summary>팀 상태를 반환합니다.</summary>
    private static ToolResult GetStatus(Team.Team Team)
    {
        if (Team.TeamName is null)
            return ToolResult.Success("No active team.");

        StringBuilder Sb = new();
        Sb.AppendLine($"Team: {Team.TeamName}");
        Sb.AppendLine($"Members: {Team.Members.Count}");

        foreach ((string Name, TeammateInfo Info) in Team.Members)
        {
            string Status = Info.Process.HasExited ? "exited" : "active";
            Sb.AppendLine($"  - {Name} (port: {Info.Port}, status: {Status})");
        }

        return ToolResult.Success(Sb.ToString().TrimEnd());
    }
    
    // ── 공통 ──
    
    /// <summary>특정 팀원에게 메시지를 보냅니다.</summary>
    private static async Task<ToolResult> SendMessage(Team.Team Team, Input Args)
    {
        if (string.IsNullOrWhiteSpace(Args.Recipient))
            return ToolResult.Error("'recipient' is required for message.");
        
        if (string.IsNullOrWhiteSpace(Args.Content))
            return ToolResult.Error("'content' is required for message.");

        try
        {
            await Team.SendMessageAsync(Args.Recipient, MessageType.Chat, Args.Content);
            return ToolResult.Success($"Message sent to '{Args.Recipient}'.");
        }
        catch (InvalidOperationException Ex)
        {
            return ToolResult.Error(Ex.Message);
        }
    }
    
    /// <summary>리더 전용 액션 에러를 반환합니다.</summary>
    private static Task<ToolResult> LeaderOnly()
        => Task.FromResult(ToolResult.Error("This action is only available to the team leader."));
}