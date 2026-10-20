from pathlib import Path
P=Path(__file__).resolve().parent
root=P.parents[2]
s=(root/'Source/SeniorSendoff/HouseFraternityRow.cpp').read_text()
s=s.replace('#include "HouseSurroundingsPolish.h"','#include "HouseSurroundingsPolish.h"\n#include "HouseExteriorSiteLayout.h"\n#include "HouseBlackHall.h"\n#include "Materials/MaterialInstanceDynamic.h"\n#include "Misc/CommandLine.h"\n#include "Misc/Parse.h"')
s=s.replace('void Letters(FHallMesh& M, FVector C, int Style)','void LegacyLetters(FHallMesh& M, FVector C, int Style)',1)
needle='void House(FHallMesh& M, float X, float Y, float W, float D, float H, int Style)'
glyph=r'''
void Word(FHallMesh& M,FVector C,int32 HouseIndex)
{
    // Original four-letter fictional combinations. 0Omega,1Xi,2Lambda,3Eta,4Psi,5Rho.
    const int32 Words[5][4]={{0,1,2,3},{4,3,1,5},{3,1,0,5},{2,0,3,1},{5,1,3,4}};
    for(int32 I=0;I<4;++I)
    {
        auto Stroke=[&](float A,float B,float D,float E){M.Cylinder(TEXT("EX_FratLetter"),C+FVector((I-1.5f)*100.f+A,0,B),C+FVector((I-1.5f)*100.f+D,0,E),4,6,UV);};
        switch(Words[HouseIndex][I])
        {
        case 0:
            for(int K=0;K<18;++K){float A=FMath::DegreesToRadians(-35.f+K*250.f/18.f),B=FMath::DegreesToRadians(-35.f+(K+1)*250.f/18.f);Stroke(FMath::Cos(A)*29,32+FMath::Sin(A)*30,FMath::Cos(B)*29,32+FMath::Sin(B)*30);}
            Stroke(-34,0,-10,0);Stroke(10,0,34,0);break;
        case 1:Stroke(-30,64,30,64);Stroke(-23,32,23,32);Stroke(-30,0,30,0);break;
        case 2:Stroke(-30,0,0,64);Stroke(0,64,30,0);break;
        case 3:Stroke(-27,0,-27,64);Stroke(27,0,27,64);Stroke(-27,32,27,32);break;
        case 4:Stroke(0,0,0,64);Stroke(-28,64,-28,35);Stroke(-28,35,0,22);Stroke(0,22,28,35);Stroke(28,35,28,64);break;
        default:Stroke(-26,0,-26,64);Stroke(-26,64,20,64);Stroke(20,64,30,50);Stroke(30,50,20,34);Stroke(20,34,-26,34);break;
        }
    }
}
void Letters(FHallMesh& M,FVector C,int Style)
{
    if(!Enabled()){LegacyLetters(M,C,Style);return;}
    Word(M,C,Style==2?0:Style==1?1:2);
}
'''
s=s.replace(needle,glyph+'\n'+needle)
s=s.replace('    Box(M,Stone,{L-12,F-12,Ground-45},{R+12,B+12,Ground+90});\n    Box(M,Body,{L,F,Ground+80},{R,B,Top});','    if(!Enabled()) { Box(M,Stone,{L-12,F-12,Ground-45},{R+12,B+12,Ground+90});\n    Box(M,Body,{L,F,Ground+80},{R,B,Top}); }')
s=s.replace('    Box(M,Stone,{PL-55,PF-75,Ground-25},{PR+55,F+10,Deck});\n    for (int Step=0;Step<5;++Step) Box(M,Stone,{X-205,PF-290+Step*43,Ground-10},{X+205,PF-242+Step*43,Ground+10+Step*16});','    if(!Enabled()) { Box(M,Stone,{PL-55,PF-75,Ground-25},{PR+55,F+10,Deck});\n    for (int Step=0;Step<5;++Step) Box(M,Stone,{X-205,PF-290+Step*43,Ground-10},{X+205,PF-242+Step*43,Ground+10+Step*16}); }')
# No permanent fake leaf/panel can remain across a real opening.
start=s.index('    Box(M,Trim,{X-117,F-13,Deck}')
end=s.index('    for(float XX:{X-200,X+200})',start)
s=s[:start]+'    if(!Enabled()) {\n'+s[start:end]+'    }\n'+s[end:]
start=s.index('        for(float XX:{X-90,X+14})for(float Z:{Deck+22,Deck+150})')
end=s.index('        if(Style==1)',start)
s=s[:start]+'        if(!Enabled()) {\n'+s[start:end]+'        }\n'+s[end:]
# Upgraded dark/tidy/party reads, with original mode unmodified.
s=s.replace('(I+Floor+Style)%5==1,true);','(Enabled()?(Style==2?Floor==1&&I==2:Style==1?Floor==0&&I==1:Floor==0):(I+Floor+Style)%5==1),true);')
s=s.replace('(I+Floor+Style)%6==1,Style!=1);','(Enabled()?Style==0&&Floor==0:(I+Floor+Style)%6==1),Style!=1);')
public=r'''
bool Enabled(){return !FParse::Param(FCommandLine::Get(),TEXT("NoFraternityRowUpgrade"));}
const TArray<FHouse>& Houses()
{
    static const TArray<FHouse> H={
        {TEXT("R1"),0,-18625,-16175,-2545,-295,30,860},
        {TEXT("R2"),1,-15300,-12500,-2530,-430,30,730},
        {TEXT("R3"),2,-11350,-8850,-2600,-400,30,810},
        {TEXT("R4"),3,-8400,-6800,450,1850,-50,660},
        {TEXT("R5"),4,-6000,-4600,300,1800,-50,740}};
    return H;
}
'''
s=s.replace('void CreateLook(UHouseBlackHallSubsystem* Look)',public+'\nvoid CreateLook(UHouseBlackHallSubsystem* Look)',1)
look=r'''
    if(!Enabled())return;
    auto Finish=[&](FName Key,FLinearColor Color,int Seed,bool Siding)
    {
        FHallImage Img;Img.Init(256,256,Color);
        for(int Y=0;Y<256;++Y)for(int X=0;X<256;++X)
        {
            const float N=(HallNoise(X*.12f,Y*.11f,Seed)-.5f)*.12f+(HallFbm(X*.025f,Y*.025f,3,Seed+1)-.5f)*.15f;
            const float Joint=Siding&&(Y%32<2)?-.18f:0.f;
            Img.At(X,Y)=FLinearColor(FMath::Max(.01f,Color.R+N+Joint),FMath::Max(.01f,Color.G+N*.9f+Joint),FMath::Max(.01f,Color.B+N*.8f+Joint));
        }
        auto* T=HallMakeTexture(Img,true,false,*FString::Printf(TEXT("T_%s"),*Key.ToString()));Look->Textures.Add(Key,T);HallMakeTextured(Look,Key,T,nullptr,.87f);
    };
    Finish(TEXT("EX_RowPlaster"),FLinearColor(.55f,.48f,.34f),8611,false);
    Finish(TEXT("EX_RowBlueSiding"),FLinearColor(.23f,.31f,.34f),8613,true);
    Finish(TEXT("EX_RowCream"),FLinearColor(.67f,.61f,.45f),8615,false);
    HallMakePlain(Look,TEXT("EX_RowTimber"),FLinearColor(.075f,.049f,.027f),.86f);
    HallMakePlain(Look,TEXT("EX_RowCurtain"),FLinearColor(.24f,.12f,.075f),.97f);
    if(auto* Parent=HallHouseMat(TEXT("M_House_Glass")))
    {
        auto* G=UMaterialInstanceDynamic::Create(Parent,Look);G->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.03f,.04f,.047f));
        G->SetScalarParameterValue(TEXT("Opacity"),.24f);G->SetScalarParameterValue(TEXT("Roughness"),.12f);Look->Materials.Add(TEXT("EX_RowPane"),G);
    }
    else HallMakePlain(Look,TEXT("EX_RowPane"),FLinearColor(.025f,.034f,.04f),.13f,.1f);
    for(int I=0;I<5;++I)
    {
        FHallImage N;N.Init(256,128,FLinearColor(.07f,.06f,.04f));FHallTextStyle S;S.Height=70;S.Stroke=6;
        const FString Text=FString::FromInt(177+I*2);HallDrawText(N,Text,{(256-HallTextWidth(Text,S))*.5f,97},S,FLinearColor(.8f,.72f,.52f));
        const FName K(*FString::Printf(TEXT("EX_RowNumber%d"),I));auto* T=HallMakeTexture(N,true,false,*K.ToString());Look->Textures.Add(K,T);HallMakeTextured(Look,K,T,nullptr,.85f);
    }
'''
pos=s.index('    HallMakePlain(Look,TEXT("EX_FratBanner")')
end=s.index('\n}',pos)
s=s[:end]+look+s[end:]
(P/'HouseFraternityRow.cpp').write_text(s)
print('Draft prepared; runtime source untouched')
