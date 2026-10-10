#pragma once

class AStoryFirstPersonCharacter;

/** Opt-in (-HouseCarryTiersReview) capture pass for small / medium / large furniture handling. */
void TickHouseCarryTiersReview(AStoryFirstPersonCharacter* Pawn);

/** Opt-in (-HouseFurniturePushReview) sprint-into-furniture check. */
void TickHouseFurniturePushReview(AStoryFirstPersonCharacter* Pawn);

/** Opt-in (-HouseBreakReview) crack/break capture pass. */
void TickHouseBreakReview(AStoryFirstPersonCharacter* Pawn);

void TickHouseFlagSurvey(AStoryFirstPersonCharacter* Pawn);
void TickHouseFlagReview(AStoryFirstPersonCharacter* Pawn);

/** Opt-in (-HouseDetailSurvey) whole-house census, frame-time probe and registered-view sweep. */
void TickHouseDetailSurvey(AStoryFirstPersonCharacter* Pawn);

/** Opt-in (-GreatRoomCollisionReview) real walking routes and fitted structural collision checks. */
void TickHouseGreatRoomCollisionReview(AStoryFirstPersonCharacter* Pawn);

/** Opt-in front entrance E interaction, collision, walking and rendered captures. */
void TickHouseFrontDoubleDoorReview(AStoryFirstPersonCharacter* Pawn);

/** Opt-in (-HouseExteriorWalkReview) concrete paths, steps and real player traversal. */
void TickHouseExteriorWalkReview(AStoryFirstPersonCharacter* Pawn);

/** Opt-in (-HouseGadgetSmoke) pull station / horn / light switch / singing bass check. */
void TickHouseGadgetSmoke(AStoryFirstPersonCharacter* Pawn);

/** Opt-in (-HouseStairSurvey) upstairs stairwell census, walk-up log and fixed views. */
void TickHouseStairSurvey(AStoryFirstPersonCharacter* Pawn);

/** Opt-in (-NoodleWingSmoke) walks the new three-floor stair and basement routes. */
void TickHouseNoodleWingReview(AStoryFirstPersonCharacter* Pawn);
