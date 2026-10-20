#include "HouseStadiumDetail.h"
#include "HouseBlackHall.h"
#include "HouseCampusFallKit.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/Package.h"
namespace HouseStadiumDetailKitImpl
{
void Describe(const FHallMesh& Input,FMeshDescription& Description)
{
 FStaticMeshAttributes Attr(Description);Attr.Register();auto Positions=Attr.GetVertexPositions();auto Normals=Attr.GetVertexInstanceNormals();auto Tangents=Attr.GetVertexInstanceTangents();auto Signs=Attr.GetVertexInstanceBinormalSigns();auto UV=Attr.GetVertexInstanceUVs();UV.SetNumChannels(1);auto Slots=Attr.GetPolygonGroupMaterialSlotNames();
 for(FName Key:Input.Order){const auto& S=Input.Sections[Key];const auto G=Description.CreatePolygonGroup();Slots[G]=Key;TArray<FVertexInstanceID> IDs;
  for(int32 I=0;I<S.V.Num();++I){const auto V=Description.CreateVertex();Positions[V]=FVector3f(S.V[I]);const auto VI=Description.CreateVertexInstance(V);IDs.Add(VI);Normals[VI]=FVector3f(S.N[I]);Tangents[VI]=FVector3f(S.T[I].TangentX);Signs[VI]=1.f;UV.Set(VI,0,FVector2f(S.UV[I]));}
  for(int32 I=0;I<S.I.Num();I+=3)Description.CreatePolygon(G,TArray<FVertexInstanceID,TInlineAllocator<3>>{IDs[S.I[I]],IDs[S.I[I+1]],IDs[S.I[I+2]]});
 }
}
}
UStaticMesh* HouseStadiumDetail::PlankKit(UHouseBlackHallSubsystem* Look)
{
 // One physical-size kit avoids stretching a metre-cube bevel across a4cm seat edge.
 auto* M=NewObject<UStaticMesh>(GetTransientPackage(),TEXT("CalderStadiumBevelledPlankKit"),RF_Transient);M->NeverStream=true;const FName Key(TEXT("SD_Aluminium"));M->GetStaticMaterials().Add(FStaticMaterial(Look->Mat(Key),Key
#if WITH_EDITORONLY_DATA
 ,Key
#endif
 ));FMeshDescription Descriptions[2];TArray<const FMeshDescription*> Pointers;
 for(int32 L=0;L<2;++L)
 {
  FHallMesh G;const FVector Half(87.5f,8,2);const auto UV=FHallUV::World(100);
  if(L)G.Box(Key,FTransform::Identity,Half,UV);
  else
  {
   constexpr float Bevel=.4f;const float H[]={87.5f,8.f,2.f};const FVector Axis[]={FVector::ForwardVector,FVector::RightVector,FVector::UpVector};
   for(int32 A=0;A<3;++A)for(float S:{-1.f,1.f}){const int32 B=(A+1)%3,C=(A+2)%3;const FVector Centre=Axis[A]*(S*H[A]),P=Axis[B]*(H[B]-Bevel),Q=Axis[C]*(H[C]-Bevel);G.Quad(Key,Centre-P-Q,Centre+P-Q,Centre+P+Q,Centre-P+Q,Axis[A]*S,UV);}
   for(int32 A=0;A<2;++A)for(int32 B=A+1;B<3;++B)for(float SA:{-1.f,1.f})for(float SB:{-1.f,1.f}){const int32 C=3-A-B;const FVector P=Axis[A]*(SA*H[A])+Axis[B]*(SB*(H[B]-Bevel)),Q=Axis[A]*(SA*(H[A]-Bevel))+Axis[B]*(SB*H[B]),End=Axis[C]*(H[C]-Bevel);G.Quad(Key,P-End,Q-End,Q+End,P+End,(Axis[A]*SA+Axis[B]*SB).GetSafeNormal(),UV);}
   for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})for(float Z:{-1.f,1.f})HouseCampusFallKit::Tri(G,Key,{X*H[0],Y*(H[1]-Bevel),Z*(H[2]-Bevel)},{X*(H[0]-Bevel),Y*H[1],Z*(H[2]-Bevel)},{X*(H[0]-Bevel),Y*(H[1]-Bevel),Z*H[2]},FVector(X,Y,Z).GetSafeNormal(),UV);
  }
  HouseStadiumDetailKitImpl::Describe(G,Descriptions[L]);Pointers.Add(&Descriptions[L]);
 }
 UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bFastBuild=true;Params.bBuildSimpleCollision=false;Params.bAllowCpuAccess=false;Params.bMarkPackageDirty=false;Params.bCommitMeshDescription=false;if(!M->BuildFromMeshDescriptions(Pointers,Params))return nullptr;
 M->CreateBodySetup();auto* Body=M->GetBodySetup();Body->CollisionTraceFlag=CTF_UseSimpleAsComplex;FKBoxElem Collider;Collider.X=175;Collider.Y=16;Collider.Z=4;Body->AggGeom.BoxElems.Add(Collider);M->GetRenderData()->ScreenSize[0].Default=1;M->GetRenderData()->ScreenSize[1].Default=.025f;return M;
}
UStaticMesh* HouseStadiumDetail::Kit(UHouseBlackHallSubsystem* Look,bool Tube)
{
 auto* M=NewObject<UStaticMesh>(GetTransientPackage(),Tube?TEXT("CalderStadiumPipeKit"):TEXT("CalderStadiumBoxKit"),RF_Transient);M->NeverStream=true;M->bAllowCPUAccess=false;const FName Key(TEXT("SD_Steel"));M->GetStaticMaterials().Add(FStaticMaterial(Look->Mat(Key),Key
#if WITH_EDITORONLY_DATA
 ,Key
#endif
 ));
 FMeshDescription D[2];TArray<const FMeshDescription*> Descriptions;
 for(int32 L=0;L<2;++L){FHallMesh G;if(Tube)G.Cylinder(Key,{0,0,-50},{0,0,50},50,L==0?10:5,FHallUV::World(100));else G.Box(Key,FTransform::Identity,{50,50,50},FHallUV::World(100));HouseStadiumDetailKitImpl::Describe(G,D[L]);Descriptions.Add(&D[L]);}
 UStaticMesh::FBuildMeshDescriptionsParams P;P.bFastBuild=true;P.bBuildSimpleCollision=false;P.bAllowCpuAccess=false;P.bMarkPackageDirty=false;P.bCommitMeshDescription=false;if(!M->BuildFromMeshDescriptions(Descriptions,P))return nullptr;
 M->CreateBodySetup();auto* Body=M->GetBodySetup();Body->CollisionTraceFlag=CTF_UseSimpleAsComplex;FKBoxElem E;E.X=E.Y=E.Z=100.f;Body->AggGeom.BoxElems.Add(E);M->GetRenderData()->ScreenSize[0].Default=1.f;M->GetRenderData()->ScreenSize[1].Default=.025f;return M;
}
UStaticMesh* HouseStadiumDetail::GrassKit(UHouseBlackHallSubsystem* Look)
{
 auto* M=NewObject<UStaticMesh>(GetTransientPackage(),TEXT("CalderStadiumGrassKit"),RF_Transient);M->NeverStream=true;const FName Key(TEXT("SD_GrassNear"));M->GetStaticMaterials().Add(FStaticMaterial(Look->Mat(Key),Key
#if WITH_EDITORONLY_DATA
 ,Key
#endif
 ));FMeshDescription D[2];TArray<const FMeshDescription*> Pointers;
 for(int32 L=0;L<2;++L){FHallMesh G;FRandomStream R(861018);for(int32 I=0;I<(L?8:40);++I){const FVector P(R.FRandRange(-48,48),R.FRandRange(-48,48),0);const float Angle=R.FRandRange(0,2*PI),H=R.FRandRange(3,7);const FVector Side(FMath::Cos(Angle),FMath::Sin(Angle),0),Bend(-Side.Y,Side.X,0),N=FVector::CrossProduct(Side,FVector::UpVector);const FVector A=P-Side*.32f,B=P+Side*.32f,C=P+Bend*1.8f+FVector(0,0,H),Tip=P+Bend*3+FVector(0,0,H+1);for(float Sign:{-1.f,1.f}){G.Quad(Key,A,B,C,Tip,N*Sign,FHallUV::World(100));}}HouseStadiumDetailKitImpl::Describe(G,D[L]);Pointers.Add(&D[L]);}
 UStaticMesh::FBuildMeshDescriptionsParams P;P.bFastBuild=true;P.bBuildSimpleCollision=false;P.bAllowCpuAccess=false;P.bMarkPackageDirty=false;P.bCommitMeshDescription=false;if(!M->BuildFromMeshDescriptions(Pointers,P))return nullptr;M->GetRenderData()->ScreenSize[0].Default=1;M->GetRenderData()->ScreenSize[1].Default=.35f;return M;
}
