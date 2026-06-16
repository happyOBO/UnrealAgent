#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;
class UWidget;

/**
 * UMG 위젯 블루프린트의 위젯 트리(UWidgetTree)를 네이티브로 편집하는 헬퍼입니다.
 *
 * Python `unreal` API에서는 UWidgetBlueprint::WidgetTree가 protected로 막혀 있어
 * 위젯 추가/삭제/재배치가 불가능합니다. 이 클래스는 C++ 에디터 API로 위젯 트리를
 * 직접 조작합니다. 위젯 BP의 이벤트 그래프 편집은 blueprint_modify(FBlueprintGraphEditor)가
 * 담당하며, 여기서는 트리 구조만 다룹니다.
 *
 * 모든 함수는 게임 스레드에서 호출되어야 합니다.
 */
class FWidgetTreeEditor
{
public:
	/** 위젯 블루프린트를 로드합니다. 엔진/cooked 경로는 차단합니다(LoadBlueprint 가드 재사용). */
	static UWidgetBlueprint* LoadWidgetBlueprint(const FString& BlueprintPath, FString& OutError);

	/**
	 * 위젯을 생성해 트리에 추가합니다.
	 * ParentName 비면 RootWidget으로 설정(루트가 없을 때), 있으면 해당 패널 위젯의 자식으로 추가.
	 */
	static bool AddWidget(UWidgetBlueprint* WidgetBP, const FString& WidgetType, const FString& WidgetName,
		const FString& ParentName, FString& OutError);

	/** 위젯을 트리에서 제거합니다. */
	static bool RemoveWidget(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FString& OutError);

	/** 위젯의 단순 속성을 설정합니다 (FProperty ImportText 경유). */
	static bool SetWidgetProperty(UWidgetBlueprint* WidgetBP, const FString& WidgetName, const FString& PropertyName,
		const FString& Value, FString& OutError);

	/** 위젯을 다른 패널 위젯의 자식으로 재배치합니다. */
	static bool ReparentWidget(UWidgetBlueprint* WidgetBP, const FString& WidgetName, const FString& NewParentName, FString& OutError);

	/** 위젯 트리 계층을 JSON 문자열로 직렬화합니다. */
	static FString ListWidgets(UWidgetBlueprint* WidgetBP);

private:
	/** 위젯 타입 이름을 UWidget 파생 UClass로 해석합니다. 실패 시 nullptr. */
	static UClass* ResolveWidgetClass(const FString& TypeName);
	/** WidgetTree에서 이름으로 위젯을 찾습니다. */
	static UWidget* FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName);
};
