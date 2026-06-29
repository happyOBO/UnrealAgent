using System.ComponentModel;
using System.Reflection;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using Microsoft.Extensions.DependencyInjection;
using UnrealAgent.Backend.Tool.Attributes;

namespace UnrealAgent.Backend.Tool;
using ClrType = System.Type;

/// <summary>
/// [AgentTool] 어트리뷰트를 스캔하여 도구를 등록합니다.
/// 도구 인스턴스는 Discovery 시 한 번 생성되어 재사용됩니다.
/// 등록된 도구는 백엔드 MCP 서버(BackendMcpServer)가 CLI에 노출하고 실행합니다.
/// </summary>
public sealed class ToolRegistry(IServiceProvider ServiceProvider)
{
    /// <summary>MCP tools/list 응답용 도구 정의입니다.</summary>
    public sealed record McpToolDefinition(string Name, string Description, JsonObject InputSchema);

    /// <summary>도구 인스턴스와 MCP 정의를 묶어 보관합니다.</summary>
    private sealed record ToolEntry(IAgentTool Tool, McpToolDefinition Definition);

    /// <summary>도구 이름 → ToolEntry 매핑입니다.</summary>
    private readonly Dictionary<string, ToolEntry> Tools = new();

    /// <summary>
    /// 등록된 모든 도구의 MCP 정의를 반환합니다 (tools/list 응답).
    /// </summary>
    public IReadOnlyList<McpToolDefinition> GetMcpDefinitions() =>
        Tools.Values.Select(E => E.Definition).ToList();

    /// <summary>
    /// 이름으로 도구 인스턴스를 조회합니다. 없으면 null입니다.
    /// </summary>
    public IAgentTool? TryGetTool(string Name) =>
        Tools.TryGetValue(Name, out ToolEntry? Entry) ? Entry.Tool : null;

    /// <summary>
    /// 지정된 어셈블리에서 [AgentTool] + IAgentTool 클래스를 스캔하여 등록합니다.
    /// 인스턴스는 DI로 한 번 생성되어 재사용됩니다.
    /// </summary>
    public void DiscoverTools(params Assembly[] Assemblies)
    {
        foreach (Assembly Asm in Assemblies)
        {
            foreach (ClrType Type in Asm.GetTypes())
            {
                // [AgentTool] 어트리뷰트가 있고 IAgentTool을 구현한 클래스만 처리합니다.
                AgentToolAttribute? Attr = Type.GetCustomAttribute<AgentToolAttribute>();

                if (Attr is null)
                    continue;

                // IAgentTool을 구현한 클래스인지 체크합니다.
                if (!typeof(IAgentTool).IsAssignableFrom(Type))
                    continue;

                // DI로 인스턴스를 한 번 생성합니다.
                if (ActivatorUtilities.CreateInstance(ServiceProvider, Type) is not IAgentTool Instance)
                    continue;

                // AgentTool<TInput>에서 TInput 타입을 추출하여 MCP 입력 스키마를 생성합니다.
                McpToolDefinition Definition = new(
                    Attr.Name,
                    Attr.Description,
                    GenerateSchemaFromType(Type));

                Tools[Attr.Name] = new ToolEntry(Instance, Definition);
            }
        }
    }

    /// <summary>
    /// AgentTool의 TInput 레코드에서 MCP inputSchema(JSON Schema)를 자동 생성합니다.
    /// [Description] 어트리뷰트로 파라미터 설명을, [JsonPropertyName]으로 JSON 키를 지정합니다.
    /// 형식: {"type":"object","properties":{...},"required":[...]}
    /// </summary>
    private static JsonObject GenerateSchemaFromType(ClrType ToolType)
    {
        JsonObject Properties = new();
        JsonArray Required = [];

        ClrType? InputType = FindInputType(ToolType);
        if (InputType is not null)
        {
            foreach (PropertyInfo Prop in InputType.GetProperties(BindingFlags.Public | BindingFlags.Instance))
            {
                // JSON 키: [JsonPropertyName]이 있으면 사용, 없으면 camelCase
                string JsonName = Prop.GetCustomAttribute<JsonPropertyNameAttribute>()?.Name
                                  ?? char.ToLowerInvariant(Prop.Name[0]) + Prop.Name[1..];

                string Description = Prop.GetCustomAttribute<DescriptionAttribute>()?.Description ?? "";

                Properties[JsonName] = new JsonObject
                {
                    ["type"] = GetJsonSchemaType(Prop.PropertyType),
                    ["description"] = Description
                };

                // Nullable이 아닌 프로퍼티는 required로 등록합니다.
                if (!IsNullable(Prop))
                    Required.Add(JsonName);
            }
        }

        return new JsonObject
        {
            ["type"] = "object",
            ["properties"] = Properties,
            ["required"] = Required
        };
    }

    /// <summary>AgentTool 상속 체인에서 TInput 타입을 추출합니다.</summary>
    private static ClrType? FindInputType(ClrType ToolType)
    {
        // 예: MyTool → AgentTool<MyToolInput> → object 순서로 올라감
        ClrType? Current = ToolType;

        while (Current is not null)
        {
            // 현재 타입이 AgentTool<> 이면 TInput을 꺼내서 반환
            if (Current.IsGenericType && Current.GetGenericTypeDefinition() == typeof(AgentTool<>))
                return Current.GetGenericArguments()[0];

            // 아니면 부모 클래스로 한 칸 올라감
            Current = Current.BaseType;
        }

        // 상속 체인에 AgentTool<>이 없으면 null
        return null;
    }

    /// <summary>C# 타입을 JSON Schema 타입 문자열로 변환합니다.</summary>
    private static string GetJsonSchemaType(ClrType ClrType)
    {
        ClrType Underlying = Nullable.GetUnderlyingType(ClrType) ?? ClrType;

        if (Underlying == typeof(string)) return "string";
        if (Underlying == typeof(int) || Underlying == typeof(long)) return "integer";
        if (Underlying == typeof(double) || Underlying == typeof(float) || Underlying == typeof(decimal)) return "number";
        if (Underlying == typeof(bool)) return "boolean";
        if (Underlying.IsArray || typeof(System.Collections.IEnumerable).IsAssignableFrom(Underlying) && Underlying != typeof(string))
            return "array";

        return "object";
    }

    /// <summary>프로퍼티가 nullable인지 확인합니다. 참조 타입도 string vs string? 구분 가능.</summary>
    private static bool IsNullable(PropertyInfo Prop)
    {
        // 값 타입: int? 등은 Nullable<T>로 감싸져 있음
        if (Prop.PropertyType.IsValueType)
            return Nullable.GetUnderlyingType(Prop.PropertyType) is not null;

        // 참조 타입: NullabilityInfoContext로 string vs string? 구분
        NullabilityInfoContext Context = new();
        NullabilityInfo Info = Context.Create(Prop);
        return Info.WriteState == NullabilityState.Nullable;
    }
}
