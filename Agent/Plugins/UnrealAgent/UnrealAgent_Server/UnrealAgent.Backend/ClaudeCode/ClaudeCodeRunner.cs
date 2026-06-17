using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json;
using UnrealAgent.Backend.Conversation;

namespace UnrealAgent.Backend.ClaudeCode;

/// <summary>
/// claude CLI를 지속 스트리밍 모드로 1턴 실행하고 stream-json NDJSON을 파싱합니다.
/// 권한 control 프로토콜(can_use_tool ↔ control_response)을 stdin/stdout으로 중계합니다.
/// 인스턴스는 1턴(=1 프로세스)용이며 사용 후 폐기합니다.
/// </summary>
public sealed class ClaudeCodeRunner(string CliPath) : IAsyncDisposable
{
    /// <summary>Dispose 시 프로세스 정상 종료를 기다리는 최대 시간(초)입니다. 초과하면 강제 종료합니다.</summary>
    private const int KillTimeoutSeconds = 5;

    private Process? Proc;
    private string? SystemPromptFile;
    private readonly StringBuilder StdErr = new();

    /// <summary>
    /// CLI를 실행하고 stream-json 이벤트를 순차적으로 방출합니다.
    /// Permission 아이템 수신 시 소비자가 Decision을 설정하면 control_response를 전송합니다.
    /// </summary>
    public async IAsyncEnumerable<ClaudeStreamItem> RunAsync(
        ClaudeRunOptions Options,
        UserInput Input,
        [EnumeratorCancellation] CancellationToken Ct = default)
    {
        // 1) 시스템 프롬프트를 임시 파일로 작성 (명령행 이스케이프/길이 회피)
        SystemPromptFile = Path.Combine(Path.GetTempPath(), $"unrealagent-sysprompt-{Guid.NewGuid():N}.txt");
        await File.WriteAllTextAsync(SystemPromptFile, Options.SystemPrompt, new UTF8Encoding(false), Ct);

        // 2) 프로세스 시작
        ProcessStartInfo Psi = CreateStartInfo(Options, SystemPromptFile);

        Proc = new Process { StartInfo = Psi };
        if (!Proc.Start())
        {
            yield return new ClaudeStreamItem.Failure("claude 프로세스를 시작하지 못했습니다.");
            yield break;
        }

        // stderr를 백그라운드로 흡수하여 파이프 버퍼 블로킹을 방지합니다.
        _ = Task.Run(async () =>
        {
            try
            {
                string? Line;
                while ((Line = await Proc.StandardError.ReadLineAsync()) != null)
                    lock (StdErr) StdErr.AppendLine(Line);
            }
            catch { /* 종료 시 무시 */ }
        }, Ct);

        // 3) 핸드셰이크 + 사용자 메시지 전송 (stdin은 턴 종료까지 열어둠)
        await WriteLineAsync(ControlProtocol.BuildInitialize(), Ct);
        await WriteLineAsync(ControlProtocol.BuildUserMessage(Input), Ct);

        // 4) stdout NDJSON 읽기 루프
        StreamReader Out = Proc.StandardOutput;
        bool Done = false;

        while (!Done)
        {
            string? Line;
            try
            {
                Line = await Out.ReadLineAsync(Ct);
            }
            catch (OperationCanceledException)
            {
                yield break;
            }

            // EOF: 프로세스가 result 없이 종료됨
            if (Line is null)
            {
                yield return BuildEofFailure();
                yield break;
            }

            if (Line.Length == 0)
                continue;

            // 한 줄을 파싱하여 0~N개의 아이템으로 변환
            ParsedLine Parsed = ParseLine(Line);

            foreach (ClaudeStreamItem Item in Parsed.Items)
            {
                // 권한 요청은 yield 후 결정을 기다렸다가 control_response를 전송
                if (Item is ClaudeStreamItem.Permission Perm)
                {
                    yield return Perm;

                    bool Allow = await Perm.Decision.Task.WaitAsync(Ct);
                    string Response = ControlProtocol.BuildPermissionResponse(
                        Perm.RequestId, Allow, Perm.InputJson,
                        Allow ? null : $"User denied '{Perm.ToolName}'.");

                    await WriteLineAsync(Response, Ct);
                }
                else
                {
                    yield return Item;
                }
            }

            if (Parsed.IsTurnEnd)
                Done = true;
        }
    }

    /// <summary>stdin에 한 줄(NDJSON)을 쓰고 개행 후 flush합니다.</summary>
    private async Task WriteLineAsync(string Json, CancellationToken Ct)
    {
        if (Proc is null)
            return;

        await Proc.StandardInput.WriteAsync(Json.AsMemory(), Ct);
        await Proc.StandardInput.WriteAsync("\n".AsMemory(), Ct);
        await Proc.StandardInput.FlushAsync(Ct);
    }

    // ── 프로세스 구성 ──

    /// <summary>플랫폼/설치형태(.exe vs .cmd)에 맞춰 ProcessStartInfo를 구성합니다.</summary>
    private ProcessStartInfo CreateStartInfo(ClaudeRunOptions Options, string SystemPromptFilePath)
    {
        List<string> Args = CliArgsBuilder.Build(Options, SystemPromptFilePath);

        ProcessStartInfo Psi = new()
        {
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
            StandardInputEncoding = new UTF8Encoding(false),
            StandardOutputEncoding = new UTF8Encoding(false),
            StandardErrorEncoding = new UTF8Encoding(false),
            WorkingDirectory = string.IsNullOrEmpty(Options.WorkingDirectory)
                ? Environment.CurrentDirectory
                : Options.WorkingDirectory,
        };

        // Windows의 .cmd/.bat 셸은 직접 CreateProcess 불가 → cmd.exe /c로 래핑
        bool IsBatch = OperatingSystem.IsWindows()
                       && (CliPath.EndsWith(".cmd", StringComparison.OrdinalIgnoreCase)
                           || CliPath.EndsWith(".bat", StringComparison.OrdinalIgnoreCase));

        if (IsBatch)
        {
            Psi.FileName = "cmd.exe";
            Psi.ArgumentList.Add("/c");
            Psi.ArgumentList.Add(CliPath);
        }
        else
        {
            Psi.FileName = CliPath;
        }

        foreach (string Arg in Args)
            Psi.ArgumentList.Add(Arg);

        // 인증: API 키가 있으면 주입, 없으면 claude 로그인 자격증명 사용
        if (!string.IsNullOrEmpty(Options.ApiKey))
            Psi.Environment["ANTHROPIC_API_KEY"] = Options.ApiKey;

        // 확장 사고 비활성화 매핑 (effort와 별개의 근사 매핑)
        if (!Options.ThinkingEnabled)
            Psi.Environment["MAX_THINKING_TOKENS"] = "0";

        return Psi;
    }

    // ── NDJSON 파싱 ──

    /// <summary>한 줄 파싱 결과입니다.</summary>
    private readonly record struct ParsedLine(IReadOnlyList<ClaudeStreamItem> Items, bool IsTurnEnd);

    /// <summary>
    /// NDJSON 한 줄을 ClaudeStreamItem 목록으로 변환합니다.
    /// (UnrealClaude ParseAndEmitNdjsonLine 매핑 + thinking/권한/usage 확장)
    /// </summary>
    private static ParsedLine ParseLine(string Line)
    {
        List<ClaudeStreamItem> Items = [];
        bool TurnEnd = false;

        JsonDocument Doc;
        try
        {
            Doc = JsonDocument.Parse(Line);
        }
        catch (JsonException)
        {
            // 비 JSON 라인은 무시
            return new ParsedLine(Items, false);
        }

        using (Doc)
        {
            JsonElement Root = Doc.RootElement;
            if (!Root.TryGetProperty("type", out JsonElement TypeEl) || TypeEl.ValueKind != JsonValueKind.String)
                return new ParsedLine(Items, false);

            string Type = TypeEl.GetString()!;

            switch (Type)
            {
                case "system":
                    if (Root.TryGetProperty("subtype", out JsonElement Sub) && Sub.GetString() == "init"
                        && Root.TryGetProperty("session_id", out JsonElement Sid) && Sid.ValueKind == JsonValueKind.String)
                    {
                        Items.Add(new ClaudeStreamItem.Init(Sid.GetString()!));
                    }
                    break;

                case "assistant":
                    AppendAssistantBlocks(Root, Items);
                    break;

                case "user":
                    AppendToolResults(Root, Items);
                    break;

                case "control_request":
                    AppendPermission(Root, Items);
                    break;

                case "result":
                    Items.Add(BuildResult(Root));
                    TurnEnd = true;
                    break;

                // rate_limit_event, control_response(우리 init에 대한 응답) 등은 무시
            }
        }

        return new ParsedLine(Items, TurnEnd);
    }

    /// <summary>assistant 메시지의 content 블록(text/thinking/tool_use)을 변환합니다.</summary>
    private static void AppendAssistantBlocks(JsonElement Root, List<ClaudeStreamItem> Items)
    {
        if (!Root.TryGetProperty("message", out JsonElement Msg))
            return;

        // 이 요청 시점의 실제 컨텍스트 크기입니다. result의 누적 usage와 달리
        // 단일 모델 호출의 입력측 총량이므로 컨텍스트 미터의 올바른 출처입니다.
        if (Msg.TryGetProperty("usage", out JsonElement Usg) && Usg.ValueKind == JsonValueKind.Object)
        {
            long Context = GetLong(Usg, "input_tokens")
                           + GetLong(Usg, "cache_read_input_tokens")
                           + GetLong(Usg, "cache_creation_input_tokens");
            if (Context > 0)
                Items.Add(new ClaudeStreamItem.ContextUsage(Context));
        }

        // 스트리밍 분류기 개입(거부) 처리
        if (Msg.TryGetProperty("stop_reason", out JsonElement StopEl)
            && StopEl.ValueKind == JsonValueKind.String && StopEl.GetString() == "refusal")
        {
            Items.Add(new ClaudeStreamItem.AssistantText(
                "응답이 콘텐츠 안전 필터에 의해 거부되었습니다. 요청을 다시 표현해 주세요."));
            return;
        }

        if (!Msg.TryGetProperty("content", out JsonElement Content) || Content.ValueKind != JsonValueKind.Array)
            return;

        foreach (JsonElement Block in Content.EnumerateArray())
        {
            if (!Block.TryGetProperty("type", out JsonElement BtEl) || BtEl.ValueKind != JsonValueKind.String)
                continue;

            switch (BtEl.GetString())
            {
                case "text":
                    if (Block.TryGetProperty("text", out JsonElement TextEl) && TextEl.ValueKind == JsonValueKind.String)
                        Items.Add(new ClaudeStreamItem.AssistantText(TextEl.GetString()!));
                    break;

                case "thinking":
                    if (Block.TryGetProperty("thinking", out JsonElement ThEl) && ThEl.ValueKind == JsonValueKind.String)
                        Items.Add(new ClaudeStreamItem.Thinking(ThEl.GetString()!));
                    break;

                case "tool_use":
                    string Id = GetString(Block, "id") ?? "";
                    string Name = GetString(Block, "name") ?? "";
                    string InputJson = Block.TryGetProperty("input", out JsonElement InEl)
                        ? InEl.GetRawText() : "{}";
                    Items.Add(new ClaudeStreamItem.ToolUse(Id, Name, InputJson));
                    break;
            }
        }
    }

    /// <summary>user 메시지의 tool_result 블록을 변환합니다.</summary>
    private static void AppendToolResults(JsonElement Root, List<ClaudeStreamItem> Items)
    {
        if (!Root.TryGetProperty("message", out JsonElement Msg)
            || !Msg.TryGetProperty("content", out JsonElement Content)
            || Content.ValueKind != JsonValueKind.Array)
            return;

        foreach (JsonElement Block in Content.EnumerateArray())
        {
            if (GetString(Block, "type") != "tool_result")
                continue;

            string ToolUseId = GetString(Block, "tool_use_id") ?? "";
            bool IsError = Block.TryGetProperty("is_error", out JsonElement ErrEl)
                           && ErrEl.ValueKind is JsonValueKind.True;
            string ResultContent = ExtractContent(Block);

            Items.Add(new ClaudeStreamItem.ToolResult(ToolUseId, ResultContent, IsError));
        }
    }

    /// <summary>control_request의 can_use_tool을 Permission 아이템으로 변환합니다.</summary>
    private static void AppendPermission(JsonElement Root, List<ClaudeStreamItem> Items)
    {
        if (!Root.TryGetProperty("request", out JsonElement Req)
            || GetString(Req, "subtype") != "can_use_tool")
            return;

        string RequestId = GetString(Root, "request_id") ?? "";
        string ToolName = GetString(Req, "tool_name") ?? "";
        string InputJson = Req.TryGetProperty("input", out JsonElement InEl) ? InEl.GetRawText() : "{}";
        string? ToolUseId = GetString(Req, "tool_use_id");

        Items.Add(new ClaudeStreamItem.Permission(RequestId, ToolName, InputJson, ToolUseId));
    }

    /// <summary>result 이벤트에서 사용량/비용을 추출합니다.</summary>
    private static ClaudeStreamItem.Result BuildResult(JsonElement Root)
    {
        bool IsError = Root.TryGetProperty("is_error", out JsonElement ErrEl) && ErrEl.ValueKind is JsonValueKind.True;
        string? Text = GetString(Root, "result");
        double Cost = Root.TryGetProperty("total_cost_usd", out JsonElement CostEl) && CostEl.ValueKind == JsonValueKind.Number
            ? CostEl.GetDouble() : 0;

        long InTok = 0, OutTok = 0, CacheRead = 0, CacheCreate = 0;
        if (Root.TryGetProperty("usage", out JsonElement Usage) && Usage.ValueKind == JsonValueKind.Object)
        {
            InTok = GetLong(Usage, "input_tokens");
            OutTok = GetLong(Usage, "output_tokens");
            // 프롬프트 캐싱 사용 시 input_tokens에서 빠지는 부분 — 컨텍스트 총량 산출에 합산합니다.
            CacheRead = GetLong(Usage, "cache_read_input_tokens");
            CacheCreate = GetLong(Usage, "cache_creation_input_tokens");
        }

        return new ClaudeStreamItem.Result(IsError, Text, InTok, OutTok, CacheRead, CacheCreate, Cost);
    }

    /// <summary>tool_result의 content(문자열 또는 블록 배열)에서 텍스트를 추출합니다.</summary>
    private static string ExtractContent(JsonElement Block)
    {
        if (!Block.TryGetProperty("content", out JsonElement Content))
            return "";

        if (Content.ValueKind == JsonValueKind.String)
            return Content.GetString() ?? "";

        if (Content.ValueKind == JsonValueKind.Array)
        {
            StringBuilder Sb = new();
            foreach (JsonElement Sub in Content.EnumerateArray())
            {
                if (GetString(Sub, "type") == "text" && Sub.TryGetProperty("text", out JsonElement T)
                    && T.ValueKind == JsonValueKind.String)
                {
                    if (Sb.Length > 0) Sb.Append('\n');
                    Sb.Append(T.GetString());
                }
            }
            return Sb.ToString();
        }

        return "";
    }

    private static string? GetString(JsonElement El, string Prop)
        => El.TryGetProperty(Prop, out JsonElement V) && V.ValueKind == JsonValueKind.String ? V.GetString() : null;

    private static long GetLong(JsonElement El, string Prop)
        => El.TryGetProperty(Prop, out JsonElement V) && V.ValueKind == JsonValueKind.Number ? V.GetInt64() : 0;

    /// <summary>result 없이 EOF에 도달했을 때 stderr를 담은 실패 아이템을 만듭니다.</summary>
    private ClaudeStreamItem.Failure BuildEofFailure()
    {
        string Err;
        lock (StdErr) Err = StdErr.ToString().Trim();

        return new ClaudeStreamItem.Failure(string.IsNullOrEmpty(Err)
            ? "claude 프로세스가 응답 없이 종료되었습니다."
            : $"claude 오류: {Err}");
    }

    // ── 정리 ──

    public async ValueTask DisposeAsync()
    {
        if (Proc is not null)
        {
            try
            {
                // stdin 종료로 프로세스에 EOF 신호
                if (!Proc.HasExited)
                    Proc.StandardInput.Close();
            }
            catch { /* 무시 */ }

            try
            {
                using CancellationTokenSource Cts = new(TimeSpan.FromSeconds(KillTimeoutSeconds));
                await Proc.WaitForExitAsync(Cts.Token);
            }
            catch
            {
                try { if (!Proc.HasExited) Proc.Kill(entireProcessTree: true); }
                catch { /* 무시 */ }
            }

            Proc.Dispose();
            Proc = null;
        }

        if (SystemPromptFile is not null)
        {
            try { File.Delete(SystemPromptFile); }
            catch { /* 무시 */ }
            SystemPromptFile = null;
        }
    }
}
