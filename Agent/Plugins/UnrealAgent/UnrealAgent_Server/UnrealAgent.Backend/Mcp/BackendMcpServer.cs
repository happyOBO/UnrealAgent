using System.Text.Json.Nodes;
using UnrealAgent.Backend.Agent;
using UnrealAgent.Backend.Tool;

namespace UnrealAgent.Backend.Mcp;

/// <summary>
/// 백엔드(.NET) 프로세스가 호스팅하는 MCP 서버입니다.
/// claude CLI가 HTTP(JSON-RPC over POST)로 호출하며, ToolRegistry의 C# 도구(team/skill)를
/// CLI 측 모델에 노출하고 실행합니다. UE 에디터의 네이티브 MCP 서버(McpServer.cpp)와
/// 병렬로 동작합니다.
///
/// JSON-RPC 2.0 메서드: initialize / tools/list / tools/call.
/// </summary>
public sealed class BackendMcpServer(ToolRegistry ToolRegistry, AgentSession Session)
{
    /// <summary>MCP 프로토콜 버전입니다. UE 네이티브 서버와 동일하게 맞춥니다.</summary>
    private const string ProtocolVersion = "2025-03-26";

    /// <summary>
    /// 요청 본문(JSON 문자열)을 처리하고 JSON-RPC 응답을 반환합니다.
    /// id가 없는 notification(예: notifications/initialized)이면 null을 반환합니다 —
    /// 호출자는 본문 없는 202 Accepted로 응답해야 합니다.
    ///
    /// 주의: id 부재 시 에러 응답을 돌려주면 CLI의 Streamable-HTTP 핸드셰이크가 깨져
    /// 첫 도구 호출에서 무한 대기에 빠집니다(McpServer.cpp의 동일 수정과 반드시 일치).
    /// </summary>
    public async Task<JsonNode?> HandleAsync(string Body, CancellationToken Ct = default)
    {
        JsonNode? Root;
        try
        {
            Root = JsonNode.Parse(Body);
        }
        catch
        {
            return MakeError(null, -32700, "Parse error");
        }

        if (Root is null)
            return MakeError(null, -32700, "Parse error");

        // notification(id 없음): 라우팅하지 않고 null 반환 → 호출자가 202 Accepted.
        JsonNode? IdNode = Root["id"];
        if (IdNode is null)
            return null;

        string? Method = Root["method"]?.GetValue<string>();

        return Method switch
        {
            "initialize" => MakeResult(IdNode, BuildInitialize()),
            "tools/list" => MakeResult(IdNode, BuildToolsList()),
            "tools/call" => MakeResult(IdNode, await BuildToolsCall(Root["params"], Ct)),
            _ => MakeError(IdNode, -32601, $"Method not found: {Method}")
        };
    }

    //-------------------------------------------------------------------------
    // 메서드 핸들러
    //-------------------------------------------------------------------------

    /// <summary>initialize 응답(serverInfo + capabilities)을 구성합니다.</summary>
    private static JsonObject BuildInitialize() => new()
    {
        ["protocolVersion"] = ProtocolVersion,
        ["serverInfo"] = new JsonObject
        {
            ["name"] = "unreal-agent-backend",
            ["version"] = "1.0.0"
        },
        ["capabilities"] = new JsonObject
        {
            ["tools"] = new JsonObject()
        }
    };

    /// <summary>tools/list 응답(등록된 도구 정의 배열)을 구성합니다.</summary>
    private JsonObject BuildToolsList()
    {
        JsonArray ToolsArray = [];

        foreach (ToolRegistry.McpToolDefinition Def in ToolRegistry.GetMcpDefinitions())
        {
            ToolsArray.Add(new JsonObject
            {
                ["name"] = Def.Name,
                ["description"] = Def.Description,
                ["inputSchema"] = Def.InputSchema.DeepClone()
            });
        }

        return new JsonObject { ["tools"] = ToolsArray };
    }

    /// <summary>tools/call 응답(도구 실행 결과)을 구성합니다.</summary>
    private async Task<JsonObject> BuildToolsCall(JsonNode? Params, CancellationToken Ct)
    {
        string? Name = Params?["name"]?.GetValue<string>();
        if (string.IsNullOrEmpty(Name))
            return MakeToolContent("Missing tool name.", bIsError: true);

        IAgentTool? Tool = ToolRegistry.TryGetTool(Name);
        if (Tool is null)
            return MakeToolContent($"Unknown tool: {Name}", bIsError: true);

        // arguments는 객체이며, 도구는 JSON 문자열을 역직렬화합니다. 없으면 빈 객체.
        string ArgsJson = Params?["arguments"]?.ToJsonString() ?? "{}";

        try
        {
            ToolResult Result = await Tool.ExecuteAsync(ArgsJson, Session, Ct);
            return MakeToolContent(Result.Content, bIsError: !Result.bIsSuccess);
        }
        catch (Exception Ex)
        {
            return MakeToolContent($"Tool '{Name}' threw: {Ex.Message}", bIsError: true);
        }
    }

    //-------------------------------------------------------------------------
    // JSON-RPC / MCP 유틸리티
    //-------------------------------------------------------------------------

    /// <summary>MCP ToolCallResult 형식({content:[{type,text}], isError})을 구성합니다.</summary>
    private static JsonObject MakeToolContent(string Text, bool bIsError) => new()
    {
        ["content"] = new JsonArray
        {
            new JsonObject { ["type"] = "text", ["text"] = Text }
        },
        ["isError"] = bIsError
    };

    /// <summary>JSON-RPC 2.0 성공 응답을 구성합니다. id 노드는 원본을 복제해 보존합니다.</summary>
    private static JsonObject MakeResult(JsonNode IdNode, JsonNode Result) => new()
    {
        ["jsonrpc"] = "2.0",
        ["id"] = IdNode.DeepClone(),
        ["result"] = Result
    };

    /// <summary>JSON-RPC 2.0 에러 응답을 구성합니다. id가 없으면 null로 둡니다.</summary>
    private static JsonObject MakeError(JsonNode? IdNode, int Code, string Message) => new()
    {
        ["jsonrpc"] = "2.0",
        ["id"] = IdNode?.DeepClone(),
        ["error"] = new JsonObject
        {
            ["code"] = Code,
            ["message"] = Message
        }
    };
}
