#include "SeniorCharacterAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshLODSettings.h"
#include "Animation/MorphTarget.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if WITH_EDITOR
#include "ClothingAssetFactory.h"
#include "ClothingAsset.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Engine/StaticMesh.h"
#include "GroomAsset.h"
#include "HairDescription.h"
#include "StaticMeshAttributes.h"
#include "ReferenceSkeleton.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterIdentity.h"
#include "MetaHumanCharacterBodyIdentity.h"
#include "SkelMeshDNAUtils.h"
#include "DNAUtils.h"
#endif

bool USeniorCharacterAssetTools::ExportBraxtonScalpReference()
{
#if WITH_EDITOR
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/MetaHumans/BraxtonRebuild/MH_Braxton_Rebuild/Face/SKM_MH_Braxton_Rebuild_FaceMesh"));
    if(!Mesh || !Mesh->GetImportedModel() || Mesh->GetImportedModel()->LODModels.Num()==0)return false;
    const auto& LOD=Mesh->GetImportedModel()->LODModels[0];
    TArray<FSoftSkinVertex> Vertices; LOD.GetVertices(Vertices);
    FString Text;
    for(const auto& V:Vertices) Text+=FString::Printf(TEXT("v %.6f %.6f %.6f\n"),V.Position.X,-V.Position.Y,V.Position.Z);
    for(int32 I=0;I+2<LOD.IndexBuffer.Num();I+=3)
        Text+=FString::Printf(TEXT("f %u %u %u\n"),LOD.IndexBuffer[I]+1,LOD.IndexBuffer[I+2]+1,LOD.IndexBuffer[I+1]+1);
    return FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("SourceArt/BraxtonHoodie/NativeFace.obj")));
#else
    return false;
#endif
}

bool USeniorCharacterAssetTools::MatchBraxtonGarmentBindPose(USkeletalMesh* Outfit, USkeletalMesh* Body)
{
#if WITH_EDITOR
    if (!Outfit || !Body ||
        !(Outfit->GetPathName().Contains(TEXT("/Details/Hoodie/")) ||
          Outfit->GetPathName().Contains(TEXT("/MH_Runner_Working/Details/RunnerOutfit/")) ||
          Outfit->GetPathName().Contains(TEXT("/MetaHumans/Fixer/MH_Fixer/Details/")))) return false;
    const FReferenceSkeleton& Native = Body->GetRefSkeleton();
    FReferenceSkeleton& Ref = Outfit->GetRefSkeleton();
    for (int32 I=0; I<Ref.GetRawBoneNum(); ++I)
    {
        const int32 Other = Native.FindBoneIndex(Ref.GetBoneName(I));
        if (Other == INDEX_NONE)
        {
            UE_LOG(LogTemp, Error, TEXT("RUNNER_BIND_MISSING_BONE index=%d name=%s"), I, *Ref.GetBoneName(I).ToString());
            return false;
        }
        const int32 Parent = Ref.GetParentIndex(I), NativeParent = Native.GetParentIndex(Other);
        if ((Parent == INDEX_NONE) != (NativeParent == INDEX_NONE))
        {
            UE_LOG(LogTemp, Error, TEXT("RUNNER_BIND_ROOT_MISMATCH name=%s importedParent=%s nativeParent=%s"),
                *Ref.GetBoneName(I).ToString(), Parent == INDEX_NONE ? TEXT("<none>") : *Ref.GetBoneName(Parent).ToString(),
                NativeParent == INDEX_NONE ? TEXT("<none>") : *Native.GetBoneName(NativeParent).ToString());
            return false;
        }
        if (Parent != INDEX_NONE && Ref.GetBoneName(Parent) != Native.GetBoneName(NativeParent))
        {
            UE_LOG(LogTemp, Error, TEXT("RUNNER_BIND_PARENT_MISMATCH name=%s importedParent=%s nativeParent=%s"),
                *Ref.GetBoneName(I).ToString(), *Ref.GetBoneName(Parent).ToString(), *Native.GetBoneName(NativeParent).ToString());
            return false;
        }
    }
    Outfit->Modify();
    int32 Changed = 0; double MaxAngle = 0, MaxPosition = 0;
    {
        FReferenceSkeletonModifier Modifier(Ref, Outfit->GetSkeleton());
        for (int32 I=0; I<Ref.GetRawBoneNum(); ++I)
        {
            const FTransform Pose = Native.GetRawRefBonePose()[Native.FindBoneIndex(Ref.GetBoneName(I))];
            const FTransform Old = Ref.GetRawRefBonePose()[I];
            const double Angle = FMath::RadiansToDegrees(Old.GetRotation().AngularDistance(Pose.GetRotation()));
            const double Distance = FVector::Distance(Old.GetTranslation(),Pose.GetTranslation());
            MaxAngle=FMath::Max(MaxAngle,Angle); MaxPosition=FMath::Max(MaxPosition,Distance);
            if (Angle > .05 || Distance > .01) ++Changed;
            Modifier.UpdateRefPoseTransform(I,Pose);
        }
    }
    Outfit->GetRefBasesInvMatrix().Reset();
    Outfit->CalculateInvRefMatrices();
    Outfit->MarkPackageDirty();
    UE_LOG(LogTemp,Display,TEXT("BRAXTON_GARMENT_BIND_POSE: changed=%d angle=%.4f distance=%.4f"),Changed,MaxAngle,MaxPosition);
    return true;
#else
    return false;
#endif
}

bool USeniorCharacterAssetTools::ShapeBraxtonHair(UGroomAsset* Groom)
{
#if WITH_EDITOR
    if (!Groom || !Groom->GetPathName().Contains(TEXT("/Details/Hoodie/Hair/"))) return false;
    auto Shape = [](FVector3f P)
    {
        const float Front = FMath::SmoothStep(2.f, 7.f, P.Y);
        if (P.Z < 167.f) P.Z += Front * (167.f-P.Z) * .70f;
        const float Back = 1.f - FMath::SmoothStep(-3.f, 2.f, P.Y);
        if (P.Z < 157.f) P.Z += Back * (157.f-P.Z) * .35f;
        const float Crown = FMath::SmoothStep(165.f, 174.f, P.Z);
        P.X *= 1.f + .035f * Crown;
        P.Z += .45f * Crown;
        return P;
    };
    Groom->Modify();
    FHairDescription Description = Groom->GetHairDescription();
    if (!Description.IsValid()) return false;
    auto Positions = Description.VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Position);
    FBox Before(ForceInit), After(ForceInit);
    for (int32 I=0; I<Description.GetNumVertices(); ++I)
    {
        FVector3f& P = Positions[FVertexID(I)];
        Before += FVector(P); P=Shape(P); After += FVector(P);
    }
    Groom->CommitHairDescription(MoveTemp(Description), EHairDescriptionType::Source);
    for (FHairGroupsCardsSourceDescription& Card : Groom->GetHairGroupsCards())
        if (UStaticMesh* Mesh = Card.ImportedMesh)
        {
            if (!Mesh->GetPathName().Contains(TEXT("/Details/Hoodie/Hair/"))) return false;
            Mesh->Modify();
            for (int32 L=0; L<Mesh->GetNumSourceModels(); ++L)
                if (FMeshDescription* Data = Mesh->GetMeshDescription(L))
                {
                    FStaticMeshAttributes Attributes(*Data);
                    auto V = Attributes.GetVertexPositions();
                    for (FVertexID Id : Data->Vertices().GetElementIDs()) V[Id]=Shape(V[Id]);
                    Mesh->CommitMeshDescription(L);
                }
            Mesh->Build(false); Mesh->MarkPackageDirty();
        }
    const bool bBuilt = Groom->CacheDerivedDatas();
    Groom->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("BRAXTON_HAIR_SHAPED: %s -> %s built=%d"), *Before.ToString(), *After.ToString(), bBuilt);
    return bBuilt;
#else
    return false;
#endif
}
bool USeniorCharacterAssetTools::PrepareFixerLeanSource(UObject* Object)
{
#if WITH_EDITOR
    UMetaHumanCharacter* Character=Cast<UMetaHumanCharacter>(Object);
    if (!Character || Character->GetPathName()!=TEXT("/Game/Characters/MetaHumans/Fixer/MH_Fixer_Lean.MH_Fixer_Lean")) return false;
    UMetaHumanCharacterEditorSubsystem* Editor=UMetaHumanCharacterEditorSubsystem::Get();
    if (!Editor || !Editor->IsObjectAddedForEditing(Character)) return false;
    UE_LOG(LogTemp,Display,TEXT("FIXER_LEAN_SOURCE fixed=%d bodyDNA=%d"),Character->bFixedBodyType,Character->HasBodyDNA());
    if (Character->bFixedBodyType)
    {
        const bool bFit=Character->HasBodyDNA() ? Editor->ParametricFitToDnaBody(Character) : Editor->ParametricFitToCompatibilityBody(Character);
        if (!bFit) return false;
        Editor->CommitBodyState(Character);
    }
    return !Character->bFixedBodyType;
#else
    return false;
#endif
}

bool USeniorCharacterAssetTools::BakeFixerLocalShape(UObject* Object)
{
#if WITH_EDITOR
    UMetaHumanCharacter* Character=Cast<UMetaHumanCharacter>(Object);
    if (!Character || Character->GetPathName()!=TEXT("/Game/Characters/MetaHumans/Fixer/MH_Fixer_Lean.MH_Fixer_Lean")) return false;
    auto* Editor=UMetaHumanCharacterEditorSubsystem::Get();
    if (!Editor || !Editor->IsObjectAddedForEditing(Character)) return false;
    // Assembly consumes stored rig DNA. Committing editable landmarks alone
    // leaves its earlier neutral shape in place. Bake the LOCAL edits over the
    // existing rig, preserving topology, weights and animation behavior.
    const USkeletalMesh* ConstBody=Editor->GetBodyEditMesh(Character);
    const USkeletalMesh* ConstFace=Editor->GetFaceEditMesh(Character);
    auto* Body=const_cast<USkeletalMesh*>(ConstBody);
    auto* Face=const_cast<USkeletalMesh*>(ConstFace);
    const auto BodyTemplate=USkelMeshDNAUtils::GetDNAReader(Body);
    const auto FaceTemplate=USkelMeshDNAUtils::GetDNAReader(Face);
    if (!BodyTemplate || !FaceTemplate) return false;
    const auto BodyDNA=Editor->GetBodyState(Character)->StateToDna(BodyTemplate->Unwrap());
    auto FaceDNA=Editor->GetFaceState(Character)->StateToDna(FaceTemplate->Unwrap());
    const auto AlignedFace=Editor->AlignFaceDNAWithBody(Character,FaceDNA);
    if (!AlignedFace) return false;
    TArray<uint8> BodyBuffer, FaceBuffer;
    SaveDNAToBuffer(&BodyDNA.Get(),EDNADataLayer::All,BodyBuffer);
    SaveDNAToBuffer(AlignedFace.Get(),EDNADataLayer::All,FaceBuffer);
    if (BodyBuffer.IsEmpty() || FaceBuffer.IsEmpty()) return false;
    Character->Modify();
    Character->SetBodyDNABuffer(BodyBuffer);
    Character->SetFaceDNABuffer(FaceBuffer,AlignedFace->GetBlendShapeChannelCount()>0);
    Character->MarkPackageDirty();
    UE_LOG(LogTemp,Display,TEXT("FIXER_LOCAL_SHAPE_BAKED bodyBytes=%d faceBytes=%d"),BodyBuffer.Num(),FaceBuffer.Num());
    return true;
#else
    return false;
#endif
}

bool USeniorCharacterAssetTools::ShapeFixerShortMullet(UGroomAsset* Groom)
{
#if WITH_EDITOR
    const FString Prefix(TEXT("/Game/MetaHumans/Fixer/MH_Fixer/Details/Hair/Mullet/"));
    if (!Groom || !Groom->GetPathName().StartsWith(Prefix)) return false;
    FHairDescription Description = Groom->GetHairDescription();
    if (!Description.IsValid()) return false;
    auto Positions = Description.VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Position);
    FBox Before(ForceInit), After(ForceInit);
    for (int32 I=0; I<Description.GetNumVertices(); ++I) Before += FVector(Positions[FVertexID(I)]);
    const float Top = float(Before.Max.Z);
    const auto Shape = [Top](FVector3f P)
    {
        // Short, close side silhouette with a modest textured tail at the nape.
        // All authoring is local to this duplicate; no shared groom is changed.
        const float Back = 1.f-FMath::SmoothStep(-3.f, 2.f, P.Y);
        const float Nape = 1.f-FMath::SmoothStep(Top-19.f, Top-10.f, P.Z);
        const float Side = FMath::SmoothStep(4.5f, 8.5f, FMath::Abs(P.X));
        const float Front = FMath::SmoothStep(1.f, 6.f, P.Y);
        const float Fringe = 1.f-FMath::SmoothStep(Top-10.f, Top-5.f, P.Z);
        P.X *= 1.f-.035f*Side*(1.f-Back*Nape);
        P.Z += Front*Fringe*1.35f;
        const float CenterTail=1.f-FMath::SmoothStep(3.f,7.f,FMath::Abs(P.X));
        P.Z -= Back*Nape*(.9f+1.8f*CenterTail+.18f*FMath::Sin(P.X*2.1f));
        P.Y -= Back*Nape*.65f;
        P.Z -= .45f*FMath::SmoothStep(Top-5.f, Top, P.Z);
        return P;
    };
    Groom->Modify();
    for (int32 I=0; I<Description.GetNumVertices(); ++I)
    {
        FVector3f& P=Positions[FVertexID(I)]; P=Shape(P); After+=FVector(P);
    }
    Groom->CommitHairDescription(MoveTemp(Description),EHairDescriptionType::Source);
    for (FHairGroupsCardsSourceDescription& Card : Groom->GetHairGroupsCards())
        if (UStaticMesh* Mesh=Card.ImportedMesh)
        {
            if (!Mesh->GetPathName().StartsWith(Prefix)) return false;
            Mesh->Modify();
            for (int32 L=0;L<Mesh->GetNumSourceModels();++L)
                if (FMeshDescription* Data=Mesh->GetMeshDescription(L))
                {
                    FStaticMeshAttributes Attributes(*Data);
                    auto V=Attributes.GetVertexPositions();
                    for (FVertexID Id : Data->Vertices().GetElementIDs()) V[Id]=Shape(V[Id]);
                    Mesh->CommitMeshDescription(L);
                }
            Mesh->Build(false);Mesh->MarkPackageDirty();
        }
    const bool bBuilt=Groom->CacheDerivedDatas();
    Groom->MarkPackageDirty();
    UE_LOG(LogTemp,Display,TEXT("FIXER_MULLET_SHAPED: %s -> %s built=%d"),*Before.ToString(),*After.ToString(),bBuilt);
    return bBuilt;
#else
    return false;
#endif
}

bool USeniorCharacterAssetTools::PrepareBraxtonHoodieExport(USkeletalMesh* Mesh)
{
#if WITH_EDITOR
    if (!Mesh || !Mesh->GetPathName().Contains(TEXT("/Details/Hoodie/"))) return false;
    Mesh->Modify();
    FScopedSkeletalMeshPostEditChange ScopedMeshEdit(Mesh);
    for (UClothingAssetBase* Base : Mesh->GetMeshClothingAssets())
        if (UClothingAssetCommon* Cloth = Cast<UClothingAssetCommon>(Base)) Cloth->UnbindFromSkeletalMesh(Mesh);
    for (FSkeletalMeshLODModel& LOD : Mesh->GetImportedModel()->LODModels)
    {
        for (FSkelMeshSection& Section : LOD.Sections)
        {
            Section.ClothingData = FClothingSectionData();
            Section.CorrespondClothAssetIndex = INDEX_NONE;
        }
        for (auto& Pair : LOD.UserSectionsData)
        {
            Pair.Value.ClothingData = FClothingSectionData();
            Pair.Value.CorrespondClothAssetIndex = INDEX_NONE;
        }
    }
    Mesh->GetMeshClothingAssets().Reset();
    Mesh->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}
bool USeniorCharacterAssetTools::PrepareBraxtonDetailLevels(USkeletalMesh* Mesh)
{
#if WITH_EDITOR
    if (!Mesh) return false;
    const bool bHoodie = Mesh->GetPathName().Contains(TEXT("/Details/Hoodie/SK_Braxton_HoodieOutfit"));
    if (!bHoodie && Mesh->GetPathName() != TEXT("/Game/Characters/Cobble/C01/SK_C01.SK_C01")) return false;
    Mesh->Modify();
    if (bHoodie) while (Mesh->GetNumSourceModels() < 3) Mesh->AddLODInfo();
    for (int32 I = 1; I < Mesh->GetNumSourceModels(); ++I)
    {
        FSkeletalMeshLODInfo* Info = Mesh->GetLODInfo(I);
        if (!Info) return false;
        Info->ReductionSettings.bRemapMorphTargets = true;
        Info->ReductionSettings.BaseLOD = 0;
        Info->ReductionSettings.NumOfTrianglesPercentage = bHoodie ? (I == 1 ? .34f : .085f) : (I == 1 ? .32f : .15f);
        UE_LOG(LogTemp, Display, TEXT("BRAXTON_LOD_PREPARED: %d remap=%d"), I, Info->ReductionSettings.bRemapMorphTargets);
    }
    Mesh->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

bool USeniorCharacterAssetTools::BuildBraxtonCloth(USkeletalMesh* Outfit, USkeletalMesh* Body)
{
#if WITH_EDITOR
    if (!Outfit || !Body || !Outfit->GetPathName().StartsWith(TEXT("/Game/MetaHumans/BraxtonRebuild/"))) return false;
    Outfit->Modify();
    FScopedSkeletalMeshPostEditChange ScopedMeshEdit(Outfit);
    const bool bFittedHoodie = Outfit->GetPathName().Contains(TEXT("/Details/Hoodie/"));
    auto BindSection = [Outfit](UClothingAssetCommon* Cloth, int32 Section)
    {
        const int32 AssetIndex = Outfit->GetMeshClothingAssets().IndexOfByKey(Cloth);
        const bool bAttachHoodieDetails = Section == 1 && Outfit->GetPathName().Contains(TEXT("/Details/Hoodie/"));
        for (int32 L = 0; L < Outfit->GetLODNum(); ++L)
        {
            FSkeletalMeshLODModel& LodModel = Outfit->GetImportedModel()->LODModels[L];
            if (!LodModel.Sections.IsValidIndex(Section)) continue;
            const int32 EndSection = bAttachHoodieDetails ? LodModel.Sections.Num() : Section+1;
            for (int32 S=Section; S<EndSection; ++S)
            {
                // Relaxed hoodie trims have their own skin weights. Mapping cuff
                // bands to the torso simulation surface stretches them on arm lifts.
                if (S > 1 && (Outfit->GetName().Contains(TEXT("HoodieOutfitV4")) || Outfit->GetName().Contains(TEXT("HoodieOutfitV5")) || Outfit->GetName().Contains(TEXT("HoodieOutfitV6")))) continue;
                const int32 MaterialIndex=LodModel.Sections[S].MaterialIndex;
                if(Outfit->GetMaterials().IsValidIndex(MaterialIndex))
                {
                    const FString Slot=Outfit->GetMaterials()[MaterialIndex].MaterialSlotName.ToString();
                    // Shoes follow the foot/toe skin weights, never the shirt's
                    // simulation cage. The volumetric hood retains its sewn shape.
                    if(Slot.Contains(TEXT("Sandal")) || Slot.Contains(TEXT("HoodieDetail")) || Slot.Contains(TEXT("HairUnderlayer"))) continue;
                }
                // Every material section uses the same physical shirt surface.
                // Re-open this identical LOD mapping while constructing the extra
                // render bindings; there is still only one simulated shirt actor.
                if (Cloth->LodMap.IsValidIndex(L)) Cloth->LodMap[L]=INDEX_NONE;
                if (!Cloth->BindToSkeletalMesh(Outfit, L, S, L)) return false;
                FSkelMeshSourceSectionUserData& Source = LodModel.UserSectionsData.FindOrAdd(LodModel.Sections[S].OriginalDataSectionIndex);
                Source.CorrespondClothAssetIndex = int16(AssetIndex);
                Source.ClothingData.AssetGuid = Cloth->GetAssetGuid();
                Source.ClothingData.AssetLodIndex = L;
            }
        }
        return true;
    };
    if (Outfit->GetMeshClothingAssets().Num() >= 2)
    {
        for (int32 Section = 0; Section < 2; ++Section)
        {
            UClothingAssetCommon* Cloth = Cast<UClothingAssetCommon>(Outfit->GetMeshClothingAssets()[Section]);
            if (!Cloth) return false;
            Cloth->Modify();
            Cloth->UnbindFromSkeletalMesh(Outfit);
            for (FClothLODDataCommon& LOD : Cloth->LodData)
            {
                const bool bShirt = Section == 1;
                for (FPointWeightMap& Mask : LOD.PointWeightMaps)
                    if (Mask.CurrentTarget == uint8(EWeightMapTargetCommon::MaxDistance))
                        for (int32 I = 0; I < Mask.Values.Num(); ++I)
                        {
                            const float Z = LOD.PhysicalMeshData.Vertices[I].Z;
                            const float Release = bShirt ? FMath::Clamp((123.f-Z)/25.f,0.f,1.f) : FMath::Clamp((89.5f-Z)/22.9f,0.f,1.f);
                            const float Seam = bFittedHoodie ? FMath::SmoothStep(bShirt ? 90.5f : 66.5f, bShirt ? 96.f : 70.5f, Z) : 1.f;
                            Mask.Values[I] = FMath::Square(Release) * Seam * (bFittedHoodie ? (bShirt ? .65f : .5f) : (bShirt ? 1.25f : .9f));
                        }
            }
            if (UChaosClothConfig* Config = Cast<UChaosClothConfig>(Cloth->ClothConfigs.FindRef(UChaosClothConfig::StaticClass()->GetFName())))
            {
                Config->CollisionThickness = .2f;
                Config->AnimDriveStiffness = { .5f, .5f };
                Config->AnimDriveDamping = { .55f, .55f };
                Config->BendingStiffnessWeighted = { .4f, .4f };
            }
            Cloth->ApplyParameterMasks(true,true);
            Cloth->RefreshBoneMapping(Outfit);
            Cloth->InvalidateAllCachedData();
            for (FClothLODDataCommon& LOD : Cloth->LodData)
                LOD.PhysicalMeshData.CalculateNumInfluences();
            if (!BindSection(Cloth, Section)) return false;
        }
        Outfit->MarkPackageDirty();
        return true;
    }
    const bool bHoodie = Outfit->GetPathName().Contains(TEXT("/Details/Hoodie/"));
    const int32 SimLOD = FMath::Min(bHoodie ? 1 : 2, Outfit->GetLODNum() - 1);
    FSkeletalMeshLODModel& Model = Outfit->GetImportedModel()->LODModels[SimLOD];
    UClothingAssetFactory* Factory = NewObject<UClothingAssetFactory>();
    const int32 Sections = FMath::Min(2, Model.Sections.Num());
    for (int32 Section = 0; Section < Sections; ++Section)
    {
        FSkeletalMeshClothBuildParams Params;
        Params.AssetName = FString::Printf(TEXT("BraxtonGarment_%d"), Section);
        Params.LodIndex = SimLOD;
        Params.SourceSection = Section;
        Params.bRemoveFromMesh = false;
        Params.PhysicsAsset = Body->GetPhysicsAsset();
        UClothingAssetCommon* Cloth = Cast<UClothingAssetCommon>(Factory->CreateFromSkeletalMesh(Outfit, Params));
        if (!Cloth || Cloth->LodData.IsEmpty()) return false;
        FClothLODDataCommon& LOD = Cloth->LodData[0];
        const TArray<FVector3f>& Vertices = LOD.PhysicalMeshData.Vertices;
        float MaxZ = -BIG_NUMBER, MinZ = BIG_NUMBER;
        for (const FVector3f& V : Vertices) { MaxZ = FMath::Max(MaxZ, V.Z); MinZ = FMath::Min(MinZ, V.Z); }
        const bool bShirt = MaxZ > 125.f;
        FPointWeightMap Mask(Vertices.Num());
        Mask.Name = TEXT("Pinned seams, soft free hems");
        Mask.CurrentTarget = uint8(EWeightMapTargetCommon::MaxDistance);
        Mask.bEnabled = true;
        for (int32 I = 0; I < Vertices.Num(); ++I)
        {
            const FVector3f& V = Vertices[I];
            // The shoulders/neck and waistband stay anchored. The lower fabric has a
            // smooth freedom gradient, bounded in cm to prevent spikes on rapid spins.
            const float Release = bShirt ? FMath::Clamp((123.f - V.Z) / 25.f, 0.f, 1.f)
                : FMath::Clamp((MaxZ - 6.f - V.Z) / FMath::Max(MaxZ - MinZ - 6.f, 1.f), 0.f, 1.f);
            const float Seam = bFittedHoodie ? FMath::SmoothStep(bShirt ? 90.5f : 66.5f, bShirt ? 96.f : 70.5f, V.Z) : 1.f;
            Mask.Values[I] = FMath::Square(Release) * Seam * (bFittedHoodie ? (bShirt ? .65f : .5f) : (bShirt ? 1.25f : .9f));
        }
        LOD.PointWeightMaps.Reset();
        LOD.PointWeightMaps.Add(Mask);
        LOD.bSmoothTransition = true;
        LOD.bUseMultipleInfluences = !bHoodie;
        const int32 SimVertexCount = Vertices.Num();
        const FClothLODDataCommon TemplateLOD = LOD;
        while (Cloth->LodData.Num() < Outfit->GetLODNum()) Cloth->LodData.Add(TemplateLOD);
        UChaosClothConfig* Config = NewObject<UChaosClothConfig>(Cloth);
        Config->MassMode = EClothMassMode::TotalMass;
        Config->TotalMass = bShirt ? .22f : .16f;
        Config->DampingCoefficient = .25f;
        Config->BendingStiffnessWeighted = { .4f, .4f };
        Config->AnimDriveStiffness = { .5f, .5f };
        Config->AnimDriveDamping = { .55f, .55f };
        Config->CollisionThickness = .2f;
        Config->bUseSelfCollisions = false;
        Cloth->ClothConfigs.Add(Config->GetClass()->GetFName(), Config);
        Cloth->ClothConfigs.Add(UChaosClothSharedSimConfig::StaticClass()->GetFName(), NewObject<UChaosClothSharedSimConfig>(Cloth));
        Cloth->ApplyParameterMasks(true, true);
        Outfit->AddClothingAsset(Cloth);
        Cloth->InvalidateAllCachedData();
        for (FClothLODDataCommon& ClothLOD : Cloth->LodData)
            ClothLOD.PhysicalMeshData.CalculateNumInfluences();
        if (!BindSection(Cloth, Section)) return false;
        UE_LOG(LogTemp, Display, TEXT("BRAXTON_PHYSICAL_CLOTH section=%d verts=%d minZ=%.2f maxZ=%.2f"), Section, SimVertexCount, MinZ, MaxZ);
    }
    Outfit->MarkPackageDirty();
    return Sections == 2;
#else
    return false;
#endif
}
