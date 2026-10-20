// Construction details are merged into the STA loader's existing meshes and freed with the room.
void Screw(FHallMesh& M,FVector C,FVector N,float Radius=.45f)
{
 const FVector R(-N.Y,N.X,0);M.Cylinder(TEXT("SD_Aluminium"),C,C+N*.25f,Radius,8,FHallUV::World(100));
 HouseCampusFallKit::Bar(M,TEXT("SD_Dark"),C+N*.28f-R*Radius*.65f,C+N*.28f+R*Radius*.65f,.10f,.08f,N);
}
void Piping(FHallMesh& M,FName Key,FVector Centre,FVector X,FVector Y,float HalfX,float HalfY,float Z,float Radius)
{for(float S:{-1.f,1.f}){M.Cylinder(Key,Centre+X*(S*HalfX)-Y*HalfY+FVector(0,0,Z),Centre+X*(S*HalfX)+Y*HalfY+FVector(0,0,Z),Radius,6,FHallUV::World(100));M.Cylinder(Key,Centre+Y*(S*HalfY)-X*HalfX+FVector(0,0,Z),Centre+Y*(S*HalfY)+X*HalfX+FVector(0,0,Z),Radius,6,FHallUV::World(100));}}
void Jersey(FHallMesh& M,FVector C,FVector Along,FVector N,int32 Number)
{
 const TArray<FVector2D> Shape={{-10,0},{10,0},{12,38},{18,30},{23,43},{12,54},{5,57},{-5,57},{-12,54},{-23,43},{-18,30},{-12,38}};
 // A shaped, shallow garment: shoulder/sleeve outline, side thickness, collar and sewn hem.
 TArray<FVector> Front;for(const auto& Q:Shape)Front.Add(C+Along*Q.X+FVector(0,0,Q.Y)+N*(1.2f+FMath::Cos(Q.X*.2f)*.65f));
 HouseCampusFallKit::Fan(M,TEXT("SD_Jersey"),C+N*2+FVector(0,0,28),Front,N);
 for(int32 J=0;J<Shape.Num();++J){const int32 K=(J+1)%Shape.Num();M.Quad(TEXT("SD_Jersey"),Front[J]-N*1.5f,Front[K]-N*1.5f,Front[K],Front[J],FVector::CrossProduct(Front[K]-Front[J],N).GetSafeNormal(),FHallUV::World(80));}
 HouseCampusFallKit::Bar(M,TEXT("SD_FabricEdge"),C-Along*9+N*2+FVector(0,0,1),C+Along*9+N*2+FVector(0,0,1),.6f,.15f,N);
 const TArray<FVector> Collar={C-Along*5+N*2+FVector(0,0,56),C+N*2+FVector(0,0,51),C+Along*5+N*2+FVector(0,0,56)};
 for(int32 J=1;J<Collar.Num();++J)M.Cylinder(TEXT("SD_Cream"),Collar[J-1],Collar[J],.5f,6,FHallUV::World(100));
 Word(M,FString::FromInt(Number),C+N*2.1f+FVector(0,0,24),Along,N,13.f);
}
