#include "ThreatBoxesWidget.h"
#include "Rendering/DrawElements.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"

int32 UThreatBoxesWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const FLinearColor C = BoxColor;
    const float W = Thickness;
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", TextSize);

    for (const FThreatScreenBox& B : Boxes)
    {
        // Convert viewport pixels to local widget space (handles DPI)
        const FVector2D TL = AllottedGeometry.AbsoluteToLocal(B.Min);
        const FVector2D BR = AllottedGeometry.AbsoluteToLocal(B.Max);
        const FVector2D TR(BR.X, TL.Y);
        const FVector2D BL(TL.X, BR.Y);

        // 4 lines rectangle
        {
            TArray<FVector2D> P;
            P.Add(TL); P.Add(TR);
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, W);
            P.Reset(); P.Add(TR); P.Add(BR);
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, W);
            P.Reset(); P.Add(BR); P.Add(BL);
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, W);
            P.Reset(); P.Add(BL); P.Add(TL);
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, W);
        }

        // Label (top-left, small margin) — uses the older overload (fine on UE 5.5)
        const FVector2D LabelPos = TL + FVector2D(4.f, -(TextSize + 4.f));
        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId + 1,
            AllottedGeometry.ToPaintGeometry(LabelPos, FVector2D(1.f, 1.f)), // <— old API
            B.Label.ToString(),
            Font,
            ESlateDrawEffect::None,
            C
        );
    }

    return LayerId + 2;
}
