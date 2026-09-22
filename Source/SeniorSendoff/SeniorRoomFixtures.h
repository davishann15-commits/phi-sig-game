#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"

// Room fixtures are drawn in the same fixed camera plane as the back wall.
// Contents stay live Slate text and controls, rather than baked screenshots.
class SSeniorRoomFixture:public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSeniorRoomFixture):_Rack(false),_Neon(false){} SLATE_ARGUMENT(bool,Rack) SLATE_ARGUMENT(bool,Neon) SLATE_END_ARGS()
    void Construct(const FArguments& A){bRack=A._Rack;bNeon=A._Neon;SetVisibility(EVisibility::HitTestInvisible);}
    FVector2D ComputeDesiredSize(float) const override{return FVector2D(400,480);}
    int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& E,int32 L,const FWidgetStyle&,bool)const override
    {
        const FVector2f S(G.GetLocalSize());
        const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
        static const FSlateRoundedBoxBrush Round(FLinearColor::White,4.f);
        auto Box=[&](float X,float Y,float W,float H,FLinearColor C,int32 Layer=0,bool R=false)
        {FSlateDrawElement::MakeBox(E,L+Layer,G.ToPaintGeometry(FVector2f(W,H),FSlateLayoutTransform(FVector2f(X,Y))),R?&Round:Brush,ESlateDrawEffect::None,C);};
        if(bNeon)
        {
            // Broad reflected red light lands on the masonry, behind the glass.
            for(int32 I=18;I>=0;--I)
                Box(-I*2,-I,S.X+I*4,S.Y+I*2,FLinearColor(.8,.015,.004,.006),0,true);
            Box(5,8,S.X,S.Y,FLinearColor(.005,.004,.003,.3),1,true);
            Box(0,0,S.X,S.Y,FLinearColor(.07,.035,.025,.31),2,true);
            Box(1,1,S.X-2,1,FLinearColor(.74,.38,.22,.45),3);
            Box(1,S.Y-2,S.X-2,2,FLinearColor(.08,.016,.009,.8),3);
            for(float X:{12.f,S.X-18})for(float Y:{12.f,S.Y-18})
            {
                Box(X+3,Y+5,9,9,FLinearColor(.006,.004,.002,.7),3,true);
                Box(X,Y,8,8,FLinearColor(.32,.21,.15,1),4,true);
                Box(X+2,Y+2,4,4,FLinearColor(.65,.46,.33,1),5,true);
                Box(X+3,Y+2,1,4,FLinearColor(.08,.05,.03,1),6);
            }
            // A real lead exits the sign and bends down into its wall transformer.
            TArray<FVector2f> Wire={FVector2f(20,S.Y),FVector2f(20,S.Y+6),FVector2f(14,S.Y+12),FVector2f(14,S.Y+19)};
            FSlateDrawElement::MakeLines(E,L+3,G.ToPaintGeometry(),Wire,ESlateDrawEffect::None,FLinearColor(.018,.012,.009,1),true,3);
            Box(4,S.Y+17,24,23,FLinearColor(.075,.065,.05,1),4,true);
            Box(6,S.Y+19,20,19,FLinearColor(.14,.115,.085,1),5,true);
            Box(14,S.Y+23,3,3,FLinearColor(.48,.30,.20,1),6,true);
            return L+7;
        }
        // The central overhead tube throws the two fixture shadows outward and
        // downward. A dark near-contact edge prevents the floating-card look.
        const float Away=bRack?1.f:-1.f;
        for(int32 I=11;I>=0;--I)
            Box(Away*(10+I*.6f)-I*.4f,12-I*.3f,S.X+I*.8f,S.Y+I*.8f,FLinearColor(.012,.009,.004,.033),0,true);
        Box(Away*3,4,S.X,S.Y,FLinearColor(.018,.012,.006,.6),0,true);
        // Exposed mounting tabs continue above the frame into expansion bolts.
        for(float X:{24.f,S.X-43})
        {
            Box(X+Away*5,-19,22,36,FLinearColor(.01,.008,.005,.4),0,true);
            Box(X,-24,19,36,FLinearColor(.16,.15,.12,1),1,true);
            Box(X+2,-23,15,34,FLinearColor(.39,.36,.28,1),1,true);
            Box(X+4,-19,11,11,FLinearColor(.12,.11,.08,1),2,true);
            Box(X+6,-17,7,7,FLinearColor(.61,.55,.40,1),3,true);
            Box(X+8,-16,2,5,FLinearColor(.12,.10,.065,1),4);
        }
        // Solid side and underside reveals show the board's thickness.
        Box(bRack?S.X-1:-7,5,8,S.Y,FLinearColor(.095,.077,.05,1),1);
        Box(bRack?0:-7,S.Y-1,S.X+7,8,FLinearColor(.065,.05,.03,1),1);
        Box(0,0,S.X,S.Y,FLinearColor(.16,.15,.12,1),1,true);
        Box(2,2,S.X-4,S.Y-4,FLinearColor(.42,.41,.35,1),1,true);
        Box(7,7,S.X-14,S.Y-14,bRack?FLinearColor(.14,.115,.075,1):FLinearColor(.66,.645,.54,1),2);
        // The top fluorescent gives the board a quiet vertical light falloff.
        for(int32 I=0;I<40;++I)Box(8,8+I*(S.Y-16)/40,S.X-16,(S.Y-16)/40+1,FLinearColor(.07,.045,.01,.0015f*I),3);
        Box(1,1,S.X-2,2,FLinearColor(.65,.61,.49,.9),4);
        Box(1,S.Y-3,S.X-2,3,FLinearColor(.045,.035,.025,1),4);
        Box(7,7,S.X-14,3,FLinearColor(.16,.14,.095,.45),4);
        Box(7,10,2,S.Y-20,FLinearColor(.18,.16,.11,.3),4);
        if(bRack)
        {
            for(float X=21;X<S.X-15;X+=18)for(float Y=24;Y<S.Y-15;Y+=18)
            { Box(X,Y,3,4,FLinearColor(.025,.021,.014,.85),4,true);Box(X,Y+3,3,1,FLinearColor(.36,.28,.15,.6),4); }
            for(int32 Row=0;Row<2;++Row)for(int32 Side=0;Side<2;++Side)
            {const float Y=Row==0?140.f:303.f;const float X=S.X*(Row==0?(Side==0?.43f:.57f):(Side==0?.39f:.65f));
             Box(X+2,Y+3,7,31,FLinearColor(0,0,0,.4),5,true);Box(X,Y,5,27,FLinearColor(.49,.47,.4,1),5,true);Box(X,Y+23,17,5,FLinearColor(.38,.37,.33,1),5,true);}
        }
        else
        {
            // Faint wiped-marker streaks; no noise over the typography.
            for(int32 I=0;I<9;++I)Box(25+(I*47)%120,160+I*34,115+(I*23)%120,1,FLinearColor(.19,.24,.23,.045),4);
            Box(10,S.Y-2,S.X-20,10,FLinearColor(.25,.25,.22,1),5);
            Box(10,S.Y-2,S.X-20,2,FLinearColor(.6,.58,.48,1),5);
            Box(34,S.Y-9,56,7,FLinearColor(.11,.11,.095,1),6,true);
            Box(111,S.Y-7,42,5,FLinearColor(.12,.22,.31,1),6,true);
            Box(154,S.Y-7,10,5,FLinearColor(.56,.055,.035,1),6,true);
            Box(S.X-95,S.Y-15,58,13,FLinearColor(.08,.105,.09,1),6,true);
        }
        for(float X:{4.f,S.X-9})for(float Y:{5.f,S.Y-10})
        {Box(X,Y,5,5,FLinearColor(.11,.105,.085,1),6,true);Box(X+1,Y+2,3,1,FLinearColor(.61,.6,.51,1),7);}
        return L+7;
    }
private:bool bRack=false,bNeon=false;
};
