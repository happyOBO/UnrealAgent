using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json;
using System.Threading.Channels;
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

    /// <summary>CLI stderr를 실시간 기록하는 로그 파일 경로입니다. stall(EOF 없음) 중에도 원인을 남깁니다.</summary>
    private string? StdErrLogPath;

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

        // CLI stderr를 실시간으로 파일에 기록합니다. EOF가 없는 stall 중에도 원인이 남도록 합니다.
        InitStdErrLog(Options.Model);

        // stderr를 백그라운드로 흡수하여 파이프 버퍼 블로킹을 방지하고, 도착 즉시 파일에 남깁니다.
        _ = Task.Run(async () =>
        {
            try
            {
                string? Line;
                while ((Line = await Proc.StandardError.ReadLineAsync()) != null)
                {
                    lock (StdErr) StdErr.AppendLine(Line);
                    AppendStdErrLog(Line);
                }
            }
            catch { /* 종료 시 무시 */ }
        }, Ct);

        // stdout도 백그라운드로 흡수하여 채널에 적재합니다. 이 drain은 stdin 쓰기보다 먼저 시작해야 합니다.
        // CLI는 initialize 수신 즉시 stdout으로 응답(system/init 등)을 쏟아내는데, 읽는 쪽이 없으면
        // OS 파이프 버퍼가 차서 CLI가 stdout write에 블록되고, 그 결과 stdin도 읽지 못해
        // 우리 쪽 user message 쓰기와 서로를 기다리는 교착(deadlock)에 빠진다. 동시 drain으로 이를 차단한다.
        Channel<string> StdoutChannel = Channel.CreateUnbounded<string>(
            new UnboundedChannelOptions { SingleReader = true, SingleWriter = true });

        _ = Task.Run(async () =>
        {
            try
            {
                string? Line;
                while ((Line = await Proc.StandardOutput.ReadLineAsync(Ct)) != null)
                    await StdoutChannel.Writer.WriteAsync(Line, Ct);

                StdoutChannel.Writer.TryComplete();
            }
            catch (Exception e)
            {
                // 취소/프로세스 종료 등은 완료 신호로 매핑합니다. 메인 루프가 EOF로 처리합니다.
                StdoutChannel.Writer.TryComplete(e);
            }
        }, Ct);

        // 3) 핸드셰이크 + 사용자 메시지 전송 (stdin은 턴 종료까지 열어둠)
        //    stdout이 위에서 이미 동시 drain 중이므로 큰 입력/출력에도 교착이 발생하지 않는다.
        await WriteLineAsync(ControlProtocol.BuildInitialize(), Ct);
        await WriteLineAsync(ControlProtocol.BuildUserMessage(Input), Ct);

        // 4) stdout NDJSON 읽기 루프 (채널 소비)
        ChannelReader<string> Reader = StdoutChannel.Reader;
        bool Done = false;

        // 유휴 타임아웃(stall) 워치독: CLI가 무출력으로 멈추면(MCP 핸드셰이크/모델 응답 지연 등)
        // 무한 대기 대신 진단 메시지로 표면화하고 턴을 끝냅니다.
        TimeSpan IdleTimeout = ResolveIdleTimeout();

        while (!Done)
        {
            string? Line = null;
            bool bStalled = false;
            bool bCancelled = false;
            bool bEof = false;

            using (CancellationTokenSource TimeoutCts = CancellationTokenSource.CreateLinkedTokenSource(Ct))
            {
                TimeoutCts.CancelAfter(IdleTimeout);
                try
                {
                    // 채널에 다음 줄이 들어올 때까지 대기합니다. false면 stdout EOF(채널 완료)입니다.
                    if (await Reader.WaitToReadAsync(TimeoutCts.Token))
                        Reader.TryRead(out Line);
                    else
                        bEof = true;
                }
                catch (OperationCanceledException)
                {
                    // 외부 취소(Stop/Esc/앱 종료)면 조용히 종료, 그 외엔 유휴 타임아웃(stall)으로 간주합니다.
                    if (Ct.IsCancellationRequested)
                        bCancelled = true;
                    else
                        bStalled = true;
                }
                catch (ChannelClosedException)
                {
                    // stdout drain Task가 예외로 채널을 완료한 경우(프로세스 종료 등) — EOF로 처리합니다.
                    bEof = true;
                }
            }

            if (bCancelled)
                yield break;

            if (bStalled)
            {
                yield return BuildStallFailure(IdleTimeout);
                yield break;
            }

            // EOF: 프로세스가 result 없이 종료됨
            if (bEof)
            {
                yield return BuildEofFailure();
                yield break;
            }

            if (Line is null || Line.Length == 0)
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

        // 인증: claude 로그인 자격증명(구독)을 사용합니다.

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

                case "stream_event":
                    // --include-partial-messages로 켜진 토큰 델타. 본문 텍스트/사고를 증분 방출한다.
                    // (완성된 assistant 메시지는 동일 텍스트를 다시 담으므로 거기선 text/thinking를 방출하지 않는다.)
                    AppendStreamEventDelta(Root, Items);
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

                case "rate_limit_event":
                    // 무시하지 않고 표면화: 레이트리밋/지연이 "그냥 멈춤"처럼 보이는 것을 막습니다.
                    Items.Add(BuildRateLimitNotice(Root));
                    break;

                // control_response(우리 init에 대한 응답) 등은 무시
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

            // text/thinking 블록은 stream_event 델타로 이미 증분 방출했으므로 여기서 다시 방출하지 않는다
            // (중복 누적 방지). tool_use는 델타의 부분 JSON이 취약하여 완성 메시지에서만 처리한다.
            if (BtEl.GetString() == "tool_use")
            {
                string Id = GetString(Block, "id") ?? "";
                string Name = GetString(Block, "name") ?? "";
                string InputJson = Block.TryGetProperty("input", out JsonElement InEl)
                    ? InEl.GetRawText() : "{}";
                Items.Add(new ClaudeStreamItem.ToolUse(Id, Name, InputJson));
            }
        }
    }

    /// <summary>
    /// stream_event(부분 메시지) 한 건에서 본문 텍스트/사고 델타를 증분 아이템으로 변환합니다.
    /// content_block_delta의 text_delta/thinking_delta만 취하고, 나머지(input_json_delta,
    /// signature_delta, message_*/content_block_start·stop 등)는 무시합니다.
    /// </summary>
    private static void AppendStreamEventDelta(JsonElement Root, List<ClaudeStreamItem> Items)
    {
        if (!Root.TryGetProperty("event", out JsonElement Event) || Event.ValueKind != JsonValueKind.Object)
            return;

        if (GetString(Event, "type") != "content_block_delta")
            return;

        if (!Event.TryGetProperty("delta", out JsonElement Delta) || Delta.ValueKind != JsonValueKind.Object)
            return;

        switch (GetString(Delta, "type"))
        {
            case "text_delta":
                if (GetString(Delta, "text") is { Length: > 0 } Text)
                    Items.Add(new ClaudeStreamItem.AssistantText(Text));
                break;

            case "thinking_delta":
                if (GetString(Delta, "thinking") is { Length: > 0 } Thinking)
                    Items.Add(new ClaudeStreamItem.Thinking(Thinking));
                break;
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

    /// <summary>rate_limit_event를 정보성 Notice로 변환합니다. 스키마가 버전마다 달라 원문을 축약 표면화합니다.</summary>
    private static ClaudeStreamItem.Notice BuildRateLimitNotice(JsonElement Root)
    {
        string Raw = Root.GetRawText();
        if (Raw.Length > 500)
            Raw = Raw[..500] + " …";

        return new ClaudeStreamItem.Notice($"⏳ 레이트리밋/지연 이벤트 수신 (응답이 지연될 수 있음): {Raw}");
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

        string PathHint = StdErrLogPath is null ? "" : $" (자세한 로그: {StdErrLogPath})";

        return new ClaudeStreamItem.Failure(string.IsNullOrEmpty(Err)
            ? $"claude 프로세스가 응답 없이 종료되었습니다.{PathHint}"
            : $"claude 오류: {Err}{PathHint}");
    }

    /// <summary>유휴 타임아웃(stall)에 도달했을 때 진단 정보를 담은 실패 아이템을 만듭니다.</summary>
    private ClaudeStreamItem.Failure BuildStallFailure(TimeSpan IdleTimeout)
    {
        string Err;
        lock (StdErr) Err = StdErr.ToString().Trim();

        string PathHint = StdErrLogPath is null ? "" : $" (자세한 로그: {StdErrLogPath})";
        string Detail = string.IsNullOrEmpty(Err) ? "" : $"\n최근 stderr: {Err}";

        return new ClaudeStreamItem.Failure(
            $"claude 응답이 {(int)IdleTimeout.TotalSeconds}초 동안 멈춰 턴을 중단했습니다. " +
            $"MCP 서버 연결 또는 모델 응답이 지연되었을 수 있습니다.{PathHint}{Detail}");
    }

    /// <summary>stdout 무출력 유휴 타임아웃을 결정합니다. UNREALAGENT_CLI_IDLE_TIMEOUT_SEC로 조정 가능합니다.</summary>
    private static TimeSpan ResolveIdleTimeout()
    {
        const int DefaultSeconds = 180;

        string? Raw = Environment.GetEnvironmentVariable("UNREALAGENT_CLI_IDLE_TIMEOUT_SEC");
        if (int.TryParse(Raw, out int Seconds) && Seconds > 0)
            return TimeSpan.FromSeconds(Seconds);

        return TimeSpan.FromSeconds(DefaultSeconds);
    }

    // ── stderr 실시간 로깅 ──

    /// <summary>stderr 로그 파일을 준비하고 턴 시작 헤더를 기록합니다.</summary>
    private void InitStdErrLog(string Model)
    {
        try
        {
            StdErrLogPath = Path.Combine(Path.GetTempPath(), "UnrealAgent", "cli-stderr.log");
            Directory.CreateDirectory(Path.GetDirectoryName(StdErrLogPath)!);
            File.AppendAllText(StdErrLogPath,
                $"{Environment.NewLine}=== turn start {DateTime.Now:yyyy-MM-dd HH:mm:ss} model={Model} ==={Environment.NewLine}");
        }
        catch
        {
            // 로깅 실패는 본 기능에 영향을 주지 않습니다.
            StdErrLogPath = null;
        }
    }

    /// <summary>stderr 한 줄을 타임스탬프와 함께 로그 파일에 덧붙입니다.</summary>
    private void AppendStdErrLog(string Line)
    {
        if (StdErrLogPath is null)
            return;

        try { File.AppendAllText(StdErrLogPath, $"[{DateTime.Now:HH:mm:ss}] {Line}{Environment.NewLine}"); }
        catch { /* 무시 */ }
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
