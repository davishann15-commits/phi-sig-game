// Additive actual-pawn office route and door interactions. Invoked independently of all original review routes.
namespace HouseStadiumDetailCoachReview
{
struct FState
{
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACharacter> Walker;TWeakObjectPtr<AHouseCampusInteriors> Interiors;TWeakObjectPtr<ACampusDoor> Door;
 int32 Phase=0,Stop=0,Reached=0,Doors=0,Indoor=0,ShellRays=0,MaxLights=0;double Since=0,Start=0,Progress=0;float Best=FLT_MAX;FVector Goal;bool Moving=false,Done=false;TArray<FString> Rows;
};
FState State;
void Finish(bool Pass,const FString& Reason)
{
 auto& S=State;if(S.Done)return;S.Done=true;S.Moving=false;if(S.Walker.IsValid())S.Walker->GetCharacterMovement()->StopMovementImmediately();
 const FString Line=FString::Printf(TEXT("%s reached=%d/8 doors=%d/4 indoor=%d/3 shellRays=%d maxLights=%d pool=6 reason=%s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),S.Reached,S.Doors,S.Indoor,S.ShellRays,S.MaxLights,*Reason);
 S.Rows.Add(Line);const FString Dir=FPaths::ProjectSavedDir()/TEXT("StadiumCoachOfficeReview");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringArrayToFile(S.Rows,*(Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_COACH_REVIEW %s"),*Line);FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
}
void Tick(UWorld* W,ELevelTick,float)
{
 if(!W||!W->IsGameWorld()||!FParse::Param(FCommandLine::Get(),TEXT("StadiumCoachOfficeReview")))return;
 auto* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!Pawn)return;auto& S=State;const double Now=W->GetTimeSeconds();
 if(S.World.Get()!=W){S=FState();S.World=W;S.Walker=Pawn;S.Start=S.Since=Now;}
 static const FDelegateHandle Input=FCoreDelegates::OnBeginFrame.AddLambda([](){auto& Live=State;if(Live.Moving&&!Live.Done&&Live.Walker.IsValid())Live.Walker->AddMovementInput((Live.Goal-Live.Walker->GetActorLocation()).GetSafeNormal2D(),1.f);});
 if(S.Done)return;if(Now-S.Start>140){Finish(false,TEXT("office route timeout"));return;}
 if(Now-S.Start<8||!HouseCalderTerrain::CollisionReady(W))return;
 const int32 Seed=HouseCampusInteriors::SeedFor(TEXT("STA"));const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 if(!S.Interiors.IsValid()){TActorIterator<AHouseCampusInteriors> It(W);if(It)S.Interiors=*It;}
 if(!S.Door.IsValid())for(TActorIterator<ACampusDoor> It(W);It;++It)if(It->DoorId==TEXT("Campus_STA_CoachOffice")){S.Door=*It;break;}
 if(!S.Interiors.IsValid()||!S.Door.IsValid())return;
 if(S.Phase==0){S.Interiors->BuildNow(Seed);Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation(HouseStadiumDetail::CoachPoint(0,155,-55+Half+3),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);S.Phase=1;S.Since=Now;return;}
 if(Now-S.Since<1.8)return;
 S.MaxLights=FMath::Max(S.MaxLights,S.Interiors->ActiveLights());if(S.MaxLights>AHouseCampusInteriors::LightPool){Finish(false,TEXT("light pool exceeded"));return;}
 auto Toggle=[&](bool Want){if(S.Door->IsOpen()==Want){Finish(false,TEXT("unexpected door state before interaction"));return false;}if(!S.Door->Interact(Pawn)){Finish(false,TEXT("door interaction refused"));return false;}++S.Doors;S.Since=Now;return true;};
 if(S.Phase==1){if(S.Interiors->PendingConversions(Seed)>0)return;if(!Toggle(false))return;S.Phase=2;return;}
 if(S.Phase==2){if(S.Door->IsOpen()){Finish(false,TEXT("door failed to close"));return;}if(!Toggle(true))return;S.Phase=3;return;}
 const FVector Stops[]={HouseStadiumDetail::CoachPoint(0,65,-55),HouseStadiumDetail::CoachPoint(0,-80,-55),HouseStadiumDetail::CoachPoint(180,-80,-55),HouseStadiumDetail::CoachPoint(0,-80,-55),HouseStadiumDetail::CoachPoint(0,155,-55),HouseStadiumDetail::CoachPoint(0,-80,-55),HouseStadiumDetail::CoachPoint(0,-205,-55),HouseStadiumDetail::CoachPoint(0,155,-55)};
 if(S.Phase==3){
  if(S.Stop==8){if(S.Reached!=8||S.Doors!=4||S.Indoor!=3||S.MaxLights<=0){Finish(false,TEXT("route counters incomplete"));return;}Finish(true,TEXT("realWalkingDoorUseReturnAndIndoorClassification"));return;}
  if(S.Stop==5&&S.Doors==2){if(!Toggle(false))return;S.Phase=4;return;}
  S.Goal=Stops[S.Stop]+FVector(0,0,Half);S.Best=FLT_MAX;S.Progress=Now;S.Moving=true;S.Phase=5;return;
 }
 if(S.Phase==4){if(S.Door->IsOpen()){Finish(false,TEXT("second close failed"));return;}if(!Toggle(true))return;S.Phase=3;return;}
 if(S.Phase!=5)return;
 const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);const float Distance=FVector::Dist2D(Feet,Stops[S.Stop]);PC->SetControlRotation((S.Goal-Pawn->GetActorLocation()).Rotation());
 if(Distance<S.Best-3){S.Best=Distance;S.Progress=Now;}if(Now-S.Progress>7){Finish(false,FString::Printf(TEXT("stalled stop%d at%s remaining%.2f"),S.Stop,*Feet.ToString(),Distance));return;}
 if(Distance>10)return;S.Moving=false;Pawn->GetCharacterMovement()->StopMovementImmediately();if(FMath::Abs(Feet.Z+55)>5){Finish(false,TEXT("wrong office floor"));return;}
 if(S.Stop==1||S.Stop==2||S.Stop==6){if(!HouseCampusInteriors::InsideInterior(Seed,Feet+FVector(0,0,100))||HouseCampusInteriors::DistanceOutside(Feet+FVector(0,0,150))>0.1||!S.Interiors->Loaded().Contains(Seed)){Finish(false,TEXT("office failed indoor or loaded test"));return;}++S.Indoor;}
 if(S.Stop==1){
  FCollisionQueryParams Q(SCENE_QUERY_STAT(CoachOfficeShell),true,Pawn);const FVector Centre=HouseStadiumDetail::CoachPoint(100,-140,60);const FVector X=(HouseStadiumDetail::CoachPoint(1,0,0)-HouseStadiumDetail::CoachPoint(0,0,0)).GetSafeNormal(),Y(-X.Y,X.X,0);
  for(const FVector Direction:{X,-X,-Y,FVector::UpVector}){FHitResult Hit;++S.ShellRays;if(!W->LineTraceSingleByChannel(Hit,Centre,Centre+Direction*650,ECC_Pawn,Q)||!GetNameSafe(Hit.GetComponent()).Contains(TEXT("CampusInterior_STA_Solid"))){Finish(false,TEXT("office shell ray leaked"));return;}}
 }
 ++S.Reached;UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_COACH_STOP index=%d feet=%s floor=-55 indoors=%d lights=%d doorOpen=%d"),S.Stop,*Feet.ToString(),int(HouseStadiumDetail::CoachInside(Feet+FVector(0,0,100))),S.Interiors->ActiveLights(),int(S.Door->IsOpen()));S.Rows.Add(FString::Printf(TEXT("stop=%d feet=%s"),S.Stop,*Feet.ToString()));++S.Stop;S.Phase=3;S.Since=Now-1.8;
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
