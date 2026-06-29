using System.Text.Json.Nodes;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Mcp;

namespace UnrealAgent.Backend.ClaudeCode;

/// <summary>
/// settings.local.json의 mcpServers를 claude CLI용 --mcp-config 파일로 변환합니다.
/// UE 에디터가 띄운 HTTP MCP 서버를 CLI가 직접 호출하도록 연결합니다.
/// </summary>
public sealed class CliMcpConfig
{
    /// <summary>생성된 CLI mcp-config 파일 경로입니다. 서버가 없으면 null입니다.</summary>
    public string? Path { get; private set; }

    /// <summary>
    /// McpConfig.Load() 결과와 백엔드 자기 자신의 MCP 서버로 CLI mcp-config 파일을 작성하고 경로를 저장합니다.
    /// 형식: {"mcpServers":{"<name>":{"type":"http","url":"<url>"}}}
    /// 시작 시 1회 호출합니다.
    /// </summary>
    /// <param name="BackendPort">
    /// 백엔드(.NET) 프로세스가 듣는 포트입니다. 자기 자신의 MCP 엔드포인트(/agent-mcp)를
    /// CLI에 노출해 team/skill 등 C# 도구를 모델이 호출할 수 있게 합니다.
    /// </param>
    public void Build(int BackendPort)
    {
        JsonObject ServersNode = new();

        // UE 에디터가 띄운 네이티브 MCP 서버들(settings.local.json).
        foreach ((string Name, McpServerConfig Config) in McpConfig.Load())
        {
            ServersNode[Name] = new JsonObject
            {
                // UE MCP 서버는 HTTP(JSON-RPC over POST)를 사용합니다.
                ["type"] = "http",
                ["url"] = Config.Url
            };
        }

        // 백엔드 자기 자신(BackendMcpServer). team/skill 같은 C# 도구를 CLI에 노출합니다.
        ServersNode["agent"] = new JsonObject
        {
            ["type"] = "http",
            ["url"] = $"http://localhost:{BackendPort}/agent-mcp"
        };

        JsonObject Root = new() { ["mcpServers"] = ServersNode };

        string Dir = AgentPaths.UserConfigDir;
        Directory.CreateDirectory(Dir);

        string FilePath = System.IO.Path.Combine(Dir, "cli-mcp-config.json");
        File.WriteAllText(FilePath, Root.ToJsonString());

        Path = FilePath;
    }
}
