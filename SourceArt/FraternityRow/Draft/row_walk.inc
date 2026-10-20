// Insert INSIDE HouseCampusInteriorsReviewImpl, before Routes().
// Review translation unit must include HouseFraternityRow.h at its normal top-level include block.
// Root owns Row-only12cm stop tolerance, floor datum, and idempotent normal door Open/wait handling.
// These are new routes. No pre-existing review waypoint is altered.
void RowRoutes(TArray<FBuildingRoute>& Out)
{
    if(!HouseFraternityRow::Enabled())return;
    // Static literals: FStop stores pointers, so no temporary FString is used for names or screenshot paths.
    static const TCHAR* Shots[5][11]={
        {TEXT("R1_01_Porch.png"),TEXT("R1_02_Hall.png"),TEXT("R1_03_Lounge.png"),TEXT("R1_04_UpperLock.png"),TEXT("R1_05_Chapter.png"),TEXT("R1_06_Dining.png"),TEXT("R1_07_Kitchen.png"),TEXT("R1_08_WC.png"),TEXT("R1_09_RearSteps.png"),TEXT("R1_10_Fence.png"),TEXT("R1_04b_Stairs.png")},
        {TEXT("R2_01_Porch.png"),TEXT("R2_02_Hall.png"),TEXT("R2_03_Lounge.png"),TEXT("R2_04_UpperLock.png"),TEXT("R2_05_Chapter.png"),TEXT("R2_06_Dining.png"),TEXT("R2_07_Kitchen.png"),TEXT("R2_08_WC.png"),TEXT("R2_09_RearSteps.png"),TEXT("R2_10_Fence.png"),TEXT("R2_04b_Stairs.png")},
        {TEXT("R3_01_Porch.png"),TEXT("R3_02_Hall.png"),TEXT("R3_03_Lounge.png"),TEXT("R3_04_UpperLock.png"),TEXT("R3_05_Chapter.png"),TEXT("R3_06_Dining.png"),TEXT("R3_07_Kitchen.png"),TEXT("R3_08_WC.png"),TEXT("R3_09_RearSteps.png"),TEXT("R3_10_Fence.png"),TEXT("R3_04b_Stairs.png")},
        {TEXT("R4_01_Porch.png"),TEXT("R4_02_Hall.png"),TEXT("R4_03_Lounge.png"),TEXT("R4_04_UpperLock.png"),TEXT("R4_05_Chapter.png"),TEXT("R4_06_Dining.png"),TEXT("R4_07_Kitchen.png"),TEXT("R4_08_WC.png"),TEXT("R4_09_RearSteps.png"),TEXT("R4_10_Fence.png"),TEXT("R4_04b_Stairs.png")},
        {TEXT("R5_01_Porch.png"),TEXT("R5_02_Hall.png"),TEXT("R5_03_Lounge.png"),TEXT("R5_04_UpperLock.png"),TEXT("R5_05_Chapter.png"),TEXT("R5_06_Dining.png"),TEXT("R5_07_Kitchen.png"),TEXT("R5_08_WC.png"),TEXT("R5_09_RearSteps.png"),TEXT("R5_10_Fence.png"),TEXT("R5_04b_Stairs.png")}
    };
    // Same clear backyard paths used by HouseFraternityRowYards, ending120cm before the original fence.
    static const TArray<TArray<FVector2D>> Yard={
        {{-17400,85},{-17400,900},{-17100,1450},{-17100,3300},{-16900,4400}},
        {{-13900,-50},{-13900,900},{-13650,2300},{-13650,4400}},
        {{-10100,-20},{-10100,550},{-9700,1900},{-9350,3300},{-9350,4400}},
        {{-7600,2350},{-7600,2700},{-7400,3100},{-7400,3620},{-7500,4400}},
        {{-6300,300},{-6300,700},{-5500,900},{-5500,2850},{-5700,3750},{-5550,4400}}
    };
    const auto& Houses=HouseFraternityRow::Houses();
    if(Houses.Num()!=5)return;
    for(int32 I=0;I<5;++I)
    {
        const auto& H=Houses[I];
        const float W=H.Width(),D=H.Depth(),C=W*.5f,LW=C-120.5f,RW=C+120.5f;
        const float Y1=D*.47f,Y2=D*.76f;
        const float DoorY[5]={Y1-110,Y1*.42f,(Y1+Y2)*.5f,D*.68f,(Y2+D)*.5f};
        const float XL=FMath::Max(50.f,C-410.f),XR=C-135.f;
        const float LowX=XL+55,UpX=XR-55,RunEnd=190+(I<3?11:9)*28;
        FBuildingRoute R;R.Id=H.Id;
        auto At=[&](float X,float Y){return FVector2D(H.L+X,H.F+Y);};
        auto Add=[&](EStep Kind,float X,float Y,int32 Floor,const TCHAR* Name,const TCHAR* Door=nullptr,const TCHAR* Shot=nullptr,float Yaw=0.f)
        {R.Stops.Add({Kind,At(X,Y),Floor,Name,Door,Shot,Yaw});};
        auto Walk=[&](float X,float Y,int32 Floor,const TCHAR* Name,const TCHAR* Shot=nullptr,float Yaw=0.f)
        {Add(EStep::Walk,X,Y,Floor,Name,nullptr,Shot,Yaw);};
        auto Door=[&](float X,float Y,const TCHAR* Id,const TCHAR* Name)
        {Add(EStep::Open,X,Y,0,Name,Id);};
        auto Enter=[&](bool Left,int32 Room,const TCHAR* Id,const TCHAR* Name,const TCHAR* Shot)
        {
            Walk(C,DoorY[Room],0,TEXT("hall outside the room"));
            Door(C,DoorY[Room],Id,TEXT("open the room door normally"));
            Walk((Left?LW-110:RW+110),DoorY[Room],0,Name,Shot,Left?180.f:0.f);
        };
        auto Exit=[&](int32 Room){Walk(C,DoorY[Room],0,TEXT("back through the room door into the hall"));};

        // Start truly on existing grade, walk each exterior riser, and interact from the actual porch.
        if(I<3)
        {
            Add(EStep::Start,C,-1060,0,TEXT("original Row front grade"));
            Walk(C,-960,0,TEXT("approach the original front steps"));
            Walk(C,-750,-1,TEXT("walk the original front risers"));
            Walk(C,-500,0,TEXT("onto the original front porch"));
        }
        else
        {
            const float Front=I==4?H.F-150.f:150.f;
            Add(EStep::Start,C,Front-(I==4?280.f:150.f)-H.F,0,TEXT("existing grade before the new front porch"));
            Walk(C,Front-(I==4?210.f:70.f)-H.F,0,TEXT("at existing grade before the ordinary front risers"));
            Walk(C,Front-(I==4?126.f:14.f)-H.F,-1,TEXT("over the first new front tread"));
        }
        Walk(C,-100,0,TEXT("on the front porch at the main pair"),Shots[I][0],90);
        Door(C,-100,TEXT("MainA"),TEXT("open or verify the front pair"));
        Walk(C,200,0,TEXT("through the main pair into the clear hall"),Shots[I][1],90);

        Enter(true,0,TEXT("Lounge"),TEXT("inside the living room"),Shots[I][2]);
        // Clear the open lounge leaf BEFORE moving south; compact R5's bag stays north of this line.
        Walk(LW-110,DoorY[0]-25,0,TEXT("clear the open living-room leaf"));
        Walk(LowX,DoorY[0]-25,0,TEXT("around the living-room leaf's west tip"));
        Walk(LowX,RunEnd+62,0,TEXT("at the actual lower stair approach"));
        Walk(LowX,RunEnd-56,-1,TEXT("start up the southbound lower flight"));
        Walk(LowX,230,-1,TEXT("continue up the lower flight"));
        Walk(LowX,135,-1,TEXT("on the110cm intermediate return landing"));
        Walk(UpX,135,-1,TEXT("turn across the return landing between visible guards"));
        Walk(UpX,274,-1,TEXT("up the northbound upper flight"));
        Walk(UpX,RunEnd-56,-1,TEXT("near the top of the returning flight"));
        Walk(UpX,RunEnd+55,1,TEXT("on the110cm upper landing"),Shots[I][3],90);
        Add(EStep::Locked,UpX,RunEnd+55,1,TEXT("try the visible private upper door"),TEXT("UpperPrivate"));
        Walk(UpX,RunEnd-56,-1,TEXT("descend the upper flight"));
        Walk(UpX,274,-1,TEXT("continue down toward the intermediate landing"));
        Walk(UpX,135,-1,TEXT("back on the intermediate return landing"));
        Walk(LowX,135,-1,TEXT("turn back onto the lower flight"));
        Walk(LowX,230,-1,TEXT("descend the lower flight"));
        Walk(LowX,RunEnd-56,-1,TEXT("near the foot of the lower flight"));
        Walk(LowX,RunEnd+62,0,TEXT("fully off the stair onto the ground floor"),Shots[I][10],-90);
        if(I==4)
        {
            // East of the sofa, west of the stair, then into the actual curved reading alcove.
            Walk(238.5f,RunEnd+62,0,TEXT("past the sofa's east side"));
            Walk(238.5f,230,0,TEXT("past the south end of the sofa"));
            Walk(200,250,0,TEXT("inside the curved southwest reading alcove"),TEXT("R5_03b_Alcove.png"),-120);
            Walk(238.5f,230,0,TEXT("out of the reading alcove"));
            Walk(238.5f,RunEnd+62,0,TEXT("back along the sofa's clear east side"));
            Walk(LowX,RunEnd+62,0,TEXT("back to the stair approach"));
        }
        Walk(LowX,DoorY[0]-25,0,TEXT("return north of the open lounge leaf"));
        Walk(LW-110,DoorY[0]-25,0,TEXT("round the lounge leaf's tip"));
        Walk(LW-110,DoorY[0],0,TEXT("aligned with the living-room opening"));
        Exit(0);

        Enter(false,1,TEXT("Chapter"),TEXT("inside the chapter room"),Shots[I][4]);Exit(1);
        Enter(true,2,TEXT("Dining"),TEXT("inside the dining room"),Shots[I][5]);
        if(I==4)
        {
            // Compact dining table ends at localX312; the open hall leaf ends nearX432.
            Walk(370,DoorY[2],0,TEXT("around the dining hall leaf's west tip"));
            Walk(370,Y1+54,0,TEXT("the clear aisle south of the dining chairs"));
            Walk(200,Y1+54,0,TEXT("at the real parlor connecting pair"));
            Door(200,Y1+54,TEXT("ParlorA"),TEXT("open the connecting parlor pair normally"));
            Walk(200,Y1-55,0,TEXT("through the connecting pair into the front parlor"),TEXT("R5_06b_ConnectedParlors.png"),-90);
            Walk(200,Y1+54,0,TEXT("back through the parlor pair into dining"));
            Walk(370,Y1+54,0,TEXT("back along the front dining aisle"));
            Walk(370,DoorY[2],0,TEXT("round the dining hall leaf"));
            Walk(LW-110,DoorY[2],0,TEXT("aligned with the dining-room exit"));
        }
        Exit(2);
        Enter(false,3,TEXT("Kitchen"),TEXT("inside the practical kitchen"),Shots[I][6]);Exit(3);
        Enter(true,4,TEXT("SharedWC"),TEXT("inside the shared bathroom"),Shots[I][7]);
        // Bypass the open140cm bathroom leaf before moving south into the stall aisle.
        const float Cell=I==4?125.f:155.f,StallL=I==4?73.f:93.f;
        const float StallR=StallL+2*Cell+7.5f,Front=D-223;
        const float LeafTip=LW-8.5f-139.2f;
        const float Bypass=(StallR+LeafTip)*.5f;
        const float Aisle=(Y2+7.5f+Front-7.5f)*.5f;
        Walk(Bypass,DoorY[4],0,TEXT("around the bathroom leaf's west tip"));
        Walk(Bypass,Aisle,0,TEXT("into the bathroom's front aisle"));
        for(int32 N=0;N<2;++N)
        {
            const float X=StallL+(N+.5f)*Cell;
            Walk(X,Aisle,0,TEXT("at the cubicle door from the clear front aisle"));
            Door(X,Aisle,N==0?TEXT("WCStall1"):TEXT("WCStall2"),TEXT("open the cubicle door normally"));
            Walk(X,Front+55,0,N==0?TEXT("inside the first WC cubicle"):TEXT("inside the second WC cubicle"));
            Walk(X,Aisle,0,TEXT("out of the cubicle to the front aisle"));
        }
        Walk(Bypass,Aisle,0,TEXT("back around the bathroom door leaf"));
        Walk(Bypass,DoorY[4],0,TEXT("north of the open bathroom leaf"));
        Walk(LW-110,DoorY[4],0,TEXT("aligned with the bathroom exit"));Exit(4);

        Walk(C,D-170,0,TEXT("at the real rear pair"));
        Door(C,D-170,TEXT("RearA"),TEXT("open or verify the rear pair normally"));
        Walk(C,D+65,0,TEXT("through the rear pair onto its top landing"),Shots[I][8],-90);
        Walk(C,D+180,-1,TEXT("walk down the ordinary rear risers"));
        // World coordinates: these continue the same physical walk through the protected clear yard path.
        for(int32 N=0;N<Yard[I].Num();++N)
        {
            const bool Last=N==Yard[I].Num()-1;
            R.Stops.Add({EStep::Walk,Yard[I][N],0,N==0?TEXT("fully off the rear steps onto existing grade"):Last?TEXT("at the original rear fence approach"):TEXT("follow the clear existing yard trail"),nullptr,Last?Shots[I][9]:nullptr,90});
        }
        R.Stops.Add({EStep::Leave,FVector2D::ZeroVector,0,TEXT("leave the Row; verify player-only streaming unloads it"),nullptr,nullptr});
        Out.Add(MoveTemp(R));
    }
}
