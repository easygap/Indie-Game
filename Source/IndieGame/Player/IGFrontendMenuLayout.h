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
	constexpr int32 ActionCount = 5;

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

	inline int32 GetVisibleActionCount(
		const bool bTitleMenu,
		const bool bCanContinue)
	{
		return ActionCount - (HidesContinue(bTitleMenu, bCanContinue) ? 1 : 0);
	}

	inline int32 GetVisibleSlotForAction(
		const int32 ActionRow,
		const bool bTitleMenu,
		const bool bCanContinue)
	{
		if (ActionRow < 0 || ActionRow >= ActionCount)
		{
			return INDEX_NONE;
		}
		if (!HidesContinue(bTitleMenu, bCanContinue))
		{
			return ActionRow;
		}
		return ActionRow == 0 ? INDEX_NONE : ActionRow - 1;
	}

	inline int32 GetActionForVisibleSlot(
		const int32 VisibleSlot,
		const bool bTitleMenu,
		const bool bCanContinue)
	{
		const int32 VisibleCount = GetVisibleActionCount(
			bTitleMenu,
			bCanContinue);
		if (VisibleSlot < 0 || VisibleSlot >= VisibleCount)
		{
			return INDEX_NONE;
		}
		return HidesContinue(bTitleMenu, bCanContinue)
			? VisibleSlot + 1
			: VisibleSlot;
	}

	inline int32 HitTestAction(
		const FMetrics& Metrics,
		const FVector2D& PointerPosition,
		const bool bTitleMenu,
		const bool bCanContinue)
	{
		const int32 VisibleCount = GetVisibleActionCount(
			bTitleMenu,
			bCanContinue);
		for (int32 VisibleSlot = 0; VisibleSlot < VisibleCount; ++VisibleSlot)
		{
			if (Metrics.GetRowHitBox(VisibleSlot).IsInside(PointerPosition))
			{
				return GetActionForVisibleSlot(
					VisibleSlot,
					bTitleMenu,
					bCanContinue);
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
