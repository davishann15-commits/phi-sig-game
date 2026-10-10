# Great room measured reference layout

Built from Stewart's IMG_2171–IMG_2175 photos. Main sectional: 14 × 9 ft overall footprint, giving 7.68096 × 4.93776 metres in the house's 1.8 horizontal scale. Opposite sofas: 6.2 ft long, giving 3.401568 metres in game. The house has normal vertical scaling: seats stay near normal height, with upper upholstery fitted to the taller photo silhouettes (about 1.17 metre backs). Other dimensions are photo estimates.

Three seating groups: the near sectional faces the column side and matching sofa, six cognac/olive chairs face the central table, and a dark U of sofas faces the far fireplace. Porch consoles, lamps, trophies, pillows, two round tables, low side tables, tall stool and three rugs complete the setup. The surviving native heater is tucked beneath its wall console to clear the porch strip.

Run build_full_room.py in Blender background mode to regenerate the FBX files. furniture_geometry.py contains the upholstered piece models. Baseline contains preserved original exports, including the approved porch bench correction. Content/Python/apply_full_great_room.py imports the results and updates Chapter01_House only.

Coordinate conversion: Blender metres [x,y,z] to Unreal centimetres [100x,-100y,100z]. Visual meshes use NoCollision; the hidden COL_GR2_Furniture provides blocking. Original porch benches and wall composites are retained. ReferenceTextures keeps the original generated rug/leather art; imported textures are 1024 square for mipmaps.
