namespace UnrealAgent.Frontend.Infrastructure;

/// <summary>
/// 여러 컴포넌트에서 공유하는 의미 기반(semantic) 상태 색상입니다.
/// 동일 hex가 여러 곳에 흩어지지 않도록 단일 출처로 모읍니다.
/// (Tailwind는 Play CDN으로 런타임 DOM을 스캔하므로 보간된 클래스 문자열도 정상 동작합니다.)
///
/// 컴포넌트 고유의 세부 음영(예: ToolBlock의 배경/테두리 틴트)은 각 컴포넌트의 테마이므로 여기 두지 않습니다.
/// </summary>
public static class ChatColors
{
    /// <summary>성공/완료/정상 — 녹색.</summary>
    public const string Success = "#4ba96c";

    /// <summary>경고/진행 중/주의 — 주황색.</summary>
    public const string Warning = "#d68a51";

    /// <summary>에러/위험 — 빨간색.</summary>
    public const string Error = "#e05e5e";
}
