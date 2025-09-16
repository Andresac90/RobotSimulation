#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreatScreenBox.h"
#include "ThreatBoxesWidget.generated.h"

/** Draws rectangles + labels for threats. Expects Min/Max in this widget's local space (DPI-correct). */
UCLASS()
class ROBOTSIMULATION_API UThreatBoxesWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Threats")
    void SetBoxes(const TArray<FThreatScreenBox>& InBoxes);

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

    // ------------ Visuals ------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor BoxColor = FLinearColor(1.f, 0.f, 0.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (ClampMin = "0.5", ClampMax = "10.0"))
    float BoxThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FSlateFontInfo LabelFont;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor LabelColor = FLinearColor(1.f, 0.f, 0.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (ClampMin = "0.0"))
    float LabelPadding = 4.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Threats")
    TArray<FThreatScreenBox> Boxes;
};
