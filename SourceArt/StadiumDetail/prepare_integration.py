"""Prepare exact narrow hook candidates; default writes Draft/Patched, never runtime source.
--apply is only for an explicitly released integration window, after baseline receipt.
"""
from pathlib import Path
import json,hashlib,difflib,sys,math
R=Path(__file__).resolve().parents[2];D=R/'SourceArt/StadiumDetail/Draft';S=R/'Source/SeniorSendoff';W=R/'Saved/StadiumDetailWork'
apply='--apply' in sys.argv
if apply:
 grant=json.loads((W/'integration_grant.json').read_text())
 assert grant['granted'] and grant['scope']=='full' and grant.get('release_evidence')
 assert (W/'before_receipt.json').exists()
 snapshot=json.loads((W/'pre_integration_snapshot.json').read_text())
 for rel,sha in snapshot['source_config_project_map_sha'].items():assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,rel+' changed since the reserved integration snapshot'
dst=S if apply else D/'Patched';dst.mkdir(exist_ok=True)
patches={};original={}
def source(name,include=True):
 text=(S/name).read_text();original[name]=text
 assert '#include "HouseStadiumDetail.h"' not in text, name+' already integrated'
 if not include:return text
 # Preserve Unreal's own-header-first rule.
 index=text.index('\n',text.index('#include '))
 return text[:index+1]+'#include "HouseStadiumDetail.h"\n'+text[index+1:]
def replace(s,a,b):
 assert s.count(a)==1,(a[:130],s.count(a));return s.replace(a,b,1)
n='HouseExteriorCampus.cpp';s=source(n)
s=replace(s,'if(HouseCampusInteriors::StadiumVomitory(K,Row))continue;const float R0=', 'if(HouseCampusInteriors::StadiumVomitory(K,Row))continue;if(HouseStadiumDetail::Enabled()){HouseStadiumDetail::Tier(Near,K,Row,Stone);continue;}const float R0=')
s=replace(s,'  Near.Quad(Stone,P(A,.98f,1050),P(B,.98f,1050),P(B,1.045f,1050),P(A,1.045f,1050),FVector::UpVector,Plain);','  if(HouseStadiumDetail::Aisle(K))HouseStadiumDetail::Rim(Near,K,Stone);else Near.Quad(Stone,P(A,.98f,1050),P(B,.98f,1050),P(B,1.045f,1050),P(A,1.045f,1050),FVector::UpVector,Plain);')
s=replace(s,'   for(int Row=0;Row<12;++Row){if(HouseCampusInteriors::StadiumVomitory(K,Row))continue;const float Rb=', '   if(!HouseStadiumDetail::Enabled())for(int Row=0;Row<12;++Row){if(HouseCampusInteriors::StadiumVomitory(K,Row))continue;const float Rb=')
s=replace(s,'   if(K%5==0){for(int Row=', '   if(!HouseStadiumDetail::Enabled()&&K%5==0){for(int Row=')
# Replace the old180cm-high inner rim rail with the new105cm outer guard.
s=replace(s,'if(K%2==0&&!bUnderPress)Near.Cylinder','if(!HouseStadiumDetail::Enabled()&&K%2==0&&!bUnderPress)Near.Cylinder')
s=replace(s,'if(!bUnderPress)HouseSurroundings::Beam','if(!HouseStadiumDetail::Enabled()&&!bUnderPress)HouseSurroundings::Beam')
s=replace(s,'if(K%2==0&&!HouseCampusInteriors::StadiumGate(K))','if(K%2==0&&!HouseCampusInteriors::StadiumGate(K)&&!HouseStadiumDetail::Portal(K))')
score_shell=' Box(Near,Dark,StadiumX-850,StadiumX+850,StadiumY-3800,StadiumY-3680,Ground+1250,Ground+2250);\n Box(Near,TEXT("EX_CampusScore"),StadiumX-770,StadiumX+770,StadiumY-3678,StadiumY-3674,Ground+1380,Ground+2150);'
s=replace(s,score_shell,' if(!(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=2))\n {\n'+score_shell+'\n }')
cap='  Panel(Near,Stone,A0+N*3,B0+N*3,1040,1110,N);'
s=replace(s,cap,'  if(HouseStadiumDetail::Aisle(K)){const FVector CapL=A0+N*3,CapR=B0+N*3;const float CapGap=135.f/FVector::Dist(CapL,CapR);Panel(Near,Stone,CapL,FMath::Lerp(CapL,CapR,.5f-CapGap),1040,1110,N);Panel(Near,Stone,FMath::Lerp(CapL,CapR,.5f+CapGap),CapR,1040,1110,N);}else '+cap.strip())
patches[n]=s
n='HouseCampusFallLook.cpp';s=source(n);i=s.rfind('\n}');s=s[:i]+'\n    HouseStadiumDetail::CreateLook(Look);'+s[i:]
s=replace(s,'HouseCampusFallLookImpl::PaintField(Img); Tex(', 'HouseCampusFallLookImpl::PaintField(Img);\n    HouseStadiumDetail::WeatherField(Img);\n    Tex(')
patches[n]=s
n='HouseCampusFall.cpp';s=source(n)
s=replace(s,'    constexpr int32 Loops = 3;','    const bool bStadiumGhost=HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7;\n    const int32 Loops=bStadiumGhost?1:3;')
s=replace(s,"                if (Ba >= TEXT('1') && Ba <= TEXT('5')) Bass", "                if (!bStadiumGhost&&Ba >= TEXT('1') && Ba <= TEXT('5')) Bass")
s=replace(s,"                if (Te >= TEXT('a') && Te <= TEXT('d'))", "                if (!bStadiumGhost&&Te >= TEXT('a') && Te <= TEXT('d'))")
s=replace(s,'                if (((Bar == 0 || Bar == 2) && S == 0) || (Bar == 3 && S == 12)) Cymbal', '                if (!bStadiumGhost&&(((Bar == 0 || Bar == 2) && S == 0) || (Bar == 3 && S == 12))) Cymbal')
s=replace(s,'    const float A = 1.f - FMath::Exp(-2.f * PI * 4200.f / DrumRate);','    const float A = 1.f - FMath::Exp(-2.f * PI * (bStadiumGhost?1400.f:4200.f) / DrumRate);')
s=replace(s,'    BuildSound();\n    UE_LOG(', '    BuildSound();\n    AHouseStadiumDetail::SpawnFor(this,Look);\n    UE_LOG(')
s=replace(s,'    float Every = 110.f;', '    float Every = HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7?260.f:110.f;')
s=replace(s,'    NextDrums = GetWorld()->GetAudioTimeSeconds() + FMath::Min(30.0, DrumsEvery * .5);','    NextDrums = GetWorld()->GetAudioTimeSeconds() + (HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7?130.0:FMath::Min(30.0, DrumsEvery * .5));')
s=s.replace('Drums->SetVolumeMultiplier(bInside ? .4f : 1.f);','Drums->SetVolumeMultiplier((bInside ? .4f : 1.f)*(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7?.10f:1.f));')
s=s.replace('Drums->SetVolumeMultiplier(bDrumsIndoors ? .4f : 1.f);','Drums->SetVolumeMultiplier((bDrumsIndoors ? .4f : 1.f)*(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7?.10f:1.f));')
patches[n]=s
n='HouseCampusFallStadium.cpp';s=source(n)
s=replace(s,'void Scoreboard(HouseCampusFallParts::FParts& P)\n{','void Scoreboard(HouseCampusFallParts::FParts& P)\n{\n    if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=2)return;')
s=replace(s,'        if (FMath::Abs(A - Aisle) < .035f) continue;','        if (FMath::Abs(A - Aisle) < .035f) continue;\n        const int32 DetailSegment=FMath::Clamp(FMath::FloorToInt(A/(2.f*PI/BowlSeg)),0,BowlSeg-1);\n        if(HouseStadiumDetail::Aisle(DetailSegment)||HouseStadiumDetail::PortalBand(DetailSegment,Row)||HouseStadiumDetail::CrossAisle(DetailSegment,Row))continue;')
s=replace(s,'Tread + (bOnBench ? 12.f : 0.f) + .3f','Tread + (bOnBench ? (HouseStadiumDetail::Enabled()?45.f:12.f) : 0.f) + .3f')
s=replace(s,'        else if (bOnBench) M.Box(TEXT("CF_Purple"),','        else if (bOnBench&&!HouseStadiumDetail::Enabled()) M.Box(TEXT("CF_Purple"),')
a=s.index('    // The press box: lit windows');b=s.index('    // Haze round the three flood heads',a);s=s[:a]+'    if(!(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=2))\n    {\n'+s[a:b]+'    }\n'+s[b:]
# Dead fixture gets no individual halo; all bank centres remain fixed.
s=replace(s,'        for (const HouseCampusPolish::FFloodBank& B : Banks)','        int32 DetailBank=0;\n        for (const HouseCampusPolish::FFloodBank& B : Banks)')
s=replace(s,'            for (const FVector& L : B.Lenses) P.Halos.Add({ L + B.Facing * 6.f, 260.f, FLinearColor(.85f, .9f, 1.f, .5f), 0.f });','            int32 DetailLens=0;\n            for (const FVector& L : B.Lenses){if(!(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=3&&DetailBank==0&&DetailLens==0))P.Halos.Add({ L + B.Facing * 6.f, 260.f, FLinearColor(.85f, .9f, 1.f, .5f), 0.f });++DetailLens;}\n            ++DetailBank;')
# Two flags use the existing cloth simulation and share its distance throttle.
marker='    HouseCampusFallStadiumImpl::Field(P);';assert marker in s
s=s.replace(marker,marker+'\n    HouseStadiumDetail::FieldCollision(P.Field,P.Solid);\n    if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=2)for(float FlagX:{700.f,3700.f}){HouseCampusFallParts::FCloth Flag;Flag.Front=Flag.Back=TEXT("SD_Fabric");Flag.Origin={FlagX+5,-9730,2925};Flag.Right={1,0,0};Flag.Down={0,0,-1};Flag.Width=130;Flag.Height=85;Flag.Cols=6;Flag.Rows=4;Flag.Amp=.9f;Flag.Pinned=1.f;P.Cloths.Add(Flag);}',1)
patches[n]=s
n='HouseCampusInteriorsStadium.cpp';s=source(n)
s=replace(s,'    const FName Menu = Kind == 0 ? FName(TEXT("CI_StaMenuGrill")) : Kind == 1 ? FName(TEXT("CI_StaMenuPopcorn")) : FName(TEXT("CI_StaMenuDrinks"));','    const FName Menu = Kind == 0 ? (HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=5?FName(TEXT("SD_Menu")):FName(TEXT("CI_StaMenuGrill"))) : Kind == 1 ? FName(TEXT("CI_StaMenuPopcorn")) : FName(TEXT("CI_StaMenuDrinks"));')
# The old650cm/25-tread flight has26cm treads. Use70cm of its existing220cm
# top landing for a720cm/24-tread flight; the same U383 tour stop stays on
# the remaining150cm landing. No mouth, doorway, height or waypoint moves.
s=replace(s,'    const int32 N = FMath::CeilToInt((ZTop - ZBottom) / 17.5f);','    const int32 N = FMath::CeilToInt((ZTop - ZBottom) / (HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?18.f:17.5f));')
s=replace(s,'    StaFlight(Solid, Clutter, V, StaStairBottom, StaMouth1, StaStairD0, StaStairD1, StaFloor, StaVomZ);','    const float DetailLandingEnd=HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?StaMouth1-70.f:StaMouth1;\n    StaFlight(Solid, Clutter, V, StaStairBottom, DetailLandingEnd, StaStairD0, StaStairD1, StaFloor, StaVomZ);')
s=replace(s,'    V.Slab(Solid, TEXT("CI_StaConcrete"), StaMouth0 - 20.f, StaMouth1, StaStairD0, StaStairD1, StaFloor - 5.f, StaVomZ);','    V.Slab(Solid, TEXT("CI_StaConcrete"), StaMouth0 - 20.f, DetailLandingEnd, StaStairD0, StaStairD1, StaFloor - 5.f, StaVomZ);')
s=replace(s,'    V.Slab(Solid, TEXT("CI_StaConcrete"), StaMouth0 - 20.f, StaMouth1, StaStairD1, StaStairD1 + 12.f, StaFloor - 5.f, StaVomZ + 95.f);','    V.Slab(Solid, TEXT("CI_StaConcrete"), StaMouth0 - 20.f, DetailLandingEnd, StaStairD1, StaStairD1 + 12.f, StaFloor - 5.f, StaVomZ + 95.f);')
s=replace(s,'    const float IX0 = X0 + 20.f, IX1 = X1 - 20.f, SY0 = Y0 + 20.f + 140.f, SY1 = Y1 - 20.f - 140.f, SpineX = (IX0 + IX1) * .5f;','    const float DetailTowerLanding=HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?130.f:140.f;\n    const float IX0 = X0 + 20.f, IX1 = X1 - 20.f, SY0 = Y0 + 20.f + DetailTowerLanding, SY1 = Y1 - 20.f - DetailTowerLanding, SpineX = (IX0 + IX1) * .5f;')
s=replace(s,'            Lockers(Solid, Clutter, O.At(U0, 0.f, StaFloor), O.Along, Count, -O.N);','            if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6)HouseStadiumDetail::WoodLockers(Solid,Clutter,O.At(U0,0.f,StaFloor),O.Along,Count,-O.N);else Lockers(Solid, Clutter, O.At(U0, 0.f, StaFloor), O.Along, Count, -O.N);')
s=replace(s,'-54.f, -51.f, StaFloor + 70.f, StaFloor + 150.f);   // a jersey on the door\'s hook','-(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?15.f:54.f), -(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?12.f:51.f), StaFloor + 70.f, StaFloor + 150.f);   // detail: inside the open wood bay; legacy: on the door hook')
s=replace(s,'    StaPressBox(Solid, Clutter, Glow, Kit);','    StaPressBox(Solid, Clutter, Glow, Kit);\n    HouseStadiumDetail::Interiors(Solid,Clutter,Glow);')
patches[n]=s
n='HouseCampusPolishStadium.cpp';s=source(n)
s=replace(s,'bool HasArch(int32 K) { return K % 2 == 0 && !IsGate(K); }','bool HasArch(int32 K) { return K % 2 == 0 && !IsGate(K) && !HouseStadiumDetail::Portal(K); }')
s=replace(s,'if (SI::IsGate(K)) { const SI::FArchFrame A(K);','if (SI::IsGate(K)||HouseStadiumDetail::Portal(K)) { const SI::FArchFrame A(K);')
s=replace(s,'SI::PaintFrieze(Img, 1024, TEXT("DEDICATED OCTOBER 22 1927"), 64);','SI::PaintFrieze(Img, 1024, HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=5?TEXT("DEDICATED OCTOBER 17 1931"):TEXT("DEDICATED OCTOBER 22 1927"), 64);')
s=replace(s,'    const TArray<FBox2D> Holes = { FBox2D(FVector2D(Len * HouseCampusInteriors::StadiumWinT0, HouseCampusInteriors::StadiumWinZ0), FVector2D(Len * HouseCampusInteriors::StadiumWinT1, HouseCampusInteriors::StadiumWinZ1)) };','    TArray<FBox2D> Holes = { FBox2D(FVector2D(Len * HouseCampusInteriors::StadiumWinT0, HouseCampusInteriors::StadiumWinZ0), FVector2D(Len * HouseCampusInteriors::StadiumWinT1, HouseCampusInteriors::StadiumWinZ1)) };\n    if(HouseStadiumDetail::Aisle(K))Holes.Add(FBox2D(FVector2D(Len*.5f-135.f,900.f),FVector2D(Len*.5f+135.f,1110.f)));\n    if(HouseStadiumDetail::Portal(K))Holes.Add(FBox2D(FVector2D(Len*.5f-175.f,-1.f),FVector2D(Len*.5f+175.f,420.f)));')
s=replace(s,'        if (bGate) Openings.Add(FBox2D(FVector2D(Mid - 125.f, -1.f), FVector2D(Mid + 125.f, 300.f)));','        if (bGate) Openings.Add(FBox2D(FVector2D(Mid - 125.f, -1.f), FVector2D(Mid + 125.f, 300.f)));\n        if(HouseStadiumDetail::Portal(K))Openings.Add(FBox2D(FVector2D(Mid-175.f,-1.f),FVector2D(Mid+175.f,420.f)));')
s=replace(s,'        C.Slab(D, TEXT("CP_Ashlar"), 0.f, C.Len, 0.f, 6.f, 92.f, 100.f, Stone);','        if(HouseStadiumDetail::Portal(K)){C.Slab(D,TEXT("CP_Ashlar"),0.f,Mid-175.f,0.f,6.f,92.f,100.f,Stone);C.Slab(D,TEXT("CP_Ashlar"),Mid+175.f,C.Len,0.f,6.f,92.f,100.f,Stone);}else C.Slab(D, TEXT("CP_Ashlar"), 0.f, C.Len, 0.f, 6.f, 92.f, 100.f, Stone);')
a=s.index('        C.Slab(D, TEXT("CP_Ashlar"), 0.f, C.Len, 0.f, 3.f, 930.f, 1040.f, Stone);');b=s.index('        // Pilasters',a);s=s[:a]+'        if(!HouseStadiumDetail::Aisle(K))\n        {\n'+s[a:b]+'        }\n'+s[b:]
s=replace(s,'        if (Frieze) C.Card(D, Frieze, Mid, 3.2f, 985.f, C.Len - 40.f, 106.f);','        if (Frieze&&!HouseStadiumDetail::Aisle(K)) C.Card(D, Frieze, Mid, 3.2f, 985.f, C.Len - 40.f, 106.f);')
s=replace(s,'    for (const FVector& L : B.Lenses)\n    {','    int32 DetailLens=0;\n    for (const FVector& L : B.Lenses)\n    {')
s=replace(s,'        P.Glow.Cylinder(TEXT("CP_FloodLens"), L - F * 1.2f, L, 23.f, 14, Plain());','        FName LensKey(TEXT("CP_FloodLens"));if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=3){if(Deg==35.f&&DetailLens==0)LensKey=TEXT("SD_DeadBulb");if(Deg==145.f&&DetailLens==1)LensKey=TEXT("SD_FloodFlicker");}\n        P.Glow.Cylinder(LensKey, L - F * 1.2f, L, 23.f, 14, Plain());++DetailLens;')
patches[n]=s
allpatch=''
# Append two additional close inspections after every current Row/stadium/standard pose.
# Only these camera lines change in HouseExterior.cpp; no house geometry is edited.
n='HouseExterior.cpp';s=source(n,False);start=s.index('const FExView ExViews[] = {');end=s.index('\n};',start)
extra='\n    // New real stadium passages: eye170cm above the ninth17.04cm tread.\n'
for k,label in ((4,'east'),(16,'west')):
 name='detail_stadium_portal_'+label
 assert name not in s
 def mid(radius,z):
  a,b=k*math.pi/20,(k+1)*math.pi/20
  return (2400+(math.cos(a)+math.cos(b))*.5*5600*radius,-13300+(math.sin(a)+math.sin(b))*.5*3800*radius,z)
 eye=mid(1.0-.198*8.5/25,-57+426*9/25+170);target=mid(.78,464)
 extra+='    { TEXT("%s"), FVector(%.2f,%.2f,%.2f), FVector(%.2f,%.2f,%.2f), 75.f },\n'%(name,*eye,*target)
s=s[:end]+extra+s[end:];patches[n]=s
new_sources=[p for p in D.glob('HouseStadiumDetail*') if p.suffix in ['.h','.cpp']]
if apply:
 for p in new_sources:assert not (dst/p.name).exists(),'Unexpected existing source '+str(dst/p.name)
for name,s in patches.items():
 allpatch+=''.join(difflib.unified_diff(original[name].splitlines(True),s.splitlines(True),fromfile='a/Source/SeniorSendoff/'+name,tofile='b/Source/SeniorSendoff/'+name))
 target=dst/name
 if apply:target.unlink() # owner handoff: remove a whole-file rewrite first to avoid iCloud "name2" copies
 target.write_text(s)
for p in new_sources:
 (dst/p.name).write_bytes(p.read_bytes())
(W/('integrated_hooks.patch' if apply else 'draft_hooks.patch')).write_text(allpatch)
print(('Applied' if apply else 'Prepared dry-run'),len(patches),'narrow legacy hooks; preserved current unrelated source.')
