# UnrealAgent ( Unreal Engine 5.7.4 ver)

UnrealAgent는 Unreal Editor 안에서 동작하는 AI 코딩 에이전트 플러그인이다. 에디터에 채팅
패널을 띄우고(`Alt+F2`), 에이전트가 블루프린트·UMG 위젯·애님 그래프·레벨 액터를 직접
편집하거나 Python·콘솔 명령 실행, 뷰포트 캡처, 로그 조회까지 에디터 기능을 도구로 다루게
한다. 목표는 "에디터를 손으로 클릭하는 작업"을 대화로 시키는 것이다.

> **프로젝트 배경** — 이 프로젝트는 인프런 강의 *UnrealAgent*를 기반으로, 강의에서 다룬
> 구조를 직접 학습·확장한 결과물이다. 강의 원본에서 다음을 수정·확장했다.
>
> - **LLM 사용 방식** — 강의의 Anthropic Console API 직접 호출 방식을 **Claude Code 기반으로
>   전환**했다. 자체 LLM 호출 루프를 제거하고 에이전트의 추론·도구·권한 모델을 Claude Code에
>   위임해, 모델 선택(Opus 4.8 / Sonnet 4.6 등)과 effort 조절을 그대로 활용한다.
> - **MCP 도구 확장** — 강의 기본 도구(`execute_python`) 에 더해 블루프린트·애님 BP·UMG 위젯·몽타주·Aim
>   Offset 등 에디터 편집 도구를 추가·보강하고, Backend가 노출하는 `skill`·`team` 도구로
>   워크플로 기능까지 확장했다.
> - **워크플로 기능** — Skill / Team Agent / Context 자동 주입 / 세션 저장·복원·초기화
>   (`/clear`) / Dev-Block 모드 등을 직접 설계·구현해 얹었다.

## 개요

UnrealAgent는 **Claude Code 기반**이다. 자체 LLM 호출 루프를 두지 않고, 로컬에 설치된
`claude` CLI를 턴마다 구동해 모델과 대화한다. 에이전트의 기본 역량(파일 도구, 추론, 권한
모델 등)은 Claude Code가 그대로 제공하고, UnrealAgent는 그 위에 ▲에디터를 조작하는 MCP
도구 ▲Unreal 도메인 시스템 프롬프트 ▲스킬·컨텍스트·팀 같은 워크플로 기능을 얹는다.

모델은 Claude Code가 지원하는 것을 그대로 쓴다. 채팅 UI에서 **Opus 4.8 / Opus 4.6 /
Sonnet 4.6 / Haiku 4.5 / Fable 5** 중 선택할 수 있고, 지원 모델은 effort(추론 강도) 조절도
가능하다.

## 구성

- **에디터 플러그인 (C++)** — `Agent/Plugins/UnrealAgent/Source`. 채팅 패널
  (`SAgentChatBrowser`)과 단축키(`Alt+F2`)를 제공하고, 에디터가 켜질 때 MCP 서버
  (`UMcpServer`)를 띄워 네이티브 에디터 도구를 노출한다.
- **오케스트레이터 (C#)** — `Agent/Plugins/UnrealAgent/UnrealAgent_Server`. `claude` CLI를
  구동해 모델 대화를 중계하는 **Backend**와, 채팅 패널에 표시되는 Blazor 기반 **Frontend**로
  나뉜다. 시스템 프롬프트는 Claude Code 기본 프롬프트에 UnrealAgent 도메인 지침·스킬 목록을
  덧붙여(`--append-system-prompt-file`) 조립한다.
- **MCP 도구** — 에디터 플러그인이 노출하는 네이티브 도구와 Backend가 노출하는 도구
  (`skill`, `team`)를 `--mcp-config`로 Claude Code에 전달한다. 사용자 개인 MCP 서버·스킬은
  `--strict-mcp-config` / `--disable-slash-commands`로 차단해 동작을 격리한다.

## MCP 도구

에이전트가 에디터를 조작하는 네이티브 도구다. 그래프 배선이나 위젯 트리처럼 Python으로
다루기 까다로운 편집은 전용 도구로 처리한다.

| 도구 | 역할 |
|------|------|
| `execute_python` | 에디터 안에서 `unreal` Python API 실행 |
| `run_console_command` | 콘솔 명령 실행 |
| `get_output_log` | 출력 로그 조회(경고·에러 포함) |
| `capture_viewport` | 뷰포트 스크린샷 캡처 — 시각 검증용 |
| `get_level_actors` | 현재 레벨의 액터 목록·속성 조회 |
| `actor_modify` | 레벨 액터 생성·삭제·속성/트랜스폼 편집 |
| `blueprint_modify` | 블루프린트 그래프·변수·함수 편집 (Break/Make Struct, FormatText 등 포함) |
| `anim_blueprint_modify` | 애님 BP 그래프·상태머신·Transform(Modify) Bone 편집 |
| `widget_modify` | UMG 위젯 트리·바인딩 편집 |
| `montage_modify` | 애님 몽타주 편집 |
| `aim_offset_modify` | Aim Offset 편집 |

이와 별개로 Backend가 `skill`·`team` 도구를 MCP로 함께 노출해, 모델이 스킬 실행이나 팀
관리를 도구 호출로 수행할 수 있게 한다.

## 기능

### Skill

반복되는 멀티스텝 작업을 슬래시 명령 하나로 묶어두는 워크플로 템플릿이다. 채팅에서
`/<skill-name>`을 입력하면 해당 스킬 본문(절차·체크리스트)이 메시지에 주입되고, 모델이
`skill` 도구로 직접 호출할 수도 있다. 스킬은 프로젝트의
`<프로젝트>/.unrealagent/skills/<name>/SKILL.md`에 두면 시작 시 자동 등록된다.

기본 제공 스킬(Shooter 프로젝트): `/widget-ui`, `/anim-graph`, `/task-doc`, `/plan-doc`,
`/fix-build`, `/diagnose-crash`, `/debug-clean`, `/verify`.

### Team Agent

병렬 작업을 위해 에이전트 팀을 운영하는 기능이다. 리더가 `team` 도구로 팀을 만들고
(`create`), 역할·작업을 지정해 팀원을 스폰하면(`spawn`) 각 팀원이 별도 백엔드 프로세스로
떠서 자기 작업을 수행한다. 팀원은 메일박스를 통해 리더·다른 팀원과 메시지를 주고받고
(`message`/`broadcast`), 결과를 리더 대화에 보고한다. 작업이 끝나면 팀원 종료(`shutdown`)
나 팀 삭제(`delete`)로 정리한다. 팀 생성은 사용자가 명시적으로 요청할 때만 이뤄진다.

### Context 자동 주입

`<프로젝트>/.unrealagent/contexts/*.md`에 둔 참조 지식(UE Python API 등)을 사용자 메시지의
키워드에 맞춰 자동으로 시스템 프롬프트에 끼워 넣는다. "무엇을 아는가"를 채우는 자리로,
사용자가 직접 호출하는 스킬("어떻게 할 것인가")과 역할이 다르다.

### 슬래시 커맨드

| 커맨드 | 동작 |
|--------|------|
| `/clear` | 대화 내역과 CLI 세션을 초기화하고 저장된 세션 파일을 삭제(새 세션 시작) |
| `/compact` | 컨텍스트를 요약·압축(Claude Code 내장 `/compact`에 위임). 추가 지시 가능 |

### 세션 저장·복원

대화는 CLI 세션 ID와 UI 메시지가 함께 디스크에 스냅샷으로 저장된다. 에디터를 껐다 켜도
직전 세션을 그대로 이어갈 수 있다. 채팅 UI에서 **Restore**로 저장된 세션을 복원(CLI는
`--resume`, UI 메시지는 화면에 재표시)하고, **New Session**으로 새로 시작한다.

### 실행 모드

| 모드 | 동작 |
|------|------|
| Normal | 권한이 필요한 도구는 실행 전 사용자에게 확인 |
| Edit | 도구 실행 자동 승인(플러그인/프로젝트 밖 편집 가드는 유지) |
| Plan | 도구 실행을 차단하고 분석·계획만 수행 |

### Dev-Block 모드

네이티브 MCP 도구의 역량 공백을 산출물로 드러내기 위한 개발용 토글이다. 켜두면 에이전트가
도구 한계에 막혔을 때 우회하지 않고 `.unrealagent/dev-blocked/`에 무엇이 막혔는지 기록한 뒤
멈춘다. `settings.local.json`의 `devBlockMode`로 켠다(기본 비활성).

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
