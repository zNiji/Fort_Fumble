// tiny screen-space HP bar drawn above each enemy
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

UCLASS()
class FORT_FUMBLE_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetHealthPercent(float InPercent);

	UFUNCTION(BlueprintCallable, Category = "HealthBar")
	void SetBarVisible(bool bVisible);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float HealthPercent = 1.f;
	bool bBarVisible = true;
};
