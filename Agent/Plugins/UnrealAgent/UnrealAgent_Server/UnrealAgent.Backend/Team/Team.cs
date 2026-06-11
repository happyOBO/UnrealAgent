using System.Diagnostics;
using System.Net.Sockets;
using UnrealAgent.Backend.Core;

namespace UnrealAgent.Backend.Team;

/// <summary>팀원의 프로세스와 포트 정보입니다.</summary>
public record TeammateInfo(Process Process, int Port);

/// <summary>
/// 팀 생명주기, 프로세스 관리, 메시징을 담당합니다.
/// </summary>
public sealed class Team : IAsyncDisposable
{
    /// <summary>현재 팀 이름입니다. 팀이 없으면 null입니다.</summary>
    public string? TeamName { get; set; }
    
    /// <summary>이 에이전트의 이름입니다. 리더는 "leader", 팀원은 고유 이름입니다.</summary>
    public string AgentName { get; set; } = "leader";

    /// <summary>부모 프로세스 ID입니다. 팀원일 때 부모 생존을 감시합니다.</summary>
    public int? ParentPid { get; set; }
    
    /// <summary>팀원 목록입니다.</summary>
    private Dictionary<string, TeammateInfo> Teammates { get; } = new();
    
    /// <summary>팀원 목록을 읽기 전용으로 반환합니다.</summary>
    public IReadOnlyDictionary<string, TeammateInfo> Members => Teammates;
    
    /// <summary>팀 상태 변경 시 발생하는 이벤트입니다.</summary>
    public event Action? OnTeamChanged;
    
    /// <summary>팀 리소스를 정리합니다.</summary>
    public async ValueTask DisposeAsync() => await DeleteTeamAsync();
    
    // ── 팀 생명주기 ──
    
    /// <summary>새 팀을 생성합니다.</summary>
    public void CreateTeam(string Name)
    {
        if (TeamName is not null)
            throw new InvalidOperationException($"Team '{TeamName}' already exists.");

        if (Directory.Exists(AgentPaths.GetTeamDir(Name)))
            throw new InvalidOperationException($"Team directory '{Name}' already exists. Use a different name.");

        Directory.CreateDirectory(AgentPaths.GetMailboxDir(Name));

        TeamName = Name;
        OnTeamChanged?.Invoke();
    }
    
    /// <summary>팀 전체를 삭제합니다.</summary>
    public async Task DeleteTeamAsync()
    {
        if (TeamName is null)
            return;

        foreach (string Name in Teammates.Keys.ToList())
            await ShutdownTeammateAsync(Name);

        string TeamDir = AgentPaths.GetTeamDir(TeamName);
        if (Directory.Exists(TeamDir))
            Directory.Delete(TeamDir, recursive: true);

        TeamName = null;
        OnTeamChanged?.Invoke();
    }
    
    // ── 팀원 관리 ──

    /// <summary>팀원 프로세스를 스폰합니다.</summary>
    public async Task SpawnTeammateAsync(string Name, string? Prompt)
    {
        if (TeamName is null)
            throw new InvalidOperationException("No active team.");
        
        if (Teammates.ContainsKey(Name))
            throw new InvalidOperationException($"Teammate '{Name}' already exists.");

        int Port = FindAvailablePort();
        Process Proc = SpawnProcess(TeamName, Name, Port, Environment.ProcessId);

        // 프로세스가 즉시 크래시하지 않았는지 확인
        await Task.Delay(1000);
        if (Proc.HasExited)
        {
            int ExitCode = Proc.ExitCode;
            Proc.Dispose();
            
            throw new InvalidOperationException($"Teammate '{Name}' crashed on startup (exit code: {ExitCode}).");
        }

        Teammates[Name] = new TeammateInfo(Proc, Port);
        OnTeamChanged?.Invoke();

        if (!string.IsNullOrEmpty(Prompt))
            await SendMessageAsync(Name, MessageType.Chat, Prompt);
    }
    
    /// <summary>팀원 프로세스를 종료합니다.</summary>
    public async Task ShutdownTeammateAsync(string Name)
    {
        if (TeamName is null)
            return;

        if (Teammates.Remove(Name, out TeammateInfo? Info))
        {
            // 프로세스가 정상적으로 존재할 때만 종료 메세지 전송
            if (!Info.Process.HasExited)
            {
                await SendMessageAsync(Name, MessageType.Command, "shutdown");

                try
                {
                    // 5초후 종료되도록 설정하고 대기
                    using CancellationTokenSource Cts = new(5000);
                    await Info.Process.WaitForExitAsync(Cts.Token);
                }
                catch (OperationCanceledException)
                {
                    Info.Process.Kill(entireProcessTree: true);
                }
            }

            Info.Process.Dispose();
        }

        OnTeamChanged?.Invoke();
    }
    
    // ── 메시징 ──

    /// <summary>특정 팀원에게 메시지를 보냅니다.</summary>
    public async Task SendMessageAsync(string To, MessageType Type, string Content)
    {
        if (TeamName is null)
            throw new InvalidOperationException("No active team.");

        string MailboxDir = AgentPaths.GetMailboxDir(TeamName);
        TeamMessage Message = new(AgentName, Type, Content, DateTime.UtcNow);

        await Mailbox.SendAsync(MailboxDir, To, Message);
    }
    
    /// <summary>모든 팀원에게 메시지를 브로드캐스트합니다.</summary>
    public async Task BroadcastAsync(string Content)
    {
        foreach (string Name in Teammates.Keys)
            await SendMessageAsync(Name, MessageType.Chat, Content);
    }
    
    // ── 프로세스 ──
    
    /// <summary>팀원 프로세스를 생성합니다.</summary>
    private static Process SpawnProcess(string TeamName, string Name, int Port, int ParentPid)
    {
        string ExePath = Environment.ProcessPath ?? throw new InvalidOperationException("Cannot determine process path.");

        return Process.Start(new ProcessStartInfo
        {
            FileName = ExePath,
            Arguments = $"--team-name \"{TeamName}\" --agent-name \"{Name}\" --port {Port} --parent-pid {ParentPid}",
            UseShellExecute = false,
            CreateNoWindow = true
        }) ?? throw new InvalidOperationException($"Failed to start process '{Name}'.");
    }
    
    /// <summary>커맨드라인 인자에서 팀 정보를 파싱합니다.</summary>
    public void ParseArgs(string[] Args)
    {
        for (int i = 0; i < Args.Length - 1; i++)
        {
            switch (Args[i])
            {
                case "--team-name":  
                    TeamName  = Args[++i]; 
                    break;
                
                case "--agent-name": 
                    AgentName = Args[++i]; 
                    break;
                
                case "--parent-pid": 
                    ParentPid = int.Parse(Args[++i]); 
                    break;
            }
        }
    }
    
    /// <summary>사용 가능한 포트를 찾습니다.</summary>
    private int FindAvailablePort(int StartPort = 59000)
    {
        HashSet<int> UsedPorts = Teammates.Values.Select(t => t.Port).ToHashSet();

        for (int Port = StartPort; Port < StartPort + 1000; Port++)
        {
            if (UsedPorts.Contains(Port))
                continue;

            try
            {
                using TcpListener Listener = new(System.Net.IPAddress.Loopback, Port);
                Listener.Start();
                Listener.Stop();

                return Port;
            }
            catch (SocketException)
            {
                
            }
        }

        throw new InvalidOperationException("No available port found.");
    }

}