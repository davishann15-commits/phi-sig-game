"""Prepare the owner's darker/worn timber request outside the frozen runtime tree."""
from pathlib import Path
import hashlib,json
PREP=Path(__file__).parent;ROOT=PREP.resolve().parents[2];SRC=ROOT/'Source/SeniorSendoff'
interior=(SRC/'HouseStadiumDetailInteriors.cpp').read_text();look=(SRC/'HouseStadiumDetailLook.cpp').read_text()
interior_edits=[];look_edits=[]
def edit(items,source,old,new):
 assert source.count(old)==1,old
 items.append({'old':old,'new':new})
old='const FVector Axis[]={FVector::ForwardVector,FVector::RightVector,FVector::UpVector};const auto UV=FHallUV::World(100);'
new='''const FVector Axis[]={FVector::ForwardVector,FVector::RightVector,FVector::UpVector};FHallUV UV=FHallUV::World(100);
 const bool AgedWood=HouseStadiumDetail::Step()>=7&&(Key.ToString().StartsWith(TEXT("SD_LockerOak"))||Key==TEXT("SD_BenchWood"));
 const FName BevelKey=AgedWood?FName(TEXT("SD_WoodEdge")):Key;
 if(AgedWood){UV=FHallUV::Local(FTransform(Rotation,Centre),100);UV.Offset={FMath::Fmod(FMath::Abs(float(Centre.X))*.013f,1.f),FMath::Fmod(FMath::Abs(float(Centre.Y))*.017f,1.f)};}'''
edit(interior_edits,interior,old,new)
old='M.Quad(Key,P(U-End),P(V-End),P(V+End),P(U+End),N((Axis[A]*SA+Axis[B]*SB).GetSafeNormal()),UV);'
edit(interior_edits,interior,old,old.replace('M.Quad(Key,','M.Quad(BevelKey,'))
old='HouseCampusFallKit::Tri(M,Key,P({X*H[0],Y*(H[1]-Bevel),Z*(H[2]-Bevel)}),'
edit(interior_edits,interior,old,old.replace('Tri(M,Key,','Tri(M,BevelKey,'))
old='I::SoftBox(Solid,J==1?TEXT("SD_LockerOakLight"):TEXT("SD_LockerOak"),C+Facing*((J-1)*11.8f)'
edit(interior_edits,interior,old,'I::SoftBox(Solid,Step()>=7?TEXT("SD_BenchWood"):J==1?TEXT("SD_LockerOakLight"):TEXT("SD_LockerOak"),C+Facing*((J-1)*11.8f)')
old='  // Open shelf back has real ventilation slots, a hanger rail and a double coat hook.'
new='''  if(Step()>=7){FRandomStream Wear(1931+L*71+int32(FMath::Abs(Origin.X)));for(float Side:{-1.f,1.f}){const FVector P=C+Along*(Side*17.7f)+Facing*24.06f+FVector(0,0,Wear.FRandRange(65,128));const float Width=Wear.FRandRange(.20f,.45f),Height=Wear.FRandRange(3,9);Detail.Quad(TEXT("SD_WoodEdge"),P-Along*Width,P+Along*Width+FVector(0,0,1.3f),P+Along*Width*.25f+FVector(0,0,Height),P-Along*Width*.35f+FVector(0,0,Height*.85f),Facing,FHallUV::World(100));}}
'''+old
edit(interior_edits,interior,old,new)
old='I.At(X,Y)=FLinearColor(.27f,.152f,.067f)*Shade*Tint;'
new='''const float Age=Step()>=7?.80f+HallNoise(X*.013f,Y*.018f,1939+Variant)*.28f:1.f;I.At(X,Y)=(Step()>=7?FLinearColor(.17f,.086f,.035f):FLinearColor(.27f,.152f,.067f))*Shade*Tint*Age;'''
edit(look_edits,look,old,new)
old='L::Texture(Look,Variant==0?TEXT("SD_LockerOakLight"):Variant==1?TEXT("SD_LockerOak"):TEXT("SD_LockerOakDark"),I,.62f,0,&N);'
edit(look_edits,look,old,old.replace('I,.62f,0,&N','I,Step()>=7?.74f:.62f,0,&N'))
old='  FHallImage Steel;Steel.Init(512,256,{.46f,.47f,.45f});'
new='''  if(Step()>=7){
   HallMakePlain(Look,TEXT("SD_WoodEdge"),{.245f,.14f,.063f},.88f);HallMakePlain(Look,TEXT("SD_WoodStain"),{.067f,.038f,.018f},.95f);
   FHallImage Bench,BenchNormal;Bench.Init(512,256,{.18f,.094f,.038f});TArray<float> Heights;Heights.SetNum(512*256);
   ParallelFor(256,[&](int32 Y){for(int32 X=0;X<512;++X){const float Warp=FMath::Sin(X*.012f)*5,Grain=HallNoise(X*.018f,(Y+Warp)*.5f,1986),Age=HallNoise(X*.012f,Y*.025f,1931),Scuff=FMath::Pow(HallNoise(X*.014f,Y*.75f,31),10.f)*.28f;const FLinearColor Base=FLinearColor(.18f,.094f,.038f)*(.72f+Grain*.27f)*(.82f+Age*.22f);Bench.At(X,Y)=FMath::Lerp(Base,FLinearColor(.28f,.164f,.078f),Scuff);Bench.At(X,Y).A=1;Heights[Y*512+X]=Grain*.08f+Age*.05f;}});
   HallHeightToNormal(Heights,512,256,.4f,true,BenchNormal);L::Texture(Look,TEXT("SD_BenchWood"),Bench,.66f,0,&BenchNormal);
  }
'''+old
edit(look_edits,look,old,new)
result={'prepared_only':True,'owner_request':'Darker, more worn wood (direct answer19October2026).','interior_preimage_sha':hashlib.sha256((SRC/'HouseStadiumDetailInteriors.cpp').read_bytes()).hexdigest(),'interior_edits':interior_edits,'look_edits':look_edits,'scope':'Final stage7 material/edge finish: darker oak, uneven aged tone, rubbed bench planks, local grain orientation, exposed bevel/end grain and small vertical stile nicks. All collision dimensions, furniture positions, prior camera lines, lights and stage6/disabled branches stay unchanged. No new Content assets. Actual final visual/native/performance gates mandatory.'}
(PREP/'wood_age_finish.json').write_text(json.dumps(result,indent=2)+'\n')
p=PREP/'integrate_after_advance.py';s=p.read_text();marker='    # All preconditions above are read-only. Every runtime write below has a fresh mtime.\n';assert s.count(marker)==1
s=s.replace(marker,'''    # Direct owner follow-up: darker and more worn timber, applied in the final finish stage.
    wood = json.loads((PREP / 'wood_age_finish.json').read_text())
    interior = src / 'HouseStadiumDetailInteriors.cpp'
    assert sha(interior) == wood['interior_preimage_sha']
    interior_after = interior.read_text()
    for item in wood['interior_edits']:interior_after = replace_once(interior_after,item['old'],item['new'])
    for item in wood['look_edits']:look_after = replace_once(look_after,item['old'],item['new'])

'''+marker)
marker='    write(legacy, legacy_after)\n';assert s.count(marker)==1;s=s.replace(marker,marker+'    write(interior, interior_after)\n    write(draft / interior.name, interior_after)\n');p.unlink();p.write_text(s)
print('Prepared only: darker, aged oak and bench/contact wear. Runtime remains frozen for current timing/reviews.')
