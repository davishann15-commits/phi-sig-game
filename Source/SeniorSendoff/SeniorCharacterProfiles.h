#pragma once
#include "CoreMinimal.h"

// Editable design concepts, deliberately separate from live combat attributes.
namespace SeniorProfiles
{
struct FProfile
{
    const TCHAR* Role;
    const TCHAR* Bio;
    const TCHAR* Ability;
    const TCHAR* AbilityDescription;
    int32 Stats[7];
    const TCHAR* Weapons[2];
    const TCHAR* WeaponDescriptions[2];
};
inline const TCHAR* StatNames[7] = {TEXT("HEALTH"),TEXT("STAMINA"),TEXT("SPEED"),TEXT("STRENGTH"),TEXT("DEFENSE"),TEXT("RELOAD"),TEXT("TEAMWORK")};
inline const FProfile Profiles[8] = {
 {TEXT("THE ANCHOR"),TEXT("When the front door starts shaking, this survivor plants their feet. A steady defender who buys the group time to regroup and keep the house standing."),TEXT("HOLD THE LINE"),TEXT("Concept: briefly reduce damage taken by nearby teammates."),{95,65,45,80,90,50,80},
  {TEXT("dǒulì"),TEXT("LAST CALL")},{TEXT("Throw, hit, catch. A woven hat that returns after impact or a miss. One hat; no ammo."),TEXT("Compact double-barrel shotgun. Heavy close-range impact; frequent reloads.")}},
 {TEXT("THE RUNNER"),TEXT("Always the first across the lawn and the last to get cornered. A fast survivor built to draw trouble away from teammates and find a way through campus."),TEXT("SECOND WIND"),TEXT("Concept: recover stamina and gain a short burst of speed."),{65,95,95,45,40,75,65},
  {TEXT("TRACK STAR"),TEXT("SHORTCUT")},{TEXT("Lightweight aluminum bat. Quick attacks with modest stagger."),TEXT("Light machine pistol. Mobile close-range fire with limited accuracy.")}},
 {TEXT("THE FIXER"),TEXT("A broken lock is a problem to solve, not a reason to panic. This resourceful survivor turns whatever is lying around into one more chance for the group."),TEXT("QUICK PATCH"),TEXT("Concept: repair barricades faster; restore a small amount of armor in later chapters."),{80,75,60,70,70,60,85},
  {TEXT("PIPE DREAM"),TEXT("NAILED IT")},{TEXT("Weighted pipe wrench. Reliable melee damage and armor pressure."),TEXT("Modified nail launcher. Accurate single shots with a small magazine.")}},
 {TEXT("THE LOOKOUT"),TEXT("Notices the open window before anyone hears the footsteps. A careful survivor who helps the team spot ambushes and choose when to fight."),TEXT("HEADS UP"),TEXT("Concept: briefly mark nearby threats, including hiding attackers."),{70,75,75,45,50,85,90},
  {TEXT("NIGHT WATCH"),TEXT("CAMPUS SCOUT")},{TEXT("Long flashlight baton. Fast defensive strikes with short reach."),TEXT("Light carbine. Controlled medium-range shots; weak at crowd clearing.")}},
 {TEXT("THE BRUISER"),TEXT("Prefers to meet trouble head-on. A hard-hitting survivor who can clear a crowded doorway, but needs teammates to cover the slower recovery between swings."),TEXT("MAKE ROOM"),TEXT("Concept: stagger enemies directly ahead with a powerful shove."),{100,60,40,100,80,35,55},
  {TEXT("DEMOLITION DAY"),TEXT("DOORBREACHER")},{TEXT("Short-handled sledgehammer. Huge stagger with slow recovery."),TEXT("Heavy pump shotgun. Wide close-range damage and a slow reload.")}},
 {TEXT("THE SUPPORT"),TEXT("Keeps counting heads when everyone else is counting enemies. A dependable teammate whose best moments happen when the rest of the group needs help."),TEXT("STAY WITH ME"),TEXT("Concept: revive downed teammates faster and grant brief protection afterward."),{80,85,65,50,65,65,100},
  {TEXT("LIFELINE"),TEXT("BACKUP PLAN")},{TEXT("Rescue hatchet. Balanced strikes that reward careful timing."),TEXT("Compact service pistol. Manageable recoil and quick reloads.")}},
 {TEXT("THE TRICKSTER"),TEXT("Never takes the obvious route when a better distraction will do. A slippery survivor who creates openings for friends and makes enemies chase the wrong target."),TEXT("OVER HERE"),TEXT("Concept: place a short-lived sound decoy that attracts ordinary attackers."),{70,90,85,50,40,80,75},
  {TEXT("PARTY CRASHER"),TEXT("BAD INFLUENCE")},{TEXT("Weighted pool cue. Long melee reach with a narrow hit area."),TEXT("Burst-fire pistol. Strong short bursts with demanding ammo control.")}},
 {TEXT("THE CLOSER"),TEXT("Saves their nerve for the moment that counts. A focused survivor who excels at pressuring dangerous seniors while friends keep the smaller threats occupied."),TEXT("PICK YOUR TARGET"),TEXT("Concept: mark one dangerous enemy so the group deals extra damage to it."),{85,65,55,75,60,90,80},
  {TEXT("SENIORITY CHECK"),TEXT("FINAL WORD")},{TEXT("Heavy crowbar. Deliberate strikes with strong single-target damage."),TEXT("Precision lever-action rifle. Powerful aimed shots and limited capacity.")}}
};
inline const FProfile& Get(int32 Index) { return Profiles[FMath::Clamp(Index,0,7)]; }
}
