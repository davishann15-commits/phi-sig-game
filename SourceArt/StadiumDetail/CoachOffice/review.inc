  if(HouseStadiumDetail::Step()>=7){
   // Add the owner-authorized office to the full-height collision review; no original samples are removed.
   auto OfficeLane=[&](FVector A,FVector B){FVector Previous;for(int32 J=0;J<=16;++J){const FVector P=FMath::Lerp(A,B,J/16.f);CheckFloor(P,TEXT("coach-office-lane"));if(J){FHitResult H;++Sweeps;if(W->SweepSingleByChannel(H,Previous+FVector(0,0,97),P+FVector(0,0,97),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,96),Q)){++Blocked;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("coach office blocked at=%s end=%s component=%s"),*Previous.ToString(),*P.ToString(),*GetNameSafe(H.GetComponent())));}}Previous=P;}};
   OfficeLane(HouseStadiumDetail::CoachPoint(0,180,-55),HouseStadiumDetail::CoachPoint(0,-210,-55));
   OfficeLane(HouseStadiumDetail::CoachPoint(0,-80,-55),HouseStadiumDetail::CoachPoint(180,-80,-55));
   for(const FVector P:{FVector(5011.463,-10995.147,-55),FVector(5213.259,-11204.916,-55)}){CheckFloor(P,TEXT("coach-office-view-floor"));FHitResult H;++Sweeps;if(W->SweepSingleByChannel(H,P+FVector(0,0,97),P+FVector(0,0,98),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,96),Q)){++Blocked;OK=false;Problems.Add(FString::Printf(TEXT("coach office view blocked at=%s component=%s"),*P.ToString(),*GetNameSafe(H.GetComponent())));}}
  }
