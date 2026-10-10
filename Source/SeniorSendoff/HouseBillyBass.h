#pragma once

#include "CoreMinimal.h"
#include "HouseBlackHallMesh.h"

class UHouseBlackHallSubsystem;

/** The singing bass on its plaque, in gadget space (X out of the wall, Z up, origin on the wall). */
struct FHwBassParts
{
    FHallMesh Plaque;                 // fixed: shield board, molding, nameplate, button, pivot peg
    FHallMesh Body;                   // swings out: body, head, fins, eye, hat, ribbon (vertices relative to BodyPivot)
    FHallMesh Jaw;                    // opens: lower jaw (relative to JawPivot)
    FHallMesh Tail;                   // flaps: tail fin (relative to TailPivot)
    FVector BodyPivot = FVector::ZeroVector, JawPivot = FVector::ZeroVector, TailPivot = FVector::ZeroVector;
    /** +1: the head points along +Y (left as seen from the hall). Swing / jaw signs depend on it. */
    float HeadSign = 1.f;
};

/** Registers the bass materials and textures (once) and builds the parts. */
void HwBuildBillyBass(UHouseBlackHallSubsystem* Look, FHwBassParts& Out);
/** Mouth opening 0..1 for the song at time T (seconds since it started). */
float HwBassMouth(float T);
