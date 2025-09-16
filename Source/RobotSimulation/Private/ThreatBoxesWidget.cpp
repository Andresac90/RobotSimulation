#include "ThreatBoxesWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UThreatBoxesWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!LabelFont.HasValidFont())
    {
        LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);
    }
}

void UThreatBoxesWidget::SetBoxes(const TArray<FThreatScreenBox>& InBoxes)
{
    Boxes = InBoxes;
    Invalidate(EInvalidateWidget::Paint); // repaint this frame
}

int32 UThreatBoxesWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (Boxes.Num() == 0) return BaseLayer;

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f) return BaseLayer;

    // Preserve all accumulated transforms from parent/layout
    const FPaintGeometry PG = AllottedGeometry.ToPaintGeometry();
    const float W = FMath::Max(0.5f, BoxThickness);

    auto ClampLocal = [&](FVector2D& P)
        {
            P.X = FMath::Clamp(P.X, 0.f, LocalSize.X);
            P.Y = FMath::Clamp(P.Y, 0.f, LocalSize.Y);
        };

    const FSlateFontInfo UseFont = LabelFont.HasValidFont()
        ? LabelFont
        : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);

    for (const FThreatScreenBox& B : Boxes)
    {
        FVector2D TL = B.Min;  // already in this widget's local space
        FVector2D BR = B.Max;

        if (TL.X > BR.X) Swap(TL.X, BR.X);
        if (TL.Y > BR.Y) Swap(TL.Y, BR.Y);

        ClampLocal(TL);
        ClampLocal(BR);
        if ((BR - TL).SizeSquared() < KINDA_SMALL_NUMBER) continue;

        const FVector2D TR(BR.X, TL.Y);
        const FVector2D BL(TL.X, BR.Y);

        // 4 edges
        {
            TArray<FVector2D> P; P.Reserve(2);

            P = { TL, TR };
            FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, PG, P, ESlateDrawEffect::None, BoxColor, true, W);
            P = { TR, BR };
            FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, PG, P, ESlateDrawEffect::None, BoxColor, true, W);
            P = { BR, BL };
            FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, PG, P, ESlateDrawEffect::None, BoxColor, true, W);
            P = { BL, TL };
            FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, PG, P, ESlateDrawEffect::None, BoxColor, true, W);
        }

        if (!B.Label.IsEmpty())
        {
            const FVector2D LabelPos(TL.X + LabelPadding, TL.Y + LabelPadding);
            const FPaintGeometry TextPG = AllottedGeometry.ToPaintGeometry(
                FVector2f(1.f, 1.f),
                FSlateLayoutTransform(FVector2f(LabelPos))
            );

            FSlateDrawElement::MakeText(
                OutDrawElements,
                BaseLayer + 2,
                TextPG,
                B.Label,
                UseFont,
                ESlateDrawEffect::None,
                LabelColor
            );
        }
    }

    return BaseLayer + 3;
}
