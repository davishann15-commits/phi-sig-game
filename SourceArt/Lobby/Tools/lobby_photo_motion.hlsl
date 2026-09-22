// Fixed-camera photo animation. Original subjects move; the house is never displaced.
struct PhotoWind
{
    // Shared with Slate's individual airborne leaves. Start-to-start gaps:
    // 7,13,5,18,9,16,11,6,15,10,17,8 seconds; repeats without a seam.
    float gust(float t)
    {
        if(t<7.0) return 0.0;
        float clock=fmod(t,135.0), start=0.0;
        float starts[12]={0,7,20,25,43,52,68,79,85,100,110,127};
        [unroll] for(int k=0;k<12;++k) if(clock>=starts[k]) start=starts[k];
        float age=clock-start;
        return smoothstep(0.0,.7,age)*(1.0-smoothstep(2.4,4.5,age));
    }
    float box(float2 p, float4 bounds, float feather)
    {
        float2 d = min(p - bounds.xy, bounds.zw - p);
        return smoothstep(0.0, feather, min(d.x, d.y));
    }
    float ellipse(float2 p, float2 center, float2 radius)
    {
        return 1.0 - smoothstep(0.92, 1.02, length((p - center) / radius));
    }
    float foliageRegion(float2 p)
    {
        float a = max(ellipse(p,float2(-55,235),float2(337,430)),
                      ellipse(p,float2(140,-74),float2(306,222)));
        a = max(a, max(ellipse(p,float2(1730,220),float2(300,415)),
                       ellipse(p,float2(1543,-60),float2(282,212))));
        a = max(a, ellipse(p,float2(120,544),float2(288,94)));
        a = max(a, ellipse(p,float2(1327,553),float2(242,111)));
        a = max(a, ellipse(p,float2(176,676),float2(275,86)));
        a = max(a, ellipse(p,float2(1494,680),float2(255,87)));
        a = max(a, ellipse(p,float2(-9,809),float2(171,39)));
        return a;
    }
    float2 foliageShift(float2 p, float t)
    {
        float burst = gust(t);
        float tree = 1.0 - smoothstep(340,530,p.y);
        float root = p.y < 614 ? 615 : (p.y < 765 ? 745 : 840);
        float height = saturate((root-p.y) / (tree > .5 ? 570 : 110));
        float coarse = sin(t*.76) + .38*sin(t*1.31+.4);
        float detail = sin(t*2.18 + p.x*.024 + p.y*.017);
        return float2((coarse*.42+detail*(.12+burst*.7)+burst*(5.5+tree*4.5))*height,
                      sin(t*1.71+p.x*.029)*height*(.12+burst*.85));
    }
    float skyBoundary(float x)
    {
        float h = min(118.0, 9.0 + abs(x-837.0)*.639);
        if (x < 247) h = min(h, max(0.0,(x-95.0)*.79));
        if (x > 1417) h = min(h, max(0.0,(1577.0-x)*.75));
        // Chimney silhouettes are excluded by color below, not rectangular
        // cutouts that would leave stationary strips of the old sky beside them.
        return h;
    }
    float skyAlpha(float2 p, float3 rgb)
    {
        float geometry = 1.0 - smoothstep(skyBoundary(p.x)-2.0, skyBoundary(p.x), p.y);
        return geometry * smoothstep(.015,.047,rgb.b-rgb.r)
                        * smoothstep(.004,.025,rgb.b-rgb.g);
    }
    float poly(float2 p, float2 vertices[12], int count)
    {
        bool inside = false;
        float distance2 = 1e10;
        [unroll] for (int i=0;i<12;++i)
        {
            if (i < count)
            {
                float2 a=vertices[i], b=vertices[i==0 ? count-1 : i-1];
                float2 edge=b-a, v=p-a;
                float2 q=v-edge*saturate(dot(v,edge)/max(dot(edge,edge),.0001));
                distance2=min(distance2,dot(q,q));
                if ((a.y>p.y)!=(b.y>p.y))
                    if (p.x < (b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) inside=!inside;
            }
        }
        float sd=sqrt(distance2)*(inside ? 1.0 : -1.0);
        return smoothstep(-.65,.65,sd);
    }
    float flagAlpha(float2 p,int index)
    {
        float2 v[12];
        int n=0;
        if(index==0)
        {
            v[0]=float2(591,299);v[1]=float2(598,335);v[2]=float2(609,371);
            v[3]=float2(604,368);v[4]=float2(598,390);v[5]=float2(592,402);
            v[6]=float2(572,383);v[7]=float2(578,355);v[8]=float2(585,325);n=9;
        }
        else if(index==1)
        {
            v[0]=float2(708,302);v[1]=float2(712,335);v[2]=float2(718,364);
            v[3]=float2(715,371);v[4]=float2(718,393);v[5]=float2(709,400);
            v[6]=float2(704,414);v[7]=float2(705,393);v[8]=float2(701,389);
            v[9]=float2(704,370);v[10]=float2(701,360);v[11]=float2(705,330);n=12;
        }
        else if(index==2)
        {
            v[0]=float2(965,310);v[1]=float2(971,347);v[2]=float2(975,371);
            v[3]=float2(984,407);v[4]=float2(976,411);v[5]=float2(971,431);
            v[6]=float2(961,418);v[7]=float2(952,403);v[8]=float2(952,392);
            v[9]=float2(955,373);v[10]=float2(959,346);n=11;
        }
        else
        {
            v[0]=float2(1077,300);v[1]=float2(1081,335);v[2]=float2(1086,367);
            v[3]=float2(1090,393);v[4]=float2(1094,418);v[5]=float2(1086,431);
            v[6]=float2(1075,414);v[7]=float2(1072,405);v[8]=float2(1075,393);
            v[9]=float2(1060,374);v[10]=float2(1068,343);n=11;
        }
        return poly(p,v,n);
    }
    float2 flagShift(float2 p,float t,int index,float wind)
    {
        float top = index==2 ? 310 : 299;
        float h = saturate((p.y-top)/118.0);
        float phase=index*1.73;
        // Driven by the exact same wind envelope as the photographed leaves.
        float gust=.35+.75*saturate(wind);
        float x=3.0*sin(t*.83+phase)+5.4*sin(t*2.86-h*7.1+phase)
               +1.5*sin(t*5.21-h*13.3+phase);
        x+=2.0*saturate(wind);
        float y=1.7*sin(t*2.63-h*8.4+phase);
        return float2(x,y)*pow(h,1.65)*gust;
    }
    float leafAlpha(float2 q,float2 halfSize,float3 original,float3 clean)
    {
        float edge=box(q,float4(-halfSize,halfSize),2.0);
        float warmth=smoothstep(.007,.025,original.r-original.g)
                    *smoothstep(.012,.036,original.r-original.b);
        float difference=smoothstep(.011,.043,length(original-clean));
        return edge*warmth*difference;
    }
};
PhotoWind W;
float2 resolution=float2(1672,941);
float2 p=UV*resolution;
float3 original=Texture2DSample(Plate,PlateSampler,UV).rgb;
float3 result=original;
// Continuous, stationary ground beneath the original-photo leaf cutouts.
// This removes their old contact shadows without stamping leaf-shaped patches.
if(p.y>712)
    result=lerp(result,Texture2DSample(GroundPlate,GroundPlateSampler,UV).rgb,smoothstep(712.0,715.0,p.y));
float3 skyColor=original;

// A continuous reconstructed sky removes the occlusion-shaped holes behind the
// roof and chimneys before transport. Two phases avoid a visible loop reset.
float skyMask=W.skyAlpha(p,original);
if(p.y<122)
{
    float phaseA=frac(Seconds/120.0), phaseB=frac(phaseA+.5);
    float blendA=1.0-abs(phaseA*2.0-1.0), blendB=1.0-blendA;
    float2 a=float2(frac(UV.x+(phaseA-.5)*.24),UV.y);
    float2 b=float2(frac(UV.x+(phaseB-.5)*.24),UV.y);
    float3 skyA=Texture2DSample(SkyPlate,SkyPlateSampler,a).rgb;
    float3 skyB=Texture2DSample(SkyPlate,SkyPlateSampler,b).rgb;
    // A subtle dusk adjustment limited to the sky, including foliage reveals.
    skyColor=(skyA*blendA+skyB*blendB)*.85;
    result=lerp(result,skyColor,skyMask);
}

// Alpha-composite the original leaves against the revealed background. Do not
// transport a color difference: that would emboss the backing's brick pattern
// into opaque foliage whenever foreground and background move differently.
float region=W.foliageRegion(p);
if(region>.0001)
{
    float2 displacement=W.foliageShift(p,Seconds)-W.foliageShift(p,0.0);
    float2 q=p-displacement, qUV=clamp(q/resolution,.0001,.9999);
    float3 cleanHere=Texture2DSample(CleanPlate,CleanPlateSampler,UV).rgb;
    float3 moved=Texture2DSample(Plate,PlateSampler,qUV).rgb;
    cleanHere=lerp(cleanHere,skyColor,W.skyAlpha(p,cleanHere));
    float oldAlpha=smoothstep(.1,.9,Texture2DSample(FoliageMatte,FoliageMatteSampler,UV).r);
    float newAlpha=smoothstep(.1,.9,Texture2DSample(FoliageMatte,FoliageMatteSampler,qUV).r);
    result=lerp(result,cleanHere,oldAlpha);
    result=lerp(result,moved,newAlpha);
}

// Original flag fabric, independently warped around fixed pole attachments.
if(p.y>288 && p.y<452 && p.x>551 && p.x<1119)
{
    [unroll] for(int flag=0;flag<4;++flag)
    {
        float center=flag==0?591:(flag==1?709:(flag==2?967:1077));
        if(abs(p.x-center)>39) continue;
        float2 d=W.flagShift(p,Seconds,flag,LeafWind)-W.flagShift(p,0.0,flag,0.0);
        float2 q=p-d;
        float oldAlpha=W.flagAlpha(p,flag), alpha=W.flagAlpha(q,flag);
        if(oldAlpha>.0001)
            result=lerp(result,Texture2DSample(CleanPlate,CleanPlateSampler,UV).rgb,oldAlpha);
        if(alpha>.0001)
        {
            float3 fabric=Texture2DSample(Plate,PlateSampler,q/resolution).rgb;
            float h=saturate((q.y-300.0)/120.0);
            float fold=(sin(Seconds*2.86-h*7.1+flag*1.73)-sin(-h*7.1+flag*1.73))*h*.055;
            result=lerp(result,fabric*(1.0+fold),alpha);
        }
    }
}

// Curtain peek every 30 seconds, one window at a time. Frame and mullion
// pixels remain untouched.
if(Seconds>=30.0)
{
    float eventTime=Seconds-30.0;
    float phase=fmod(eventTime,30.0);
    if(phase<4.8)
    {
        int index=(int)fmod(floor(eventTime/30.0),4.0);
        float4 windows[4]={float4(1158,479,53,76),float4(460,477,52,77),
                           float4(1158,298,44,86),float4(468,298,45,87)};
        float4 win=windows[index];
        float2 local=p-win.xy;
        if(all(local>1.5) && all(local<win.zw-1.5))
        {
            float opening=smoothstep(0.0,.95,phase)*(1.0-smoothstep(3.7,4.8,phase));
            // Measured dividers in the source photograph, not an assumed
            // evenly spaced grid. Source samples cannot cross these rails.
            float3 cutsX=index==0?float3(14,28,42):(index==1?float3(12,24,38):
                        (index==2?float3(23,44,44):float3(21,45,45)));
            float3 cutsY=index==0?float3(19,41.5,60.5):(index==1?float3(20.5,42.5,61.5):
                        (index==2?float3(22,44,66):float3(21.5,43.5,65.5)));
            float left=0,right=win.z,top=0,bottom=win.w;
            [unroll] for(int divider=0;divider<3;++divider)
            {
                if(local.x>cutsX[divider]) left=max(left,cutsX[divider]);
                else right=min(right,cutsX[divider]);
                if(local.y>cutsY[divider]) top=max(top,cutsY[divider]);
                else bottom=min(bottom,cutsY[divider]);
            }
            float pane=smoothstep(2.2,3.2,min(local.x-left,right-local.x))
                       *smoothstep(2.3,3.3,min(local.y-top,bottom-local.y));
            float center=index==0?21.0:(index==1?31.0:(index==2?12.0:32.0));
            float height=smoothstep(0.0,12.0,local.y)*(1.0-smoothstep(win.w-12.0,win.w,local.y));
            float pull=opening*height*3.8*(local.x<center?1.0:-1.0);
            float sampleX=clamp(local.x+pull,left+3.1,right-3.1);
            float2 curtainUV=(p+float2(sampleX-local.x,0))/resolution;
            float3 curtain=Texture2DSample(Plate,PlateSampler,curtainUV).rgb;
            float gapWidth=opening*6.4*height;
            float gap=1.0-smoothstep(max(0.0,gapWidth-.8),gapWidth+.8,abs(local.x-center));
            gap*=opening;
            float lean=(1.0-opening)*7.0-1.2*smoothstep(1.8,2.05,phase);
            float2 face=local-float2(center+lean,30);
            face.x+=face.y*.18*smoothstep(1.8,2.05,phase);
            float head=1.0-smoothstep(.88,1.1,length(face/float2(5.1,8.0)));
            float shoulders=1.0-smoothstep(.85,1.12,length((local-float2(center+lean+1,53))/float2(11,19)));
            float fingers=0;
            [unroll] for(int finger=0;finger<3;++finger)
                fingers=max(fingers,1.0-smoothstep(.7,1.1,length((local-float2(center+gapWidth-1.2,40+finger*3.1))/float2(1.3,.85))));
            float silhouette=max(max(head,shoulders)*gap,fingers*opening);
            // No face, glowing eyes or marching limbs: just an indistinct
            // near-black shape partly hidden by the parted warm curtains.
            float3 room=curtain*.28;
            float3 peek=lerp(curtain,room,gap*.78);
            peek=lerp(peek,float3(.0008,.001,.0012),silhouette*.97);
            result=lerp(result,peek,pane*opening);
        }
    }
}

// Four independently timed lighting circuits, driven by the paused lobby clock.
// Soft masks follow the measured source-photo windows, bulbs and light spill.
if(max(max(LightDim.r,LightDim.g),max(LightDim.b,LightDimA))>.0001)
{
    float left=max(W.box(p,float4(467,299,514,386),3),W.box(p,float4(459,476,513,555),3));
    left=max(left,W.box(p,float4(196,219,220,331),4));
    float right=max(W.box(p,float4(1158,299,1206,386),3),W.box(p,float4(1158,478,1212,555),3));
    right=max(right,max(W.box(p,float4(1454,219,1480,331),4),W.box(p,float4(1461,441,1495,533),4)));
    float porch=max(W.box(p,float4(791,301,876,384),5),W.box(p,float4(787,441,878,475),4));
    porch=max(porch,max(W.box(p,float4(669,298,697,378),4),W.box(p,float4(975,298,1003,378),4)));
    // Central hanging light plus four lamps flanking the front entrance.
    float4 lamps[5]={float4(833,272,22,27),float4(674,491,18,28),
        float4(754,475,18,28),float4(914,475,18,28),float4(989,491,18,28)};
    [unroll] for(int i=0;i<5;++i)
    {
        float distance=length((p-lamps[i].xy)/lamps[i].zw);
        float core=1.0-smoothstep(.35,1.0,distance);
        float spill=(1.0-smoothstep(.8,3.0,distance))*.38;
        porch=max(porch,max(core,spill));
    }
    float garden=0;
    float4 grounds[4]={float4(170,718,24,25),float4(1520,718,24,25),
        float4(367,579,10,24),float4(1125,579,10,24)};
    [unroll] for(int j=0;j<4;++j)
    {
        float distance=length((p-grounds[j].xy)/grounds[j].zw);
        garden=max(garden,max(1.0-smoothstep(.3,1.0,distance),
            (1.0-smoothstep(.8,3.5,distance))*.45));
    }
    // Warm emissive pixels respond strongly; dark brick and white frames retain
    // their ambient illumination. The moving window silhouette stays composited.
    float warm=max(smoothstep(.012,.10,result.r-result.b),smoothstep(.65,.95,min(result.r,min(result.g,result.b))));
    float luminous=smoothstep(.04,.25,result.r);
    float dim=max(max(left*LightDim.r,right*LightDim.g),max(porch*LightDim.b,garden*LightDimA));
    result*=1.0-dim*warm*luminous;
}

// Fallen leaves are now exact original-photo cutout meshes in Slate. The
// background remains unchanged here; no ground UV displacement or fake flakes.
return max(result,0.0);
