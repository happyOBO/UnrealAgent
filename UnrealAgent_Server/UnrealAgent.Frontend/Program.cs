using Anthropic.Models.Messages;
using Microsoft.Extensions.DependencyInjection;
using UnrealAgent.Backend.Auth;
using UnrealAgent.Backend.Conversation;
using UnrealAgent.Backend.Core;
using Block = UnrealAgent.Backend.Core.Block;

ServiceCollection Services = new ServiceCollection();
Services.AddSingleton<AuthConfig>();

ServiceProvider Provider = Services.BuildServiceProvider();
AuthConfig Auth = Provider.GetRequiredService<AuthConfig>();

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

while (true)
{
    Console.Write("\n> ");
    string? Input = Console.ReadLine();

    if (string.IsNullOrWhiteSpace(Input)) 
        continue;
    
    if (Input.Equals("exit", StringComparison.OrdinalIgnoreCase)) 
        break;

    var Parameters = new MessageCreateParams
    {
        Model = "claude-opus-4-6",
        MaxTokens = 1024,
        Messages = [new() { Role = Role.User, Content = Input }],
        Thinking = new ThinkingConfigAdaptive(),
        OutputConfig = new OutputConfig()
        {
            Effort = Effort.High
        }
    };

    ApiStreamSpan Span = new ApiStreamSpan();
    await foreach (RawMessageStreamEvent Event in Auth.Client!.Messages.CreateStreaming(Parameters))
    {
        switch (Span.Process(Event))
        {
            case ChatEvent.Thinking Think:
                Console.Write(Think.Content);
                break;
            
            case ChatEvent.Text Txt:
                Console.Write(Txt.Content);
                break;
        }
    }
    
    Console.WriteLine();

    Console.WriteLine("\n--- 완료된 블록 ---");
    foreach (Block B in Span.Blocks)
    {
        switch (B)
        {
            case Block.Thinking T:
                Console.WriteLine($"Thinking : {T.Content} {T.Signature}");
                break;
            
            case Block.Text T:
                Console.WriteLine($"Text : {T.Content}");
                break;
        }
    }

    Console.WriteLine(Span.FinalStopReason);
}
