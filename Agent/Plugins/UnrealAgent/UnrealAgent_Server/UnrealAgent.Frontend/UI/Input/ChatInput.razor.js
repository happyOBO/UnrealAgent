/**
 * textarea에 키 바인딩을 설정합니다.
 * 멘션 팝업 열림 시: ArrowUp/Down → 탐색, Enter → 선택, Tab → 드릴다운, ArrowLeft → 상위, Escape → 닫기.
 * 커맨드 팝업 열림 시: ArrowUp/Down → 탐색, Enter/Tab → 선택, Escape → 닫기.
 * 팝업 닫힘 시: Enter → 전송, Shift+Tab → 모드 순환.
 */
export function setupKeyBindings(textarea, dotNetRef)
{
    textarea.addEventListener("keydown", function (e)
    {
        // 멘션 팝업이 열려있을 때
        if (document.querySelector(".mention-popup"))
        {
            switch (e.key)
            {
                case "ArrowUp":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("MentionNavigate", -1);
                    return;
                case "ArrowDown":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("MentionNavigate", 1);
                    return;
                case "Enter":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("MentionSelect");
                    return;
                case "Tab":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("MentionTab");
                    return;
                case "ArrowLeft":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("MentionGoBack");
                    return;
                case "Escape":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("MentionClose");
                    return;
            }
        }

        // 커맨드 팝업이 열려있을 때
        if (document.querySelector(".command-popup"))
        {
            switch (e.key)
            {
                case "ArrowUp":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("PopupNavigate", -1);
                    return;
                case "ArrowDown":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("PopupNavigate", 1);
                    return;
                case "Tab":
                case "Enter":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("PopupSelect");
                    return;
                case "Escape":
                    e.preventDefault();
                    dotNetRef.invokeMethodAsync("PopupClose");
                    return;
            }
        }

        // 기본 키 바인딩
        if (e.key === "Enter" && !e.shiftKey)
        {
            e.preventDefault();
            textarea.closest("form").requestSubmit();
        }
        else if (e.key === "Tab" && e.shiftKey)
        {
            e.preventDefault();
            dotNetRef.invokeMethodAsync("CycleMode");
        }
    });
}