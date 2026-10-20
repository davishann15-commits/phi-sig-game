# Raised west field and woodland fence

Unreal-only runtime terrain change for `/Game/Story/Maps/Chapter01_House`.

## Layout

The broad field behind the west fraternity row reaches the actual west court mound height, Z=-161.2 cm. The outer flank of the mound joins that level field. The existing front grade blends into it gradually, and its rear edge eases down into the woodland terrain between Y=4300 and 6500. A single shared height calculation places collision, grass, leaves and trees on the same surface.

The weathered post-and-three-rail wooden fence follows the ground at Y=4520, from X=-19300 to -3900. It has uneven posts, bowed/repaired boards, exposed nails, splits and a few missing lower rails. A 540 cm opening at X=-7320..-6780 provides access to the woods; the original shack path remains beyond the eastern end of the fence.

## Protected scope

The saved house map is unchanged. The perimeter concrete paths, basement entrances, court floor, hoops, AC pad, roads, fraternity buildings, shack, deep woodland hills and hollow bottoms retain their original geometry and positions. The new fence has physical collision and no shadows or indirect-light updates. No real lights were added.

## Review

Development build and native campaign captures are in `evidence/`. Terrain probes cover the plateau, mound join, woodland slope and mesh seam; fence capsule probes check six blocked spans and the clear opening. Foliage probes check the raised area. A separate actual character route checks walking across the field, through the opening and into the woods. Final screenshots are in `final/`.
