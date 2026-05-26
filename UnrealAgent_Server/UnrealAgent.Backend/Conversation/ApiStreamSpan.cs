using System.Text;
using Anthropic.Models.Messages;
using UnrealAgent.Backend.Core;
using Block = UnrealAgent.Backend.Core.Block;

namespace UnrealAgent.Backend.Conversation;

/// <summary>
/// API 호출 1회의 스트리밍 응답을 파싱하고 누적합니다.
/// Process()로 이벤트를 먹이고, Complete()로 결과를 확정합니다.
/// </summary>
public sealed class ApiStreamSpan
{
    /// <summary>현재 진행 중인 블록의 종류와 상태입니다.</summary>
    private abstract record ActiveBlock 
    {
        /// <summary>텍스트 응답 블록입니다.</summary>
        public sealed record Text : ActiveBlock;

        /// <summary>사고 과정(Extended Thinking) 블록입니다.</summary>
        public sealed record Thinking : ActiveBlock;
    }
    
    // ── 스트리밍 중 상태 ──

    /// <summary>현재 처리 중인 블록입니다. null이면 블록 사이 유휴 상태입니다.</summary>
    private ActiveBlock? CurrentBlock;
    
    /// <summary>텍스트 델타를 누적하는 버퍼입니다.</summary>
    private readonly StringBuilder TextBuffer = new();

    /// <summary>사고 과정 델타를 누적하는 버퍼입니다.</summary>
    private readonly StringBuilder ThinkingBuffer = new();
    
    /// <summary>사고 블록의 서명입니다. 사고 델타 이후 별도 델타로 도착합니다.</summary>
    private string? ThinkingSignature;
    
    // ── 턴 완료 후 결과 ──

    /// <summary>메시지 레벨 종료 사유입니다 (EndSpan, ToolUse, PauseTurn 등).</summary>
    public StopReason? FinalStopReason;
    
    /// <summary>완성된 어시스턴트 콘텐츠 블록 목록입니다 (텍스트, 사고, 도구 호출).</summary>
    public IReadOnlyList<Block> Blocks => AssistantBlocks;
    private readonly List<Block> AssistantBlocks = [];
    
    // ── 일반 함수들 ──
    
    /// <summary>
    /// 스트리밍 이벤트 하나를 처리합니다.
    /// 클라이언트에 전달할 ChatEvent가 있으면 반환하고, 없으면 null을 반환합니다.
    /// </summary>
    public ChatEvent? Process(RawMessageStreamEvent Event)
    {
        // 1) 콘텐츠 블록 시작 — text / thinking 블록이 새로 열림
        if (Event.TryPickContentBlockStart(out RawContentBlockStartEvent? StartEvt))
            return ProcessBlockStart(StartEvt);

        // 2) 콘텐츠 블록 델타 — 텍스트 조각이 스트리밍으로 도착
        if (Event.TryPickContentBlockDelta(out RawContentBlockDeltaEvent? DeltaEvt))
            return ProcessDelta(DeltaEvt);

        // 3) 콘텐츠 블록 종료 — 하나의 블록 스트리밍 완료
        if (Event.TryPickContentBlockStop(out RawContentBlockStopEvent? BlockStopEvt))
            return ProcessBlockStop();

        // 4) 메시지 시작 — 응답 전체의 시작, 토큰 사용량(Usage) 등 메타 정보 포함
        if (Event.TryPickStart(out RawMessageStartEvent? StartMsgEvt))
            return ProcessMessageStart(StartMsgEvt);

        // 5) 메시지 델타 — 응답 종료 시점, stop_reason 등 최종 메타 정보 포함
        if (Event.TryPickDelta(out RawMessageDeltaEvent? MsgDelta))
            return ProcessMessageDelta(MsgDelta);

        return null;
    }

    /// <summary>
    /// 메시지 시작 이벤트를 처리합니다.
    /// 캐시 포함 전체 입력 토큰 수를 캡처합니다.
    /// </summary>
    private ChatEvent? ProcessMessageStart(RawMessageStartEvent StartMsgEvt)
    {
        return null;
    }
    
    /// <summary>
    /// 메시지 델타 이벤트를 처리합니다.
    /// 종료 사유를 캡처합니다.
    /// </summary>
    private ChatEvent? ProcessMessageDelta(RawMessageDeltaEvent MsgDelta)
    {
        if (MsgDelta.Delta.StopReason is { } Reason)
            FinalStopReason = Reason;

        return null;
    }

    /// <summary>
    /// 블록 시작 이벤트를 처리합니다.
    /// 블록 종류를 식별하여 CurrentBlock 상태를 설정합니다.
    /// </summary>
    private ChatEvent? ProcessBlockStart(RawContentBlockStartEvent StartEvt)
    {
        if (StartEvt.ContentBlock.TryPickText(out _))
            CurrentBlock = new ActiveBlock.Text();
        
        else if (StartEvt.ContentBlock.TryPickThinking(out _))
            CurrentBlock = new ActiveBlock.Thinking();
        
        return null;
    }

    /// <summary>
    /// 콘텐츠 블록 델타를 처리합니다.
    /// CurrentBlock 상태에 따라 적절한 버퍼에 누적하고, 텍스트/사고 델타는 ChatEvent로 즉시 반환합니다.
    /// </summary>
    private ChatEvent? ProcessDelta(RawContentBlockDeltaEvent DeltaEvt)
    {
        switch (CurrentBlock)
        {
            case ActiveBlock.Text when DeltaEvt.Delta.TryPickText(out TextDelta? TextDelta):
                TextBuffer.Append(TextDelta.Text);
                return new ChatEvent.Text(TextDelta.Text);
            
            case ActiveBlock.Thinking when DeltaEvt.Delta.TryPickThinking(out ThinkingDelta? ThinkingDelta):
                ThinkingBuffer.Append(ThinkingDelta.Thinking);
                return new ChatEvent.Thinking(ThinkingDelta.Thinking);
            
            // Signature 델타는 Thinking 블록 내에서 사고 델타 이후에 도착합니다.
            case ActiveBlock.Thinking when DeltaEvt.Delta.TryPickSignature(out SignatureDelta? SigDelta):
                ThinkingSignature = SigDelta.Signature;
                return null;
            
            default:
                return null;
        }
    }


    /// <summary>
    /// 블록 종료 이벤트를 처리합니다.
    /// 누적된 버퍼를 확정하여 AssistantBlocks에 도메인 Block으로 추가합니다.
    /// </summary>
    private ChatEvent? ProcessBlockStop()
    {
        switch (CurrentBlock)
        {
            case ActiveBlock.Text:
            {
                if (TextBuffer.Length > 0)
                {
                    AssistantBlocks.Add(new Block.Text(TextBuffer.ToString()));
                    TextBuffer.Clear();
                }
                break;
            }

            case ActiveBlock.Thinking:
            {
                if (ThinkingBuffer.Length > 0)
                {
                    AssistantBlocks.Add(new Block.Thinking(ThinkingBuffer.ToString(), ThinkingSignature));
                    ThinkingBuffer.Clear();
                    ThinkingSignature = null;
                }
                break;
            }
        }
        
        CurrentBlock = null;
        return null;
    }
}