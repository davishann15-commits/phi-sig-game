# Autumn leaves and front-yard debris

Updated the Unreal project on 2026-10-10.

## Visible changes

- Maple, oak, and birch leaves use distinct shapes with photographed-style veins, mottling, dry edges, and autumn color variants.
- The old repeating foliage strips are replaced with individually folded leaves, petioles, and fine twigs. Leaves sit at varied angles and attach to branches.
- Tree bark and the new lawn props have dedicated materials that render correctly on shared meshes in a native game.
- The front yard has 13 sticks, 8 pinecones, 2 paper scraps, 2 discarded cans, and 2 cups. Placement is deterministic and sparse, near lawn edges and trees. The central front path is excluded.
- Pinecones have overlapping raised scales; sticks have bark, broken ends, and exposed wood. The cans are dented and the paper scraps are bent.
- The new lawn items are decoration with no blocking collision. They do not add real lights or individual physics actors.

## Screenshots

These are actual native game renders, using the existing exterior lighting and room settings.

- [Trees and house](evidence/final-views/34_nature_oak_polished.png)
- [Oak leaves](evidence/final-views/35_nature_oak_leaf_detail.png)
- [Birch leaves](evidence/final-views/36_nature_birch_leaf_detail.png)
- [Maple leaves](evidence/final-views/37_nature_maple_leaf_detail.png)
- [Sparse lawn debris](evidence/final-views/38_nature_front_yard_litter.png)
- [Pinecone on the lawn](evidence/final-views/40_nature_pinecone_close.png)

## Asset source and saved files

The two transparent leaf atlases were generated with the built-in image generation tool. The final tree crowns and fallen leaves use `assets/AutumnSingleLeaves.png`; the earlier cluster atlas is retained as source artwork. Both generated source PNGs are 1254 by 1254 pixels. Unreal imports them with alpha-preserving mip settings and a 2048 pixel size limit.

The exact generation prompts and original output filenames are preserved in [assets/manifest.json](assets/manifest.json).

New native assets are saved under `Content/SeniorSendOff/House/ExteriorNature/`:

- `T_Nature_LeafSingles.uasset`
- `T_Nature_LeafClusters.uasset`
- `M_NatureTreeFoliage.uasset`
- `M_NaturePropPlain.uasset`
- `M_NaturePropTextured.uasset`

The indoor material masters remain unchanged. The little yard meshes are authored by the existing Unreal environment builder.

## Verification

The final editor development build succeeded (`build4.log`). The final six camera views were inspected for leaf shapes, attachment, missing materials, lawn placement, and pinecone scale. The import script saved the new materials and textures without saving a map.

Nine protected files, including the house map, walkway and court code, Billy Bass, tree reaction relay, and leaf wind/audio code, retain their original hashes. See [evidence/protected-after.json](evidence/protected-after.json).

The camera sweep was capped at 30 fps for repeatable capture. It stayed at that cap in all six views; render-thread samples ranged from 5.40 to 12.44 ms. This is not an uncapped performance benchmark. The real light count remains 52 for the generated exterior. See [frame samples](evidence/final-views/frametimes.csv).

The native tree-hit review passed for all three species, using actual weapon throws and a basketball physics impact. Repeated hits produced 11 accepted events, 104 settled leaves, and a peak of 76 active leaves within the existing 216-slot limit. The house damage count stayed unchanged. See [results](evidence/final-hits/result.txt) and [verification](evidence/verification.json). The multiplayer gameplay code was preserved; multiplayer was not rerun for this visual-only pass.
