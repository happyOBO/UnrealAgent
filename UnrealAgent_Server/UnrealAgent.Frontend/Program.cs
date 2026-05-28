using Anthropic.Models.Messages;
using Microsoft.Extensions.DependencyInjection;
using UnrealAgent.Backend.Agent;
using UnrealAgent.Backend.Auth;
using UnrealAgent.Backend.Conversation;
using UnrealAgent.Backend.Core;
using UnrealAgent.Backend.Prompt;
using UnrealAgent.Backend.Tool;
using UnrealAgent.Backend.Tool.Tools;
using Block = UnrealAgent.Backend.Core.Block;

var Services = new ServiceCollection();

Services.AddHttpClient("OAuth", C => C.Timeout = TimeSpan.FromSeconds(30));

// ── Auth 모듈 ──
Services.AddSingleton<AuthConfig>();

// ── Agent 모듈 (에이전트 루프 + 세션) ──
Services.AddSingleton<AgentSession>();

// ── Runtime 모듈 ──
Services.AddSingleton<PromptBuilder>();

// ── Tool 모듈 ──
Services.AddSingleton<ToolRegistry>();
Services.AddSingleton<ToolExecutor>();

var Provider = Services.BuildServiceProvider();

var Auth = Provider.GetRequiredService<AuthConfig>();
var AgentSession = Provider.GetRequiredService<AgentSession>();
var PromptBuilder = Provider.GetRequiredService<PromptBuilder>();
Provider.GetRequiredService<ToolRegistry>().DiscoverTools(typeof(WebSearch).Assembly);
var ToolExecutor = Provider.GetRequiredService<ToolExecutor>();

// ── 로직 ──

Auth.Load();
if (!Auth.IsApiKeyConfigured())
{
    Console.Write("API Key를 입력하세요: ");
    string? Key = Console.ReadLine();

    if (string.IsNullOrWhiteSpace(Key))
    {
        Console.WriteLine("API Key가 입력되지 않았습니다.");
        return;
    }

    Auth.SetApiKey(Key);
    Console.WriteLine("API Key 저장 완료!");
}

// 대화 루프
while (true)
{
    // 사용자 입력 대기
    Console.Write("\n> ");
    string? Input = Console.ReadLine();

    if (string.IsNullOrWhiteSpace(Input))
        continue;

    if (Input.Equals("exit", StringComparison.OrdinalIgnoreCase))
        break;

    // 대화 히스토리에 사용자 입력 추가
    MessageSpan CurrentMessageSpan = AgentSession.Conversation.AddMessageSpan(Input);

    // 에이전트 루프: 도구 실행이 필요하면 API를 반복 호출
    bool bContinue = true;
    while (bContinue)
    {
        // API 요청 파라미터 구성
        MessageCreateParams Parameters = PromptBuilder.Build(AgentSession);

        // 스트리밍 응답 수신 및 출력
        ApiStreamSpan ApiStreamSpan = new ApiStreamSpan();
        await foreach (RawMessageStreamEvent Event in Auth.Client!.Messages.CreateStreaming(Parameters))
        {
            switch (ApiStreamSpan.Process(Event))
            {
                case ChatEvent.Text Txt:
                    Console.Write(Txt.Content);
                    break;
            }
        }

        // 완료된 응답을 대화 히스토리에 저장
        switch (ApiStreamSpan.Complete())
        {
            case ApiStreamSpan.Result.EndSpan { CompletedSpan: { } AssistantSpan }:
            {
                CurrentMessageSpan.AssistantSpans.Add(AssistantSpan);
                bContinue = false;
                
                break;
            }

            case ApiStreamSpan.Result.ExecuteTools { CompletedSpan: { } AssistantSpan, ToolCalls: { } ToolCalls }:
            {
                CurrentMessageSpan.AssistantSpans.Add(AssistantSpan);

                // 도구 실행
                foreach (Block.ToolUse ToolCall in ToolCalls)
                {
                    await foreach (ChatEvent Evt in ToolExecutor.ExecuteAsync(ToolCall, AssistantSpan, AgentSession))
                    {
                        if (Evt is ChatEvent.ToolStart Tool)
                            Console.WriteLine($"\n-- {Tool.Name} : {Tool.Input} 도구 사용 --");
                    }
                }

                // 도구 결과를 포함하여 다음 API 호출로 이어감
                break;
            }

            case ApiStreamSpan.Result.Continue { CompletedSpan: { } AssistantSpan }:
            {
                CurrentMessageSpan.AssistantSpans.Add(AssistantSpan);
                
                // 잘린 응답을 이어서 생성
                break;
            }
        }
    }
    
    Console.WriteLine();
}
