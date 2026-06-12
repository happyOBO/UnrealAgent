using System.Text.Json;
using System.Text.Json.Nodes;
using UnrealAgent.Backend.Conversation;

namespace UnrealAgent.Backend.ClaudeCode;

/// <summary>
/// claude CLI stream-json 입력(stdin) 메시지를 생성합니다.
/// 모두 NDJSON 한 줄(개행 미포함)로 직렬화됩니다.
/// </summary>
public static class ControlProtocol
{
    /// <summary>압축 직렬화 옵션입니다 (NDJSON 단일 라인).</summary>
    private static readonly JsonSerializerOptions Compact = new() { WriteIndented = false };

    /// <summary>
    /// initialize control_request를 생성합니다. 세션 시작 시 1회 전송합니다.
    /// </summary>
    public static string BuildInitialize()
    {
        JsonObject Root = new()
        {
            ["type"] = "control_request",
            ["request_id"] = Guid.NewGuid().ToString(),
            ["request"] = new JsonObject { ["subtype"] = "initialize" }
        };
        return Root.ToJsonString(Compact);
    }

    /// <summary>
    /// 사용자 메시지를 stream-json user 이벤트로 생성합니다.
    /// 이미지가 있으면 image 블록을 먼저, 텍스트 블록을 뒤에 둡니다.
    /// </summary>
    public static string BuildUserMessage(UserInput Input)
    {
        JsonArray Content = new();

        if (Input.bHasImage)
        {
            Content.Add(new JsonObject
            {
                ["type"] = "image",
                ["source"] = new JsonObject
                {
                    ["type"] = "base64",
                    ["media_type"] = Input.ImageMediaType,
                    ["data"] = Input.ImageBase64
                }
            });
        }

        Content.Add(new JsonObject { ["type"] = "text", ["text"] = Input.Text });

        JsonObject Root = new()
        {
            ["type"] = "user",
            ["message"] = new JsonObject
            {
                ["role"] = "user",
                ["content"] = Content
            }
        };
        return Root.ToJsonString(Compact);
    }

    /// <summary>
    /// can_use_tool control_request에 대한 응답을 생성합니다.
    /// allow면 원본 입력을 updatedInput으로 echo하고, deny면 사유 메시지를 담습니다.
    /// </summary>
    public static string BuildPermissionResponse(string RequestId, bool Allow, string InputJson, string? DenyMessage)
    {
        JsonObject Inner = Allow
            ? new JsonObject
            {
                ["behavior"] = "allow",
                ["updatedInput"] = ParseOrEmpty(InputJson)
            }
            : new JsonObject
            {
                ["behavior"] = "deny",
                ["message"] = DenyMessage ?? "User denied tool execution."
            };

        JsonObject Root = new()
        {
            ["type"] = "control_response",
            ["response"] = new JsonObject
            {
                ["subtype"] = "success",
                ["request_id"] = RequestId,
                ["response"] = Inner
            }
        };
        return Root.ToJsonString(Compact);
    }

    /// <summary>도구 입력 JSON을 JsonNode로 파싱합니다. 실패 시 빈 객체를 반환합니다.</summary>
    private static JsonNode ParseOrEmpty(string InputJson)
    {
        if (string.IsNullOrWhiteSpace(InputJson))
            return new JsonObject();

        try
        {
            return JsonNode.Parse(InputJson) ?? new JsonObject();
        }
        catch (JsonException)
        {
            return new JsonObject();
        }
    }
}
