// paints a dark backplate + green/yellow/red fill so HP stays readable

#include "Enemy/EnemyHealthBarWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

void UEnemyHealthBarWidget::SetHealthPercent(float InPercent)
{
	HealthPercent = FMath::Clamp(InPercent, 0.f, 1.f);
}

void UEnemyHealthBarWidget::SetBarVisible(bool bVisible)
{
	bBarVisible = bVisible;
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

int32 UEnemyHealthBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	if (!bBarVisible)
	{
		return LayerId;
	}

	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	if (Size.X < 1.f || Size.Y < 1.f)
	{
		return LayerId;
	}

	// dark outline plate
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform()),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor(0.05f, 0.05f, 0.05f, 0.85f));

	const float InnerPad = 2.f;
	const FVector2f FillOrigin(InnerPad, InnerPad);
	const FVector2f FillMax(FMath::Max(0.f, Size.X - InnerPad * 2.f), FMath::Max(0.f, Size.Y - InnerPad * 2.f));
	const FVector2f FillSize(FillMax.X * HealthPercent, FillMax.Y);

	// green when healthy, yellow mid, red when nearly dead
	FLinearColor FillColor(0.2f, 0.9f, 0.25f, 1.f);
	if (HealthPercent < 0.35f)
	{
		FillColor = FLinearColor(0.95f, 0.2f, 0.15f, 1.f);
	}
	else if (HealthPercent < 0.65f)
	{
		FillColor = FLinearColor(0.95f, 0.85f, 0.15f, 1.f);
	}

	if (FillSize.X > 0.5f)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FillSize, FSlateLayoutTransform(FillOrigin)),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FillColor);
	}

	return LayerId + 1;
}
