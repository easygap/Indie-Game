#pragma once

#include "CoreMinimal.h"

/**
 * Shared screen-space contract for the title and pause menus.
 *
 * The native Canvas HUD and pointer hit testing must consume the same metrics.
 * Keeping that math here prevents the common failure where a responsive visual
 * moves while its invisible mouse target remains at a hard-coded 1080p row.
 */
namespace IGFrontendMenuLayout
{
	constexpr int32 ActionCount = 6;

	/**
	 * §9 「밤 5」 — 있을 수 없는 슬롯.
	 *
	 * 액션 목록의 **마지막**이지만 화면에서는 「이어하기」 바로 밑에 놓인다.
	 * 이 분리가 요점이다: 기존 액션 인덱스가 하나도 움직이지 않으므로 확인
	 * 디스패치와 계약이 그대로 남고, 플레이어에게는 이어하기 목록에 행 하나가
	 * 조용히 늘어난 것으로 보인다. 저장 파일은 만들지 않는다(§14).
	 */
	constexpr int32 NightFiveAction = 5;

	/**
	 * 화면에서의 자리. 액션 순서와 다르며, 위에서 아래로:
	 * 이어하기 · 밤 5 · 새 게임 · 설정 · 제작 정보 · 게임 종료.
	 */
	constexpr int32 ScreenOrder[ActionCount] = {0, 2, 3, 4, 5, 1};

	struct FMetrics
	{
		FVector2D CanvasSize = FVector2D::ZeroVector;
		float Scale = 1.0f;
		float ContentLeft = 0.0f;
		float ContentWidth = 0.0f;
		float TitleTop = 0.0f;
		float MessageTop = 0.0f;
		float MenuTop = 0.0f;
		float RowHeight = 0.0f;
		float RowGap = 0.0f;
		float FooterTop = 0.0f;

		float GetRowStride() const
		{
			return RowHeight + RowGap;
		}

		FVector2D GetRowPosition(const int32 VisibleSlot) const
		{
			return FVector2D(
				ContentLeft,
				MenuTop + VisibleSlot * GetRowStride());
		}

		FBox2D GetRowHitBox(const int32 VisibleSlot) const
		{
			const FVector2D Position = GetRowPosition(VisibleSlot);
			return FBox2D(
				FVector2D(Position.X - 36.0f * Scale, Position.Y),
				FVector2D(
					Position.X + ContentWidth,
					Position.Y + RowHeight));
		}
	};

	inline FMetrics MakeMetrics(const float CanvasWidth, const float CanvasHeight)
	{
		FMetrics Result;
		Result.CanvasSize = FVector2D(CanvasWidth, CanvasHeight);
		Result.Scale = FMath::Clamp(
			FMath::Min(CanvasWidth / 1920.0f, CanvasHeight / 1080.0f),
			0.67f,
			2.0f);
		Result.ContentLeft = FMath::Max(
			54.0f * Result.Scale,
			CanvasWidth * 0.078f);
		Result.ContentWidth = FMath::Clamp(
			CanvasWidth * 0.30f,
			300.0f * Result.Scale,
			470.0f * Result.Scale);
		Result.TitleTop = FMath::Max(
			70.0f * Result.Scale,
			CanvasHeight * 0.105f);
		Result.MenuTop = FMath::Max(
			282.0f * Result.Scale,
			CanvasHeight * 0.34f);
		Result.MessageTop = Result.MenuTop - 48.0f * Result.Scale;
		Result.RowHeight = FMath::Max(44.0f, 54.0f * Result.Scale);
		Result.RowGap = 8.0f * Result.Scale;
		Result.FooterTop = CanvasHeight - FMath::Max(
			38.0f,
			44.0f * Result.Scale);
		return Result;
	}

	inline bool HidesContinue(
		const bool bTitleMenu,
		const bool bCanContinue)
	{
		return bTitleMenu && !bCanContinue;
	}

	/**
	 * 밤 5는 타이틀에서만, 그리고 엔딩 B를 본 세이브가 있을 때만 보인다.
	 * 일시정지 메뉴에는 절대 나타나지 않는다 — 메타 개입은 본편 바깥이다.
	 */
	inline bool HidesNightFive(
		const bool bTitleMenu,
		const bool bNightFiveAvailable)
	{
		return !bTitleMenu || !bNightFiveAvailable;
	}

	inline bool IsActionHidden(
		const int32 ActionRow,
		const bool bTitleMenu,
		const bool bCanContinue,
		const bool bNightFiveAvailable)
	{
		if (ActionRow == 0)
		{
			return HidesContinue(bTitleMenu, bCanContinue);
		}
		if (ActionRow == NightFiveAction)
		{
			return HidesNightFive(bTitleMenu, bNightFiveAvailable);
		}
		return false;
	}

	inline int32 GetVisibleActionCount(
		const bool bTitleMenu,
		const bool bCanContinue,
		const bool bNightFiveAvailable = false)
	{
		int32 Count = 0;
		for (int32 ActionRow = 0; ActionRow < ActionCount; ++ActionRow)
		{
			Count += IsActionHidden(
				ActionRow,
				bTitleMenu,
				bCanContinue,
				bNightFiveAvailable)
				? 0
				: 1;
		}
		return Count;
	}

	inline int32 GetVisibleSlotForAction(
		const int32 ActionRow,
		const bool bTitleMenu,
		const bool bCanContinue,
		const bool bNightFiveAvailable = false)
	{
		if (ActionRow < 0 || ActionRow >= ActionCount)
		{
			return INDEX_NONE;
		}
		if (IsActionHidden(
				ActionRow,
				bTitleMenu,
				bCanContinue,
				bNightFiveAvailable))
		{
			return INDEX_NONE;
		}
		// Count the visible rows that sit above this one on screen. Works for any
		// combination of hidden rows without a special case per combination.
		int32 VisibleSlot = 0;
		for (int32 Other = 0; Other < ActionCount; ++Other)
		{
			if (Other == ActionRow
				|| IsActionHidden(
					Other,
					bTitleMenu,
					bCanContinue,
					bNightFiveAvailable))
			{
				continue;
			}
			VisibleSlot += ScreenOrder[Other] < ScreenOrder[ActionRow] ? 1 : 0;
		}
		return VisibleSlot;
	}

	inline int32 GetActionForVisibleSlot(
		const int32 VisibleSlot,
		const bool bTitleMenu,
		const bool bCanContinue,
		const bool bNightFiveAvailable = false)
	{
		for (int32 ActionRow = 0; ActionRow < ActionCount; ++ActionRow)
		{
			if (GetVisibleSlotForAction(
					ActionRow,
					bTitleMenu,
					bCanContinue,
					bNightFiveAvailable)
				== VisibleSlot)
			{
				return ActionRow;
			}
		}
		return INDEX_NONE;
	}

	inline int32 HitTestAction(
		const FMetrics& Metrics,
		const FVector2D& PointerPosition,
		const bool bTitleMenu,
		const bool bCanContinue,
		const bool bNightFiveAvailable = false)
	{
		const int32 VisibleCount = GetVisibleActionCount(
			bTitleMenu,
			bCanContinue,
			bNightFiveAvailable);
		for (int32 VisibleSlot = 0; VisibleSlot < VisibleCount; ++VisibleSlot)
		{
			if (Metrics.GetRowHitBox(VisibleSlot).IsInside(PointerPosition))
			{
				return GetActionForVisibleSlot(
					VisibleSlot,
					bTitleMenu,
					bCanContinue,
					bNightFiveAvailable);
			}
		}
		return INDEX_NONE;
	}

	inline bool ValidateMetrics(const FMetrics& Metrics)
	{
		const float LastRowBottom = Metrics.MenuTop
			+ (ActionCount - 1) * Metrics.GetRowStride()
			+ Metrics.RowHeight;
		return Metrics.CanvasSize.X > 0.0f
			&& Metrics.CanvasSize.Y > 0.0f
			&& Metrics.RowHeight >= 44.0f
			&& Metrics.ContentLeft >= 0.0f
			&& Metrics.ContentLeft + Metrics.ContentWidth <= Metrics.CanvasSize.X
			&& LastRowBottom < Metrics.FooterTop;
	}
}
