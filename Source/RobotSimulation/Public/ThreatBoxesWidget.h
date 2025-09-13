#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreatScreenBox.h"
#include "ThreatBoxesWidget.generated.h"

/**
 * Draws red rectangles + labels for threats projected to screen space.
 * Works in Shipping builds and handles DPI/viewport scaling correctly.
 */
UCLASS()
class ROBOTSIMULATION_API UThreatBoxesWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
    FLinearColor BoxColor = FLinearColor(1, 0, 0, 1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "0.5", ClampMax = "6.0"))
    float Thickness = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (ClampMin = "8", ClampMax = "64"))
    int32 TextSize = 16;

    UFUNCTION(BlueprintCallable, Category = "Threats")
    void SetBoxes(const TArray<FThreatScreenBox>& InBoxes)
    {
        Boxes = InBoxes;
        Invalidate(EInvalidateWidget::Paint);
    }

protected:
    virtual void NativeConstruct() override;

    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

private:
    // Incoming Min/Max are viewport-relative pixels (PlayerViewportRelative = true).
    static FVector2D ViewportToLocal(const FVector2D& ViewportPx, const FGeometry& Geo);
    static void ClampRectToLocal(FVector2D& TL, FVector2D& BR, const FVector2D& LocalSize);

    TArray<FThreatScreenBox> Boxes;
};
