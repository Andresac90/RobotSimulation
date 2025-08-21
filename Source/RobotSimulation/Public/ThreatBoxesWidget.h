#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreatScreenBox.h"
#include "ThreatBoxesWidget.generated.h"

UCLASS()
class ROBOTSIMULATION_API UThreatBoxesWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
    FLinearColor BoxColor = FLinearColor(1, 0, 0, 1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
    float Thickness = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
    int32 TextSize = 16;

    UFUNCTION(BlueprintCallable, Category = "Threats")
    void SetBoxes(const TArray<FThreatScreenBox>& InBoxes)
    {
        Boxes = InBoxes;
        Invalidate(EInvalidateWidget::LayoutAndVolatility);
    }

protected:
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    TArray<FThreatScreenBox> Boxes;
};
