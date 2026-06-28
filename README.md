# UnrealAgent

UnrealAgent는 Unreal Editor 안에서 동작하는 AI 코딩 에이전트 플러그인이다. 에디터에 채팅
패널을 띄우고(`Alt+F2`), 에이전트가 블루프린트·UMG 위젯·애님 그래프·레벨 액터를 직접
편집하거나 Python·콘솔 명령 실행, 뷰포트 캡처, 로그 조회까지 에디터 기능을 도구로 다루게
한다. 목표는 "에디터를 손으로 클릭하는 작업"을 대화로 시키는 것이다.

## 구성

- **에디터 플러그인 (C++)** — `Agent/Plugins/UnrealAgent`. 채팅 패널(`SAgentChatBrowser`)과
  단축키(`Alt+F2`)를 제공하고, 에디터가 켜질 때 MCP 서버(`UMcpServer`, 기본 포트 55559,
  `settings.local.json`에서 변경)를 띄워 네이티브 도구를 노출한다.
- **네이티브 도구 (`FMcpTool`)** — 에이전트가 호출하는 에디터 연산. `execute_python`,
  `run_console_command`, `get_output_log`, `capture_viewport`, `get_level_actors`,
  `actor_modify`, `blueprint_modify`, `anim_blueprint_modify`, `widget_modify`,
  `montage_modify`, `aim_offset_modify`. 그래프 배선이나 위젯 트리처럼 Python으로
  다루기 까다로운 편집은 전용 도구로 처리한다.
- **백엔드 오케스트레이터** — `claude` CLI를 턴마다 구동해 모델과 대화하고, MCP 서버에
  HTTP로 붙어 도구 호출을 중계한다. 시스템 프롬프트는 프로젝트의 contexts와 skills에서
  조립된다.

## 지식 주입: Context와 Skill

프로젝트 단위로 두 종류의 마크다운을 주입한다. 둘은 발동 방식과 담는 내용이 다르다.

| 구분 | `contexts/*.md` | Skill (`SKILL.md`) |
|------|-----------------|--------------------|
| 발동 | 사용자 메시지의 키워드에 자동 매칭 | 사용자가 `/명령`을 직접 입력 |
| 내용 | UE Python API 등 참조 지식 | 작업 절차·체크리스트 |
| 용도 | "무엇을 아는가" | "어떻게 할 것인가" |

## Skill 작성 가이드

### Skill이란

Skill은 사용자가 `/<skill-name>`을 입력하면 그 본문이 `<system-reminder>` 형태로 user
메시지에 끼어 들어가는 워크플로 템플릿이다. 반복되는 멀티스텝 작업을 슬래시 명령 하나로
묶어두고, 프로젝트 컨벤션과 검증 절차를 매번 똑같이 강제하는 용도다.

모델이 알아서 부르지는 않는다. Claude Code의 `Skill` 도구는 모델이 필요하다고 판단해
자동 호출하지만, UnrealAgent의 Skill은 사용자가 직접 `/명령`을 쳐야 발동한다.

지식은 contexts가 이미 자동으로 넣어주므로, Skill 본문에 API 설명을 다시 늘어놓을 필요는
없다. 절차와 규칙, 검증에만 집중하면 된다.

### 파일 위치

스킬은 프로젝트 단위로 저장된다.

```
<언리얼프로젝트루트>/.unrealagent/skills/<skill-name>/SKILL.md
```

`skills/` 아래 각 디렉토리에 `SKILL.md`를 하나씩 두면 에이전트 시작 시 자동으로
스캔·등록된다(`SkillRegistry.DiscoverSkills`). 스캔은 시작할 때 한 번만 돌기 때문에, 새
스킬을 추가했다면 에이전트를 재시작해야 인식된다.

### SKILL.md 형식

YAML 프론트매터와 마크다운 본문으로 이뤄진다.

```markdown
---
name: widget-ui
description: UMG 위젯을 생성·수정한다. 위젯 트리·바인딩·시각 검증까지 한 번에 수행.
---

이 작업은 완료까지 연속으로 수행한다. ...
(절차/체크리스트 본문)
```

프론트매터 키는 다음과 같다. 그 외 키는 무시된다.

| 키 | 기본값 | 의미 |
|----|--------|------|
| `name` | (필수) | 슬래시 명령 이름. `/name`으로 호출. |
| `description` | (필수) | 한 줄 설명. 자동완성 팝업·목록에 표시. |
| `user-invocable` | `true` | `false`면 `/` 자동완성 팝업에서 숨김. |
| `disable-model-invocation` | `false` | 현재 모델 자동호출을 지원하지 않아 실효 없음. |

### 본문 작성 패턴

본문은 결국 "이 절차대로, 이 도구로, 이렇게 검증하라"를 적는 자리다. 네 가지만 챙기면 된다.

첫째, 첫 줄에 연속 수행을 지시한다. 베이스 프롬프트가 "한 번에 한 작업"으로 단발 실행을
유도하기 때문에, 멀티스텝 스킬은 첫 줄에서 "이 작업은 완료까지 연속으로 수행한다. 한
단계만 하고 멈추지 말 것"처럼 명시해 그 제약을 풀어줘야 한다.

둘째, 쓸 도구를 이름으로 못박는다. `widget_modify`, `blueprint_modify`, `capture_viewport`,
`get_output_log`, `run_console_command`, `execute_python`, `actor_modify`,
`get_level_actors` 등 사용할 네이티브 도구를 정확히 적고, 무엇이 불가능한지도 같이 적는다
(예: 위젯 트리는 Python으로 못 건드리니 `widget_modify`를 쓴다).

셋째, 프로젝트 컨벤션을 본문에 박아둔다. 네이밍 접두사(`WBP_`, `B_`, `SM_` …), 중복 생성
금지, 기존 GameplayTag·코드 컨벤션 준수 같은 규칙을 적어두면 매번 다시 입력할 필요가 없다.

넷째, 검증 단계를 넣는다. 변경 후 `capture_viewport`와 `get_output_log`(경고·에러 포함)로
스스로 확인하고 결과를 보고하게 한다.

### 절차를 적되, 지식은 적지 않는다

좋은 본문은 도구와 순서, 검증이 있는 절차다. 예를 들어 "1) `widget_modify list_widgets`로
현재 트리 확인, 2) 패널부터 `add_widget`, 3) 클릭 로직은 `blueprint_modify`
ComponentBoundEvent, 4) `capture_viewport`로 검증" 같은 식이다.

반대로 "UMG의 `unreal.WidgetTree` API는 다음 함수를 제공한다 …" 처럼 API 지식을 다시
나열하는 본문은 피한다. 그건 contexts/*.md가 할 일이고, 토큰만 잡아먹는다.

### 호출 방법

채팅 입력창에 `/`를 치면 등록된 스킬이 자동완성으로 뜬다. 스킬 이름 뒤에 작업 내용을 한
줄로 붙여 호출하면, 그 텍스트가 스킬 본문(절차 지침) 뒤에 함께 모델로 전달된다.

```
/widget-ui WBP_HealthBar에 체력 ProgressBar 추가해줘
/task-doc Docs/INVENTORY_SYSTEM_KR.md 기반으로 진행
/fix-build  (이어서 컴파일 에러 붙여넣기)
```

인자 없이 `/widget-ui`만 입력하면 절차 지침만 주입되고, 다음 메시지로 작업을 이어가면 된다.

후속 메시지에서 `/`를 다시 칠 필요는 없다. 한 번 주입된 스킬 지침은 대화 기록에 남아 같은
세션 동안 계속 적용된다. 다시 호출하면 본문이 재주입될 뿐이고, 이는 `/compact`로 앞
내용이 압축돼 사라졌거나 다른 스킬로 전환할 때만 의미가 있다.

### 기본 제공 스킬 (Shooter 프로젝트)

| 스킬 | 용도 |
|------|------|
| `/widget-ui` | UMG 위젯(HUD/크로스헤어) 생성·수정·바인딩·시각 검증 |
| `/anim-graph` | 애님 BP 그래프 배선(상태머신·AimOffset·턴인플레이스/스파인 트위스트)·전이 조건·컴파일 검증 |
| `/task-doc` | Docs 문서 기반 구현 + 착수 전 컨벤션·중복 점검 |
| `/fix-build` | C++ 컴파일 에러 진단·수정·재빌드 검증 |
| `/diagnose-crash` | 런타임 크래시(Access violation·GAS ASC) 근거 기반 분석 |
| `/debug-clean` | 임시 디버그 로그·CVar·주석 제거 정리 |
| `/verify` | 변경 후 뷰포트 캡처 + 전체 로그로 자가검증 |


## Dev-Block 모드

### Dev-Block이란

UnrealAgent의 네이티브 MCP 도구는 아직 개발 단계라 역량 공백(capability gap)이 있다.
**Dev-Block 모드**는 개발 단계 전용 토글로, 켜면 에이전트가 *도구 자체의 한계*에
막혔을 때 **우회(workaround)하지 않고, 무엇이 막혔는지 기록한 뒤 작업을 중단**한다.

> 일반적인 사용자 실수(잘못된 에셋 경로·오타)나 재시도로 풀리는 일시적 실패에는
> 적용되지 않는다. **도구가 그 연산을 지원하지 못할 때만** 발동한다.

이를 통해 `execute_python`으로 전용 도구가 해야 할 일을 억지로 때우거나, 사용자에게
"수동으로 하세요"라고 떠넘기는 식의 우회를 막고, **개선이 필요한 도구의 공백을
산출물로 남긴다**.

### 켜는 방법

`<언리얼프로젝트루트>/.unrealagent/settings.local.json` 에 `devBlockMode` 키를 둔다:

```json
{
  "devBlockMode": true
}
```

- 키가 없거나 `false`면 비활성(기본값).
- 시스템 프롬프트는 **매 턴 재생성**되므로, 토글을 바꾸면 **다음 메시지부터 즉시** 반영된다(재시작 불필요).

### 막혔을 때 일어나는 일

도구 역량에 막히면 에이전트는:

1. 작업을 **중단**한다(우회 시도 금지).
2. `Write` 도구로 `.unrealagent/dev-blocked/<task-slug>.md` 에 기록을 남긴다:

   ```markdown
   # <task title>
   - status: blocked
   - blocked_tool: <도구 이름> / op: <연산>
   - error: <도구가 보고한 에러·한계 그대로>
   - updated: <YYYY-MM-DD>

   ## Goal
   <사용자가 원래 요청한 것>

   ## Completed
   - <도구로 이미 끝낸 부분>

   ## Remaining (after the tool is fixed)
   - <막힌 지점부터 완료까지 남은 단계>
   ```
3. 어떤 도구·연산이 왜 막혔는지 사용자에게 알리고 턴을 끝낸다.

> 기록 폴더 `.unrealagent/dev-blocked/` 는 `.gitignore`에 등록돼 있어 커밋되지 않는다(개발 단계 산출물).

### 막힌 작업 재개·해결

- **세션 복원 시 자동 점검**: Dev-Block 모드에서 이전 세션을 복원하면, 첫 턴에
  에이전트가 `.unrealagent/dev-blocked/` 의 `status: blocked` 기록을 읽고, 도구가
  고쳐졌다면 재개할지 사용자에게 먼저 묻는다.
- **해결 처리**: 막혔던 작업이 끝나면 해당 기록을
  `.unrealagent/dev-blocked/resolved/<slug>.md` 로 **옮기고**, `status: blocked` →
  `status: resolved` 로 바꾼 뒤 `resolved: <YYYY-MM-DD>` 줄을 추가한다.
  최상위 폴더에는 **열린(`status: blocked`) 기록만** 남긴다.


## 클래스 다이어그램

```mermaid
classDiagram
    class FUnrealAgentModule {
        <<IModuleInterface>>
        +StartupModule()
        +ShutdownModule()
        -OnSpawnChatTab()
        -OnToggleChatPanel()
    }
    class FAgentChatInputProcessor {
        <<IInputProcessor>>
        +HandleKeyDownEvent()
    }
    class FAgentChatCommands {
        <<TCommands>>
        +ToggleChatPanel (Alt+F2)
    }
    class SAgentChatBrowser {
        <<SCompoundWidget>>
        +LoadServerUrl()
        -WebBrowserWidget
    }

    class UMcpServer {
        <<UEditorSubsystem>>
        +Initialize()
        +Deinitialize()
        +StartServer() Frontend.exe
        -HandleMcpRequest()
        -DiscoverTools()
        -BuildInputSchema()
        -ExecuteTool()
        -PopulateToolParams()
        -ToolMap : Map~string,UScriptStruct~
    }
    class FUnrealAgentSettings {
        <<static>>
        +GetMcpServerPort()
        +GetFrontendPort()
    }
    class FAgentLogCapture {
        <<FOutputDevice>>
    }

    class FMcpTool {
        <<USTRUCT base>>
        +ToolDescription() FString
        +Execute() FMcpResponse
        +Args
    }
    class FMcpResponse {
        <<USTRUCT>>
        +Success()
        +Failure()
        +SuccessImage()
    }

    class FExecutePythonTool
    class FRunConsoleCommandTool
    class FGetOutputLogTool
    class FCaptureViewportTool
    class FGetLevelActorsTool
    class FActorModifyTool
    class FBlueprintModifyTool
    class FAnimBlueprintModifyTool
    class FWidgetModifyTool
    class FMontageModifyTool
    class FAimOffsetModifyTool

    class FBlueprintGraphEditor {
        <<static>>
    }
    class FAnimBlueprintEditor
    class FWidgetTreeEditor
    class FMontageEditor
    class FAimOffsetEditor
    class ActorToolHelpers

    FUnrealAgentModule --> FAgentChatInputProcessor
    FUnrealAgentModule --> FAgentChatCommands
    FUnrealAgentModule --> SAgentChatBrowser
    FAgentChatInputProcessor ..> FAgentChatCommands : Alt+F2
    UMcpServer ..> FUnrealAgentSettings : 포트
    SAgentChatBrowser ..> FUnrealAgentSettings : URL
    UMcpServer ..> FMcpTool : discover/execute
    UMcpServer --> FMcpResponse

    FMcpTool <|-- FExecutePythonTool
    FMcpTool <|-- FRunConsoleCommandTool
    FMcpTool <|-- FGetOutputLogTool
    FMcpTool <|-- FCaptureViewportTool
    FMcpTool <|-- FGetLevelActorsTool
    FMcpTool <|-- FActorModifyTool
    FMcpTool <|-- FBlueprintModifyTool
    FMcpTool <|-- FAnimBlueprintModifyTool
    FMcpTool <|-- FWidgetModifyTool
    FMcpTool <|-- FMontageModifyTool
    FMcpTool <|-- FAimOffsetModifyTool

    FGetOutputLogTool ..> FAgentLogCapture
    FGetLevelActorsTool ..> ActorToolHelpers
    FActorModifyTool ..> ActorToolHelpers
    FBlueprintModifyTool ..> FBlueprintGraphEditor
    FAnimBlueprintModifyTool ..> FAnimBlueprintEditor
    FWidgetModifyTool ..> FWidgetTreeEditor
    FMontageModifyTool ..> FMontageEditor
    FAimOffsetModifyTool ..> FAimOffsetEditor
```