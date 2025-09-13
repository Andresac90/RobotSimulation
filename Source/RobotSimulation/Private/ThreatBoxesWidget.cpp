#include "ThreatBoxesWidget.h"
#include "Rendering/DrawElements.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

void UThreatBoxesWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Nothing required here; SetBoxes() drives repaints.
}

FVector2D UThreatBoxesWidget::ViewportToLocal(const FVector2D& ViewportPx, const FGeometry& Geo)
{
    // Get the current game viewport size (in screen pixels).
    FVector2D ViewportSize(0.f, 0.f);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }

    // If we have a real size, map viewport pixels -> normalized -> widget local.
    if (ViewportSize.X > 0.f && ViewportSize.Y > 0.f)
    {
        const FVector2D LocalSize = Geo.GetLocalSize();
        const FVector2D N(ViewportPx.X / ViewportSize.X, ViewportPx.Y / ViewportSize.Y);
        return FVector2D(N.X * LocalSize.X, N.Y * LocalSize.Y);
    }

    // Fallback: treat as absolute desktop pixels and convert.
    return Geo.AbsoluteToLocal(ViewportPx);
}

void UThreatBoxesWidget::ClampRectToLocal(FVector2D& TL, FVector2D& BR, const FVector2D& LocalSize)
{
    TL.X = FMath::Clamp(TL.X, 0.f, LocalSize.X);
    TL.Y = FMath::Clamp(TL.Y, 0.f, LocalSize.Y);
    BR.X = FMath::Clamp(BR.X, 0.f, LocalSize.X);
    BR.Y = FMath::Clamp(BR.Y, 0.f, LocalSize.Y);
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
    const FLinearColor C = BoxColor.CopyWithNewOpacity(1.f);
    const float W = FMath::Max(0.5f, Thickness);
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FMath::Clamp(TextSize, 8, 64));

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();

    // Modern API: build a paint geometry once for the whole widget area (lines)
    const FPaintGeometry FullPG = AllottedGeometry.ToPaintGeometry(
        FVector2f(LocalSize.X, LocalSize.Y),
        FSlateLayoutTransform()  // identity
    );

    for (const FThreatScreenBox& B : Boxes)
    {
        // Convert viewport pixel coords to widget-local
        FVector2D TL = ViewportToLocal(B.Min, AllottedGeometry);
        FVector2D BR = ViewportToLocal(B.Max, AllottedGeometry);

        // Ensure TL is top-left and BR is bottom-right
        if (TL.X > BR.X) Swap(TL.X, BR.X);
        if (TL.Y > BR.Y) Swap(TL.Y, BR.Y);

        // Clamp into the widget bounds to avoid drawing offscreen
        ClampRectToLocal(TL, BR, LocalSize);

        // Skip degenerate rects (can happen if fully offscreen)
        if ((BR - TL).SizeSquared() < KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const FVector2D TR(BR.X, TL.Y);
        const FVector2D BL(TL.X, BR.Y);

        // Draw 4 rectangle edges with the same paint geometry
        {
            // Top
            {
                TArray<FVector2D> P; P.Reserve(2);
                P.Add(TL); P.Add(TR);
                FSlateDrawElement::MakeLines(
                    OutDrawElements, LayerId,
                    FullPG,
                    P, ESlateDrawEffect::None, C, true, W);
            }
            // Right
            {
                TArray<FVector2D> P; P.Reserve(2);
                P.Add(TR); P.Add(BR);
                FSlateDrawElement::MakeLines(
                    OutDrawElements, LayerId,
                    FullPG,
                    P, ESlateDrawEffect::None, C, true, W);
            }
            // Bottom
            {
                TArray<FVector2D> P; P.Reserve(2);
                P.Add(BR); P.Add(BL);
                FSlateDrawElement::MakeLines(
                    OutDrawElements, LayerId,
                    FullPG,
                    P, ESlateDrawEffect::None, C, true, W);
            }
            // Left
            {
                TArray<FVector2D> P; P.Reserve(2);
                P.Add(BL); P.Add(TL);
                FSlateDrawElement::MakeLines(
                    OutDrawElements, LayerId,
                    FullPG,
                    P, ESlateDrawEffect::None, C, true, W);
            }
        }

        // Label (inside box, small margin)
        const float LabelPad = 4.f;
        const FVector2D LabelPos = FVector2D(TL.X + LabelPad, TL.Y + LabelPad);

        const FPaintGeometry TextPG = AllottedGeometry.ToPaintGeometry(
            FVector2f(1.f, 1.f),                         // not used for text sizing here
            FSlateLayoutTransform(FVector2f(LabelPos.X, LabelPos.Y))
        );

        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId + 1,
            TextPG,
            B.Label.IsEmpty() ? FText::FromString(TEXT("Threat")) : B.Label,
            Font,
            ESlateDrawEffect::None,
            C
        );
    }

    return LayerId + 2;
}
