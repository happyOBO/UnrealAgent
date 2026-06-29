using UnrealAgent.Backend.Agent;
using UnrealAgent.Backend.Agent.Middleware;
using UnrealAgent.Backend.ClaudeCode;
using UnrealAgent.Backend.Command;
using UnrealAgent.Backend.Command.Commands;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Mcp;
using UnrealAgent.Backend.Model;
using UnrealAgent.Backend.Model.Models;
using UnrealAgent.Backend.Prompt;
using UnrealAgent.Backend.Prompt.Context;
using UnrealAgent.Backend.Skill;
using UnrealAgent.Backend.Tool;
using UnrealAgent.Backend.Tool.Tools;
using UnrealAgent.Frontend.Infrastructure;

// ── 포트 결정 ──
// 기본값은 settings.local.json(frontendPort)에서 읽습니다. 포트의 단일 출처이며 UE 플러그인도 같은 파일을 읽습니다.
// 팀원 모드에서는 커맨드라인 --port가 우선합니다.
int Port = AppSettings.GetFrontendPort();

for (int i = 0; i < args.Length - 1; i++)
{
    if (args[i] == "--port")
    {
        Port = int.Parse(args[i + 1]);
        break;
    }
}

// ── WebApplicationBuilder (서비스 등록 + 앱 설정을 담는 빌더) 생성 ──
WebApplicationBuilder Builder = WebApplication.CreateBuilder(args);

// ── Kestrel (요청을 받아서 넘겨주는 서버 엔진) 포트 설정 ──
Builder.WebHost.UseUrls($"http://localhost:{Port}");

// ── 정적 웹 자산 강제 로드 ──
Builder.WebHost.UseStaticWebAssets();

// ── Blazor 서비스 등록 (Razor 컴포넌트 + 서버 측 인터랙티브 모드) ──
Builder.Services.AddRazorComponents().AddInteractiveServerComponents();

// ── SignalR 메시지 크기 제한 (이미지 Base64 전송을 위해 30MB로 확장) ──
Builder.Services.AddSignalR(Options => Options.MaximumReceiveMessageSize = 30 * 1024 * 1024);

// ── Agent 모듈 (에이전트 루프 + 세션) ──
Builder.Services.AddSingleton<AgentSession>();
Builder.Services.AddSingleton<AgentLoop>();
Builder.Services.AddSingleton<SessionStore>();

// ── AgentRunner (메시지 큐 + 에이전트 루프 서비스) ──
Builder.Services.AddSingleton<AgentRunner>();
Builder.Services.AddHostedService(Sp => Sp.GetRequiredService<AgentRunner>());

// ── Runtime 모듈 ──
Builder.Services.AddSingleton<PromptBuilder>();
Builder.Services.AddSingleton<ContextRegistry>();

// ── Tool 모듈 + 백엔드 MCP 서버 (team/skill 등 C# 도구를 CLI에 노출) ──
Builder.Services.AddSingleton<ToolRegistry>();
Builder.Services.AddSingleton<BackendMcpServer>();

// ── Claude Code(CLI) 모듈 ──
Builder.Services.AddSingleton<ClaudeCliLocator>();
Builder.Services.AddSingleton<CliMcpConfig>();

// ── Command 모듈 ──
Builder.Services.AddSingleton<CommandRegistry>();

// ── Skill 모듈 ──
Builder.Services.AddSingleton<SkillRegistry>();

// ── Claude 모델 레지스트리 & 런타임 설정 ──
Builder.Services.AddSingleton<ModelRegistry>();
Builder.Services.AddSingleton<ModelSettings>();

// ── Agent 미들웨어 파이프라인 ──
Builder.Services.AddSingleton<SlashCommandMiddleware>();

// 여기까지 서비스 등록 단계. Build() 이후는 미들웨어/라우팅 설정 단계입니다.
WebApplication App = Builder.Build();

// ── 어트리뷰트 기반 자동 스캔 ──
App.Services.GetRequiredService<ToolRegistry>().DiscoverTools(typeof(TeamTool).Assembly);
App.Services.GetRequiredService<ModelRegistry>().DiscoverModels(typeof(Opus46).Assembly);
App.Services.GetRequiredService<CommandRegistry>().DiscoverCommands(typeof(ClearCommand).Assembly);

// ── 스킬 파일시스템 스캔 ──
App.Services.GetRequiredService<SkillRegistry>().DiscoverSkills();

// ── UE 도메인 컨텍스트 문서 스캔 ──
App.Services.GetRequiredService<ContextRegistry>().DiscoverContexts();

// ── 팀 인자 파싱 (팀원 모드일 때 TeamName, AgentName, ParentPid 설정) ──
App.Services.GetRequiredService<AgentSession>().Team.ParseArgs(args);

// ── 설정 로드 ──
App.Services.GetRequiredService<ModelSettings>().Load();

// ── CLI용 MCP 설정 파일 생성 ──
// settings.local.json의 UE MCP 서버 + 백엔드 자기 자신(/agent-mcp)을 claude CLI
// --mcp-config 형식으로 변환합니다. UE 네이티브 도구와 team/skill 도구를 모두 CLI에 노출합니다.
App.Services.GetRequiredService<CliMcpConfig>().Build(Port);

// ── 팀원 모드: iframe 임베딩 허용 ──
if (App.Services.GetRequiredService<AgentSession>().Team.ParentPid is not null)
{
    App.Use(async (HttpContext Context, Func<Task> Next) =>
    {
        Context.Response.OnStarting(() =>
        {
            Context.Response.Headers.Remove("X-Frame-Options");
            Context.Response.Headers["Content-Security-Policy"] = "frame-ancestors http://localhost:*";
            return Task.CompletedTask;
        });
        await Next();
    });
}

// ── 미들웨어 파이프라인 ──
App.UseStaticFiles();
App.UseAntiforgery();

// ── Blazor 엔드포인트 (Razor 컴포넌트 라우팅 + 서버 렌더 모드 적용) ──
App.MapRazorComponents<App>().AddInteractiveServerRenderMode();

// ── 백엔드 MCP 엔드포인트 (claude CLI가 HTTP JSON-RPC로 team/skill 도구를 호출) ──
// id 없는 notification은 본문 없는 202 Accepted로 응답합니다(핸드셰이크 보존).
App.MapPost("/agent-mcp", async (HttpContext Context, BackendMcpServer Server, CancellationToken Ct) =>
{
    using StreamReader Reader = new(Context.Request.Body);
    string Body = await Reader.ReadToEndAsync(Ct);

    System.Text.Json.Nodes.JsonNode? Response = await Server.HandleAsync(Body, Ct);

    return Response is null
        ? Results.StatusCode(StatusCodes.Status202Accepted)
        : Results.Content(Response.ToJsonString(), "application/json");
});

// ── 서버 실행 (요청 수신 대기 시작) - http://localhost:55558/ ──
App.Run();