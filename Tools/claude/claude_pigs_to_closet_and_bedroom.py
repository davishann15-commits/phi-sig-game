"""Move the pig pen into the basement library's mechanical closet and turn the old
second-floor pigsty into Boywick's bedroom (photo-matched layout).

Run headless from the project folder:
  UnrealEditor-Cmd SeniorSendoff.uproject -run=pythonscript -script=Tools/claude/claude_pigs_to_closet_and_bedroom.py

Everything is data-driven and idempotent: re-running moves the same actors to the
same places and recreates the bedroom actors (old BW_ actors are removed first).
Only the listed labels change; every other actor is verified untouched before save.
"""
import json, os, unreal

MAP = '/Game/Story/Maps/Chapter01_House'
A = unreal.EditorAssetLibrary
ELL = unreal.EditorLevelLibrary
LOG = lambda m: unreal.log('CLAUDE_MAP ' + m)
REPORT = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeDump', 'map_edit_report.json')

# ---------------------------------------------------------------- geometry
FLOOR_B = -329.06          # basement library / closet floor (feet height)
FLOOR_U = 302.3            # second floor bedroom floor
CEIL_U = 566.1
XW, XE, YS, YN = -2972.0, -2355.0, -1371.8, -587.0     # bedroom interior (617 x 785, matched to Room 204)
MAT = lambda n: '/Game/SeniorSendOff/House/Materials/' + n
FOLDER_BED = 'Second floor/Boywick bedroom'
FOLDER_PEN = 'Basement/Pigsty closet'

def V(x, y, z): return unreal.Vector(x, y, z)
def R(yaw, pitch=0.0, roll=0.0): return unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll)
def asset(path):
    if not A.does_asset_exist(path): raise RuntimeError('Missing asset ' + path)
    obj = A.load_asset(path)
    if not obj: raise RuntimeError('Could not load ' + path)
    return obj
def maybe(path):
    return A.load_asset(path) if A.does_asset_exist(path) else None

# ---------------------------------------------------------------- pig pen: where everything goes
# (label) -> (location, yaw)
PEN_MOVES = {
    'Interactive_PIG_Mattress__001':            (V(-2815.0, 1000.0, FLOOR_B + 10.5), 90.0),
    'Interactive_PIG_Blanket__001':             (V(-2812.5, 951.0, FLOOR_B + 23.46), 90.0),
    'Interactive_PIG_CheetosBag__001':          (V(-2600.0, 905.0, FLOOR_B + 14.24), 66.0),
    'Interactive_PIG_Crumbs__001':              (V(-2631.0, 870.0, FLOOR_B + 0.66), 113.0),
    'Interactive_PIG_WaterTrough__001':         (V(-2572.0, 1040.0, FLOOR_B + 17.1), 90.0),
    'Interactive_PIG_FeedBucket__001':          (V(-2575.0, 1105.0, FLOOR_B + 20.79), 102.0),
    'Interactive_PIG_BrokenBoards__001':        (V(-2870.0, 1105.0, FLOOR_B + 4.4), 82.0),
    'Interactive_PIG_BulkheadLamp__001':        (V(-2720.0, 1119.0, -140.0), 180.0),
    'Interactive_PIG_KeepShutSign__001':        (V(-2640.0, 1120.0, -189.0), 0.0),
    'Interactive_PIG_OutsideDoorPlaque__walldecor_1': (V(-2566.0, 770.0, FLOOR_B + 137.2), 0.0),
    'PIG_EdibleCheetos':                        (V(-2630.0, 870.0, FLOOR_B), 0.0),
    'PIG_Pepper':                               (V(-2700.0, 940.0, FLOOR_B + 29.76), 20.0),
    'PIG_Coal':                                 (V(-2780.0, 1060.0, FLOOR_B + 39.36), -120.0),
    'PIG_Straw_00':                             (V(-2860.0, 850.0, FLOOR_B + 0.2), 45.0),
    'PIG_Straw_01':                             (V(-2640.0, 1000.0, FLOOR_B + 0.2), 10.0),
    'PIG_Straw_02':                             (V(-2700.0, 1090.0, FLOOR_B + 0.2), -20.0),
    'PIG_Straw_03':                             (V(-2590.0, 960.0, FLOOR_B + 0.2), 80.0),
    'PIG_Straw_04':                             (V(-2850.0, 1115.0, FLOOR_B + 0.2), 115.0),
    'PIG_Straw_05':                             (V(-2760.0, 870.0, FLOOR_B + 0.2), 67.0),
    'PIG_WallScratch_West':                     (V(-2893.0, 1000.0, FLOOR_B + 15.7), 90.0),
    'PIG_WallScratch_North':                    (V(-2800.0, 1125.0, FLOOR_B + 16.7), 0.0),
    'PIG_DampCornerFill':                       (V(-2880.0, 1110.0, -153.0), 0.0),
    'PIG_FloorBounce':                          (V(-2700.0, 980.0, -191.0), 0.0),
}
PIG_PROPS = {
    'PIG_Pepper': dict(bed_location=V(-2815.0, 967.0, FLOOR_B + 21.0)),
    'PIG_Coal':   dict(bed_location=V(-2815.0, 1035.0, FLOOR_B + 21.0)),
}
PIG_COMMON = dict(pigsty_door_id=unreal.Name('BasementLibrary_Mechanical'),
                  pigsty_minimum=V(-2900.0, 795.0, -345.0), pigsty_maximum=V(-2540.0, 1132.0, -250.0), bed_yaw=90.0)
PEN_STAYS = {'PIG_ExpandedRoom', 'PIG_ExpandedRoomCollision'}   # the vent network and the room shell stay upstairs

# ---------------------------------------------------------------- bedroom build list
CUBE, CYL, SPHERE = '/Engine/BasicShapes/Cube', '/Engine/BasicShapes/Cylinder', '/Engine/BasicShapes/Sphere'
BR = '/Game/SeniorSendOff/House/AcrossPosterBedroom/'
LIB = '/Game/SeniorSendOff/House/BasementLibrary/'
KIT = '/Game/SeniorSendOff/House/ReferenceKitchen/'
HI = '/Game/SeniorSendOff/House/Interactivity/'
BWDIR = '/Game/SeniorSendOff/House/BoywickRoom'
ZF, ZC = FLOOR_U, CEIL_U
CF = -2425.0                       # front face of the built-in closets on the door wall
DOOR_Y0, DOOR_Y1 = -1342.0, -1148.0  # hallway door frame (keep clear)
WIN_Y0, WIN_Y1, WIN_Z0, WIN_Z1 = -995.0, -885.0, ZF + 80.0, ZF + 235.0

# Flat colours as material instances of the house's plain master (created once, reused).
COLORS = {
    'wall': ((.40, .38, .34), .85, 0), 'ceil': ((.74, .73, .70), .9, 0), 'trim': ((.82, .81, .78), .5, 0), 'base': ((.03, .03, .03), .6, 0),
    'steel': ((.015, .015, .016), .45, .6), 'mattress': ((.82, .82, .80), .9, 0), 'mattside': ((.55, .55, .55), .9, 0),
    'orange': ((.80, .32, .05), .9, 0), 'greypillow': ((.42, .43, .45), .95, 0), 'comforter': ((.06, .065, .075), .95, 0),
    'beige': ((.52, .45, .36), .95, 0), 'couch': ((.075, .075, .08), .95, 0), 'couchdark': ((.035, .035, .04), .95, 0),
    'teal': ((.26, .40, .34), .9, 0), 'tealpat': ((.62, .68, .60), .9, 0), 'walnut': ((.14, .07, .035), .6, 0), 'walnut2': ((.10, .05, .025), .6, 0),
    'desk': ((.76, .74, .70), .6, 0), 'fridge': ((.02, .02, .022), .3, .1), 'fabricdrawer': ((.055, .06, .065), .95, 0),
    'tv': ((.01, .01, .012), .2, .1), 'screen': ((.02, .03, .07), .3, 0, (.10, .16, .45)), 'console': ((.85, .85, .86), .4, 0),
    'rug': ((.66, .64, .60), .95, 0), 'rugstripe': ((.22, .24, .28), .95, 0), 'cup': ((.88, .88, .86), .6, 0), 'red': ((.55, .05, .04), .6, 0),
    'paper': ((.82, .82, .78), .9, 0), 'jar': ((.55, .32, .10), .3, 0), 'purple': ((.30, .14, .45), .4, 0), 'black': ((.02, .02, .02), .5, 0),
    'brown': ((.24, .13, .06), .8, 0), 'cardboard': ((.45, .33, .20), .9, 0), 'basket': ((.86, .86, .85), .6, 0), 'laptop': ((.45, .46, .48), .3, .8),
    'blue': ((.05, .15, .55), .8, 0), 'pink': ((.80, .50, .48), .9, 0), 'yellow': ((.85, .65, .08), .9, 0), 'white': ((.85, .85, .84), .9, 0),
    'grey': ((.35, .36, .38), .9, 0), 'green': ((.08, .30, .12), .6, 0), 'greenb': ((.12, .45, .18), .4, 0), 'bag': ((.85, .84, .82), .7, 0),
    'cereal': ((.70, .12, .10), .7, 0), 'protein': ((.85, .85, .80), .5, 0), 'lightglass': ((.95, .93, .88), .2, 0, (1.0, .92, .78)),
}
_mats = {}
def mat(key):
    if key in _mats: return _mats[key]
    if key.startswith('/'):
        _mats[key] = asset(key); return _mats[key]
    name = 'MI_BW_' + key
    path = BWDIR + '/' + name
    if A.does_asset_exist(path): m = A.load_asset(path)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, BWDIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    E = unreal.MaterialEditingLibrary
    E.set_material_instance_parent(m, asset('/Game/SeniorSendOff/House/Materials/M_House_Plain'))
    spec = COLORS[key]
    E.set_material_instance_vector_parameter_value(m, 'Tint', unreal.LinearColor(*spec[0], 1.0))
    E.set_material_instance_scalar_parameter_value(m, 'Roughness', spec[1])
    E.set_material_instance_scalar_parameter_value(m, 'Metallic', spec[2])
    em = spec[3] if len(spec) > 3 else (0, 0, 0)
    E.set_material_instance_vector_parameter_value(m, 'Emission', unreal.LinearColor(*em, 1.0))
    A.save_loaded_asset(m)
    _mats[key] = m
    return m

OUT = []
def B(label, x0, x1, y0, y1, z0, z1, m, col='NoCollision', yaw=0.0, pitch=0.0, roll=0.0):
    """Box from world-space min/max corners (cm)."""
    OUT.append(dict(kind='sm', label=label, mesh=CUBE, loc=V((x0 + x1) * .5, (y0 + y1) * .5, (z0 + z1) * .5), yaw=yaw, pitch=pitch, roll=roll,
                    scale=(abs(x1 - x0) / 100.0, abs(y1 - y0) / 100.0, abs(z1 - z0) / 100.0), materials=[m], collision=col))
def C(label, x, y, z0, z1, r, m, col='NoCollision', pitch=0.0, roll=0.0, yaw=0.0):
    """Upright cylinder (or tilted with pitch/roll) from z0..z1, radius r."""
    OUT.append(dict(kind='sm', label=label, mesh=CYL, loc=V(x, y, (z0 + z1) * .5), yaw=yaw, pitch=pitch, roll=roll,
                    scale=(r / 50.0, r / 50.0, abs(z1 - z0) / 100.0), materials=[m], collision=col))
def S(label, x, y, z, sx, sy, sz, m, yaw=0.0, pitch=0.0, roll=0.0, col='NoCollision'):
    OUT.append(dict(kind='sm', label=label, mesh=SPHERE, loc=V(x, y, z), yaw=yaw, pitch=pitch, roll=roll, scale=(sx / 100.0, sy / 100.0, sz / 100.0), materials=[m], collision=col))
def P(label, mesh, x, y, z, yaw, name, mass, scale=None, movable=True, wall=False, materials=None):
    OUT.append(dict(kind='prop', label=label, mesh=mesh, loc=V(x, y, z), yaw=yaw, prop_id=label, name=name, mass=mass, scale=scale, movable=movable, wall=wall, materials=materials))
def M(label, mesh, x, y, z, yaw=0.0, scale=(1, 1, 1), materials=None, col='NoCollision'):
    OUT.append(dict(kind='sm', label=label, mesh=mesh, loc=V(x, y, z), yaw=yaw, scale=scale, materials=materials or [], collision=col))
def L(label, x, y, z, intensity, radius, color):
    OUT.append(dict(kind='light', label=label, loc=V(x, y, z), intensity=intensity, radius=radius, color=color))

def bedroom_actors():
    OUT.clear()
    T = 12.0
    # ---- shell: floor, walls, ceiling (replaces the old pigsty shell so the room can match Room 204's size)
    M('BW_Floor', BR + 'SM_Bedroom_WoodFloor', 0.0, -993.0 - 856.58, -2.7)
    B('BW_FloorCollision', XW - T, XE + T, YS - T, YN + T, ZF - 12, ZF, mat('base'), col='BlockAll')
    OUT[-1]['hidden'] = True
    B('BW_Ceiling', XW - T, XE + T, YS - T, YN + T, ZC, ZC + 10, mat('ceil'), col='BlockAll')
    B('BW_Wall_W', XW - T, XW, YS - T, YN + T, ZF, ZC, mat('wall'), col='BlockAll')
    B('BW_Wall_S', XW - T, XE + T, YS - T, YS, ZF, ZC, mat('wall'), col='BlockAll')
    B('BW_Wall_N', XW - T, XE + T, YN, YN + T, ZF, ZC, mat('wall'), col='BlockAll')
    B('BW_Wall_E_S', XE, XE + T, YS - T, DOOR_Y0, ZF, ZC, mat('wall'), col='BlockAll')
    B('BW_Wall_E_N', XE, XE + T, DOOR_Y1, YN + T, ZF, ZC, mat('wall'), col='BlockAll')
    B('BW_Wall_E_Header', XE, XE + T, DOOR_Y0, DOOR_Y1, ZF + 216, ZC, mat('wall'), col='BlockAll')
    # black baseboards
    B('BW_Base_W', XW, XW + 1.5, YS, YN, ZF, ZF + 9, mat('base'))
    B('BW_Base_S', XW, XE, YS, YS + 1.5, ZF, ZF + 9, mat('base'))
    B('BW_Base_N', XW, CF, YN - 1.5, YN, ZF, ZF + 9, mat('base'))
    B('BW_Base_E', CF - 1.5, CF, -880, YN, ZF, ZF + 9, mat('base'))
    # ---- built-in closets on the door wall: boxes closet nearest the door, drawer closet next, comic poster between
    cy = [(-1145.0, -1055.0, 'Boxes'), (-990.0, -900.0, 'Drawers')]
    for i, (a, b) in enumerate([(-1155.0, -1145.0), (-1055.0, -990.0), (-900.0, -880.0)]):
        B('BW_ClosetPier_%d' % i, CF, XE, a, b, ZF, ZC, mat('wall'), col='BlockAll')
    B('BW_ClosetHeader', CF, XE, -1155.0, -880.0, ZF + 205, ZC, mat('wall'), col='BlockAll')
    B('BW_ClosetEndCap', CF, XE, -880.0, -878.0, ZF, ZC, mat('wall'))
    for y0, y1, tag in cy:
        B('BW_Closet%s_JambL' % tag, CF - 2, CF + 1, y0 - 7, y0, ZF, ZF + 212, mat('trim'))
        B('BW_Closet%s_JambR' % tag, CF - 2, CF + 1, y1, y1 + 7, ZF, ZF + 212, mat('trim'))
        B('BW_Closet%s_Head' % tag, CF - 2, CF + 1, y0 - 7, y1 + 7, ZF + 205, ZF + 212, mat('trim'))
        B('BW_Closet%s_Shelf' % tag, CF + 5, XE, y0, y1, ZF + 180, ZF + 182, mat('trim'))
        C('BW_Closet%s_Rod' % tag, XE - 32, (y0 + y1) * .5, ZF + 172 - (y1 - y0) * .5, ZF + 172 + (y1 - y0) * .5, 1.2, mat('steel'), roll=90.0)
        B('BW_Closet%s_Base' % tag, CF + 1, XE, y0, y1, ZF, ZF + 8, mat('base'))
    shirts = ['blue', 'grey', 'white', 'pink', 'orange', 'grey', 'black', 'orange', 'white', 'yellow', 'orange', 'brown', 'blue', 'pink']
    y = -988.0
    for i, k in enumerate(shirts):
        B('BW_ShirtA_%02d' % i, XE - 56, XE - 8, y, y + 2.5, ZF + 95 + (i % 3) * 4, ZF + 170, mat(k)); y += 6.0
    for i, k in enumerate(['white', 'grey', 'white', 'black', 'black', 'white', 'grey']):
        B('BW_ShirtB_%02d' % i, XE - 56, XE - 8, -1100 + i * 7.0, -1097.5 + i * 7.0, ZF + 85 if k == 'black' else ZF + 100, ZF + 170, mat(k))
    # fabric drawer unit in the drawer closet, boxes + shoes + a broom in the other
    B('BW_DrawerFrame', XE - 42, XE - 4, -985, -905, ZF, ZF + 92, mat('black'))
    for r in range(4):
        for c, (a, b) in enumerate([(-984, -946), (-944, -906)]):
            B('BW_Drawer_%d%d' % (r, c), XE - 44, XE - 42, a, b, ZF + 3 + r * 22.5, ZF + 23.5 + r * 22.5, mat('brown'))
            B('BW_DrawerPull_%d%d' % (r, c), XE - 45, XE - 44, (a + b) * .5 - 6, (a + b) * .5 + 6, ZF + 15 + r * 22.5, ZF + 17 + r * 22.5, mat('black'))
    B('BW_Box_1', XE - 40, XE - 4, -1145, -1100, ZF, ZF + 60, mat('cardboard'))
    B('BW_Box_2', XE - 38, XE - 6, -1143, -1105, ZF + 60, ZF + 115, mat('cardboard'))
    B('BW_Box_3', XE - 30, XE - 4, -1100, -1075, ZF, ZF + 80, mat('white'))
    C('BW_Broom', XE - 30, -1080, ZF, ZF + 140, 1.2, mat('grey'), pitch=0.0, roll=-22.0)
    for i in range(3):
        B('BW_ClosetShoe_%d' % i, CF + 8 + i * 12, CF + 38 + i * 12, -1092 + i * 9, -1082 + i * 9, ZF, ZF + 10, mat(['white', 'brown', 'grey'][i]), yaw=20.0 * i)
    # ---- loft bed on the north wall (full over full, black steel), futon/bed underneath
    lx0, lx1, ly1 = -2810.0, -2600.0, YN - 3.0
    ly0 = ly1 - 148.0
    for i, (x, yy) in enumerate(((lx0, ly0), (lx1, ly0), (lx0, ly1), (lx1, ly1))):
        B('BW_LoftPost_%d' % i, x - 2.5, x + 2.5, yy - 2.5, yy + 2.5, ZF, ZF + 192, mat('steel'), col='BlockAll')
    for z, tag in ((ZF + 148, 'Deck'), (ZF + 28, 'Low')):
        B('BW_Loft%sRail_S' % tag, lx0, lx1, ly0 - 2.5, ly0 + 2.5, z - 3, z + 3, mat('steel'), col='BlockAll')
        B('BW_Loft%sRail_N' % tag, lx0, lx1, ly1 - 2.5, ly1 + 2.5, z - 3, z + 3, mat('steel'), col='BlockAll')
        B('BW_Loft%sRail_W' % tag, lx0 - 2.5, lx0 + 2.5, ly0, ly1, z - 3, z + 3, mat('steel'), col='BlockAll')
        B('BW_Loft%sRail_E' % tag, lx1 - 2.5, lx1 + 2.5, ly0, ly1, z - 3, z + 3, mat('steel'), col='BlockAll')
    for i in range(7):
        x = lx0 + 15 + i * 30
        B('BW_LoftSlat_%d' % i, x - 3, x + 3, ly0, ly1, ZF + 146, ZF + 149, mat('steel'))
    B('BW_LoftMattress', lx0 + 2, lx1 - 2, ly0 + 3, ly1 - 3, ZF + 150, ZF + 176, mat('mattress'), col='BlockAll')
    B('BW_LoftMattressBand', lx0 + 1.5, lx1 - 1.5, ly0 + 2.5, ly1 - 2.5, ZF + 152, ZF + 163, mat('mattside'))
    for i in range(6):   # quilt ridges
        x = lx0 + 20 + i * 34
        B('BW_LoftQuilt_%d' % i, x - 1, x + 1, ly0 + 6, ly1 - 6, ZF + 176, ZF + 177, mat('mattside'))
    S('BW_LoftPillow', lx0 + 30, (ly0 + ly1) * .5 + 15, ZF + 184, 40, 110, 16, mat('greypillow'))
    S('BW_LoftTowel', -2700, (ly0 + ly1) * .5 + 10, ZF + 186, 55, 45, 24, mat('orange'), yaw=20.0)
    C('BW_LoftCup', -2668, ly1 - 25, ZF + 176, ZF + 188, 4.5, mat('cup'))
    # front guard rails and the ladder end
    for z in (ZF + 172, ZF + 190):
        B('BW_LoftGuard_%d' % int(z), lx0, lx1 - 45, ly0 - 2, ly0 + 2, z - 2, z + 2, mat('steel'))
    B('BW_LoftGuardPost', lx1 - 47, lx1 - 43, ly0 - 2, ly0 + 2, ZF + 148, ZF + 192, mat('steel'))
    for i, z in enumerate((ZF + 60, ZF + 90, ZF + 120, ZF + 175)):
        B('BW_LoftEndBar_%d' % i, lx1 - 2, lx1 + 2, ly0, ly1, z - 2, z + 2, mat('steel'))
    # lower bed
    B('BW_LowMattress', lx0 + 2, lx1 - 2, ly0 + 3, ly1 - 3, ZF + 30, ZF + 52, mat('greypillow'), col='BlockAll')
    B('BW_LowComforter', lx0 + 4, lx1 - 30, ly0 - 4, ly1 - 4, ZF + 50, ZF + 58, mat('comforter'), yaw=1.5)
    S('BW_LowComforterLump', -2745, ly0 + 70, ZF + 60, 90, 70, 22, mat('comforter'), yaw=12.0)
    S('BW_LowPillow', lx0 + 28, (ly0 + ly1) * .5, ZF + 62, 38, 70, 16, mat('couchdark'))
    # beige blanket draped over the loft's front rail, laundry bag hanging at the desk end
    B('BW_DrapeBlanket', -2690, -2640, ly0 - 4, ly0 - 1, ZF + 70, ZF + 150, mat('beige'), yaw=4.0)
    B('BW_DrapeBlanketTop', -2690, -2640, ly0 - 4, ly0 + 18, ZF + 149, ZF + 152, mat('beige'))
    S('BW_LaundryBag', lx0 - 6, ly0 - 8, ZF + 115, 22, 22, 55, mat('bag'))
    # floating rope shelves above the loft and the empty TV wall mount
    for i, x in enumerate((-2740.0, -2665.0)):
        B('BW_FloatShelf_%d' % i, x - 25, x + 25, YN - 16, YN, ZF + 205, ZF + 208, mat('walnut2'))
        for dx in (-22, 22):
            B('BW_FloatRope_%d_%d' % (i, dx), x + dx - .4, x + dx + .4, YN - 8, YN - 7, ZF + 208, ZF + 236, mat('black'))
    B('BW_TVMount', -2530 - 38, -2530 + 38, YN - 3, YN, ZF + 190, ZF + 222, mat('black'))
    B('BW_TVMountCut', -2530 - 30, -2530 + 30, YN - 3.2, YN - 2.8, ZF + 196, ZF + 216, mat('wall'))
    # ---- TV dresser by the closets: grey fabric drawers, TV and a white console
    dx0, dx1, dy1 = -2525.0, -2425.0, YN - 1.0
    dy0 = dy1 - 32.0
    B('BW_DresserFrame', dx0, dx1, dy0, dy1, ZF, ZF + 72, mat('black'), col='BlockAll')
    for i in range(3):
        a = dx0 + 3 + i * 32.0
        B('BW_DresserTop_%d' % i, a, a + 30, dy0 - 1.5, dy0, ZF + 40, ZF + 68, mat('fabricdrawer'))
        B('BW_DresserTopPull_%d' % i, a + 9, a + 21, dy0 - 2.5, dy0 - 1.5, ZF + 60, ZF + 62, mat('steel'))
    for i in range(2):
        a = dx0 + 3 + i * 48.0
        B('BW_DresserLow_%d' % i, a, a + 46, dy0 - 1.5, dy0, ZF + 4, ZF + 37, mat('fabricdrawer'))
        B('BW_DresserLowPull_%d' % i, a + 17, a + 29, dy0 - 2.5, dy0 - 1.5, ZF + 29, ZF + 31, mat('steel'))
    tvx = (dx0 + dx1) * .5 - 6
    B('BW_TVStand', tvx - 18, tvx + 18, dy0 + 6, dy1 - 6, ZF + 72, ZF + 74, mat('tv'))
    B('BW_TV', tvx - 48, tvx + 48, dy0 + 13, dy0 + 17, ZF + 78, ZF + 134, mat('tv'), col='BlockAll')
    B('BW_TVScreen', tvx - 46, tvx + 46, dy0 + 12.6, dy0 + 13, ZF + 80, ZF + 132, mat('screen'))
    B('BW_Console', dx1 - 14, dx1 - 4, dy0 + 4, dy0 + 30, ZF + 72, ZF + 111, mat('console'))
    B('BW_ConsoleBlack', dx1 - 13, dx1 - 5, dy0 + 3, dy0 + 31, ZF + 85, ZF + 98, mat('black'))
    # trash can between the dresser and the loft
    B('BW_Trash', -2580, -2540, YN - 38, YN - 6, ZF, ZF + 58, mat('basket'), col='BlockAll')
    S('BW_TrashBag', -2560, YN - 22, ZF + 62, 42, 36, 22, mat('paper'))
    S('BW_TrashBag2', -2552, YN - 18, ZF + 68, 22, 20, 18, mat('cardboard'))
    # ---- far (west) wall: desk + green chair, wire rack over the mini fridge, window with blinds
    wy0, wy1 = -720.0, -620.0
    B('BW_DeskTop', XW, XW + 52, wy0, wy1, ZF + 73, ZF + 76, mat('desk'), col='BlockAll')
    B('BW_DeskApron', XW + 2, XW + 50, wy0 + 3, wy1 - 3, ZF + 64, ZF + 73, mat('desk'))
    B('BW_DeskDrawerFace', XW + 50, XW + 51, wy0 + 30, wy1 - 30, ZF + 65, ZF + 72, mat('desk'))
    for i, (ox, yy, pr) in enumerate(((XW + 6, wy0 + 5, 6.0), (XW + 46, wy0 + 5, -6.0), (XW + 6, wy1 - 5, 6.0), (XW + 46, wy1 - 5, -6.0))):
        B('BW_DeskLeg_%d' % i, ox - 2, ox + 2, yy - 2, yy + 2, ZF, ZF + 66, mat('desk'), pitch=pr)
    C('BW_LampBase', XW + 16, wy1 - 18, ZF + 76, ZF + 78, 7, mat('white'))
    B('BW_LampArm', XW + 15, XW + 17, wy1 - 19, wy1 - 17, ZF + 78, ZF + 118, mat('white'), roll=10.0)
    B('BW_LampHead', XW + 12, XW + 40, wy1 - 22, wy1 - 14, ZF + 116, ZF + 119, mat('white'))
    C('BW_AirPurifier', XW + 18, wy0 + 18, ZF + 76, ZF + 104, 9, mat('white'))
    for i, (yy, k) in enumerate(((wy0 + 45, 'greenb'), (wy0 + 52, 'greenb'), (wy0 + 60, 'white'))):
        C('BW_DeskBottle_%d' % i, XW + 30, yy, ZF + 76, ZF + 96, 3, mat(k))
    B('BW_DeskTissues', XW + 10, XW + 32, wy0 + 66, wy0 + 78, ZF + 76, ZF + 86, mat('white'))
    B('BW_DeskPhotoFrame', XW + 6, XW + 8, wy1 - 45, wy1 - 32, ZF + 76, ZF + 88, mat('white'))
    P('BW_DeskChair', KIT + 'SM_Kitchen_GreenSpindleChair', XW + 75, (wy0 + wy1) * .5, ZF, 180.0, 'Green chair', 7.0)
    B('BW_ChairJacket', XW + 92, XW + 98, (wy0 + wy1) * .5 - 20, (wy0 + wy1) * .5 + 20, ZF + 55, ZF + 95, mat('black'))
    # wire rack with the mini fridge at the bottom
    ry0, ry1, rx1 = -860.0, -768.0, XW + 40
    for i, (x, yy) in enumerate(((XW + 2, ry0), (rx1, ry0), (XW + 2, ry1), (rx1, ry1))):
        C('BW_RackPost_%d' % i, x, yy, ZF, ZF + 182, 1.3, mat('steel'))
    for i, z in enumerate((ZF + 92, ZF + 122, ZF + 152, ZF + 180)):
        B('BW_RackShelf_%d' % i, XW + 1, rx1 + 1, ry0, ry1, z, z + 1.2, mat('steel'), col='BlockAll')
    B('BW_Fridge', XW + 3, XW + 46, ry0 + 4, ry1 - 4, ZF, ZF + 86, mat('fridge'), col='BlockAll')
    B('BW_FridgeSeam', XW + 46, XW + 46.5, ry0 + 4, ry1 - 4, ZF + 70, ZF + 70.6, mat('steel'))
    B('BW_FridgeHandle', XW + 46.5, XW + 48, ry1 - 10, ry1 - 7, ZF + 40, ZF + 68, mat('steel'))
    food = [  # (shelf, y0, y1, height, colour) on each wire shelf
        (0, -855, -835, 26, 'protein'), (0, -832, -822, 18, 'purple'), (0, -818, -800, 22, 'white'), (0, -796, -784, 14, 'blue'),
        (1, -856, -838, 20, 'blue'), (1, -834, -812, 12, 'purple'), (1, -808, -790, 16, 'yellow'), (1, -786, -774, 20, 'paper'),
        (2, -855, -838, 28, 'cereal'), (2, -835, -822, 22, 'cereal'), (2, -818, -808, 15, 'jar'), (2, -804, -790, 24, 'brown'),
        (3, -850, -842, 12, 'red'), (3, -836, -824, 9, 'white'), (3, -812, -806, 14, 'jar')]
    zs = (ZF + 93.2, ZF + 123.2, ZF + 153.2, ZF + 181.2)
    for i, (sh, a, b, hh, k) in enumerate(food):
        B('BW_Food_%02d' % i, XW + 8, XW + 30, a, b, zs[sh], zs[sh] + hh, mat(k))
    # window: white casing, sill and closed blinds
    B('BW_WinGlass', XW, XW + .5, WIN_Y0, WIN_Y1, WIN_Z0, WIN_Z1, mat('lightglass'))
    M('BW_WinBlinds', KIT + 'SM_Kitchen_WindowBlinds', XW + 5, (WIN_Y0 + WIN_Y1) * .5, WIN_Z0 - 2, yaw=90.0, scale=((WIN_Y1 - WIN_Y0) / 131.4, 1.0, (WIN_Z1 - WIN_Z0) / 152.75))
    B('BW_WinCase_L', XW, XW + 3, WIN_Y0 - 9, WIN_Y0, WIN_Z0 - 6, WIN_Z1 + 9, mat('trim'))
    B('BW_WinCase_R', XW, XW + 3, WIN_Y1, WIN_Y1 + 9, WIN_Z0 - 6, WIN_Z1 + 9, mat('trim'))
    B('BW_WinCase_T', XW, XW + 3, WIN_Y0 - 9, WIN_Y1 + 9, WIN_Z1, WIN_Z1 + 9, mat('trim'))
    B('BW_WinSill', XW, XW + 9, WIN_Y0 - 13, WIN_Y1 + 13, WIN_Z0 - 8, WIN_Z0 - 3, mat('trim'))
    B('BW_WinApron', XW, XW + 2.5, WIN_Y0 - 9, WIN_Y1 + 9, WIN_Z0 - 18, WIN_Z0 - 8, mat('trim'))
    # ---- south wall: dark grey futon couch with teal pillows, clothes and a laptop; laundry basket
    sx0, sx1, sy0 = -2955.0, -2765.0, YS
    sy1 = sy0 + 88.0
    for i, x in enumerate((sx0 + 8, sx1 - 8)):
        for j, yy in enumerate((sy0 + 10, sy1 - 10)):
            C('BW_CouchLeg_%d%d' % (i, j), x, yy, ZF, ZF + 8, 2.0, mat('black'))
    B('BW_CouchBase', sx0, sx1, sy0 + 2, sy1, ZF + 8, ZF + 30, mat('couch'), col='BlockAll')
    for i in range(2):
        a = sx0 + 2 + i * 93.0
        B('BW_CouchSeat_%d' % i, a, a + 92, sy0 + 22, sy1, ZF + 30, ZF + 44, mat('couch'))
        for k in range(3):
            S('BW_CouchSeatTuft_%d%d' % (i, k), a + 16 + k * 30, sy0 + 55, ZF + 44.3, 2.4, 2.4, 1.2, mat('couchdark'))
    B('BW_CouchBack', sx0, sx1, sy0 + 2, sy0 + 24, ZF + 30, ZF + 86, mat('couch'), col='BlockAll', roll=-8.0)
    for i in range(6):
        x = sx0 + 16 + i * 32
        B('BW_CouchBackSeam_%d' % i, x - .5, x + .5, sy0 + 24, sy0 + 25, ZF + 44, ZF + 84, mat('couchdark'), roll=-8.0)
    S('BW_Pillow_1', sx0 + 32, sy0 + 36, ZF + 66, 46, 15, 44, mat('teal'), yaw=8.0, roll=-14.0)
    S('BW_Pillow_1Pat', sx0 + 32, sy0 + 43, ZF + 66, 30, 4, 28, mat('tealpat'), yaw=8.0, roll=-14.0)
    S('BW_Pillow_2', sx0 + 72, sy0 + 40, ZF + 64, 46, 15, 44, mat('teal'), yaw=-12.0, roll=-20.0)
    S('BW_Pillow_2Pat', sx0 + 72, sy0 + 47, ZF + 64, 30, 4, 28, mat('tealpat'), yaw=-12.0, roll=-20.0)
    for i, (x, w, k) in enumerate(((-2850, 40, 'white'), (-2830, 35, 'black'), (-2815, 30, 'yellow'), (-2800, 38, 'grey'), (-2840, 30, 'green'))):
        B('BW_Clothes_%d' % i, x - w * .5, x + w * .5, sy0 + 30 + i * 3, sy0 + 62 + i * 2, ZF + 44 + i * 2.2, ZF + 46.2 + i * 2.2, mat(k), yaw=i * 17.0)
    B('BW_LaptopBase', -2795, -2765, sy0 + 50, sy0 + 72, ZF + 44, ZF + 46, mat('laptop'), yaw=-10.0)
    B('BW_LaptopLid', -2795, -2765, sy0 + 48, sy0 + 50, ZF + 46, ZF + 66, mat('laptop'), yaw=-10.0, roll=-12.0)
    B('BW_LaundryBasket', -2585, -2535, YS + 4, YS + 40, ZF, ZF + 56, mat('basket'), col='BlockAll')
    B('BW_LaundryLid', -2588, -2532, YS + 2, YS + 42, ZF + 56, ZF + 60, mat('basket'))
    for i in range(4):
        B('BW_LaundryVent_%d' % i, -2582 + i * 12, -2574 + i * 12, YS + 39.8, YS + 40.4, ZF + 8, ZF + 30, mat('grey'))
    # ---- striped rug and the rustic coffee table with what's on it in the photos
    rx0, rx1, ry0r, ry1r = -2820.0, -2500.0, -1250.0, -1075.0
    B('BW_Rug', rx0, rx1, ry0r, ry1r, ZF, ZF + .8, mat('rug'))
    for i in range(10):
        x = rx0 + 18 + i * 32
        B('BW_RugStripe_%d' % i, x - 1.5, x + 1.5, ry0r + 6, ry1r - 6, ZF + .8, ZF + 1.0, mat('rugstripe'))
    tx0, tx1, ty0, ty1, tz = -2770.0, -2640.0, -1225.0, -1160.0, ZF + 46.0
    for i in range(4):
        B('BW_TablePlank_%d' % i, tx0, tx1, ty0 + i * 16.3, ty0 + i * 16.3 + 15.8, tz - 4, tz, mat('walnut' if i % 2 else 'walnut2'), col='BlockAll')
    B('BW_TableApron_N', tx0 + 4, tx1 - 4, ty1 - 6, ty1 - 3, tz - 13, tz - 4, mat('walnut2'))
    B('BW_TableApron_S', tx0 + 4, tx1 - 4, ty0 + 3, ty0 + 6, tz - 13, tz - 4, mat('walnut2'))
    for i, (x, yy) in enumerate(((tx0 + 6, ty0 + 6), (tx1 - 6, ty0 + 6), (tx0 + 6, ty1 - 6), (tx1 - 6, ty1 - 6))):
        B('BW_TableLeg_%d' % i, x - 4, x + 4, yy - 4, yy + 4, ZF, tz - 4, mat('walnut2'), col='BlockAll')
    B('BW_TableShelf', tx0 + 4, tx1 - 4, ty0 + 4, ty1 - 4, ZF + 10, ZF + 13, mat('walnut'))
    for i, (x, yy, h, k) in enumerate(((-2700, -1185, 20, 'cup'), (-2690, -1200, 15, 'cup'), (-2672, -1176, 11, 'cup'), (-2725, -1205, 18, 'cup'))):
        C('BW_Cup_%d' % i, x, yy, tz, tz + h, 5.0 if h > 14 else 4.2, mat(k))
        C('BW_CupLogo_%d' % i, x, yy, tz + h * .45, tz + h * .7, (5.0 if h > 14 else 4.2) + .1, mat('red' if i % 2 == 0 else 'cup'))
    P('BW_PaperTowels', KIT + 'SM_Kitchen_PaperTowelRoll', -2712, -1172, tz, 0.0, 'Paper towels', .45)
    C('BW_PretzelJar', -2660, -1195, tz, tz + 24, 7.5, mat('jar'))
    C('BW_PretzelJarLid', -2660, -1195, tz + 24, tz + 27, 7.6, mat('black'))
    C('BW_WaterBottle', -2680, -1170, tz, tz + 25, 4.2, mat('purple'))
    C('BW_Shaker', -2648, -1215, tz, tz + 22, 4.5, mat('black'))
    B('BW_Papers', -2745, -2722, -1220, -1200, tz, tz + .5, mat('paper'), yaw=14.0)
    B('BW_Sunglasses', -2755, -2742, -1182, -1177, tz, tz + 1.5, mat('black'), yaw=-25.0)
    B('BW_Keys', -2732, -2726, -1190, -1182, tz, tz + 1.5, mat('steel'), yaw=30.0)
    # flip flops near the door and sneakers by the loft
    for i, (x, yy, yaw, k) in enumerate(((-2540, -1060, 15.0, 'black'), (-2525, -1066, -10.0, 'black'), (-2470, -1290, 40.0, 'blue'), (-2458, -1300, 55.0, 'blue'))):
        B('BW_FlipFlop_%d' % i, x - 13, x + 13, yy - 5, yy + 5, ZF, ZF + 1.6, mat(k), yaw=yaw)
    for i, (x, yy) in enumerate(((-2690, -760), (-2672, -775))):
        B('BW_Sneaker_%d' % i, x - 15, x + 15, yy - 5.5, yy + 5.5, ZF, ZF + 10, mat('white'), yaw=30.0 + i * 25)
    # ---- ceiling: round light, square diffuser near the loft, rectangular return near the door, smoke detector
    C('BW_CeilingLight', -2660, -990, ZC - 10, ZC, 22, mat('lightglass'))
    C('BW_CeilingLightRim', -2660, -990, ZC - 2, ZC, 24, mat('trim'))
    L('BW_CeilingLamp', -2660, -990, ZC - 40, 1700.0, 700.0, (1.0, .93, .80))
    L('BW_WindowFill', XW + 80, (WIN_Y0 + WIN_Y1) * .5, ZF + 170, 250.0, 380.0, (.85, .9, 1.0))
    B('BW_Diffuser', -2760 - 22, -2760 + 22, -810 - 22, -810 + 22, ZC - 1.5, ZC, mat('trim'))
    B('BW_DiffuserInner', -2760 - 12, -2760 + 12, -810 - 12, -810 + 12, ZC - 2.2, ZC - 1.5, mat('wall'))
    B('BW_Return', -2480 - 30, -2480 + 30, -1220 - 14, -1220 + 14, ZC - 1.5, ZC, mat('trim'))
    for i in range(6):
        x = -2480 - 25 + i * 10
        B('BW_ReturnSlat_%d' % i, x - 1, x + 1, -1232, -1208, ZC - 2.5, ZC - 1.5, mat('base'))
    C('BW_SmokeDetector', -2560, YS + 3, ZC - 40, ZC - 34, 7, mat('white'), roll=90.0)
    B('BW_LightSwitch', XE - 1.5, XE, -1120, -1112, ZF + 112, ZF + 124, mat('white'))
    # the vent hatch the vent network uses, on the new north wall
    return list(OUT)

# ---------------------------------------------------------------- helpers
def spawn_sm(row):
    a = ELL.spawn_actor_from_class(unreal.StaticMeshActor, row['loc'], R(row.get('yaw', 0.0), row.get('pitch', 0.0), row.get('roll', 0.0)))
    a.set_actor_label(row['label']); a.set_folder_path(FOLDER_BED)
    c = a.static_mesh_component
    c.set_static_mesh(asset(row['mesh']))
    a.set_actor_scale3d(V(*row.get('scale', (1, 1, 1))))
    c.set_collision_profile_name(row.get('collision', 'BlockAll'))
    c.set_editor_property('generate_overlap_events', False)
    if row.get('hidden'): c.set_visibility(False); a.set_actor_hidden_in_game(True)
    for i, m in enumerate(row.get('materials', []) or []):
        if m: c.set_material(i, m if isinstance(m, unreal.MaterialInterface) else asset(m))
    return a

def spawn_prop(row):
    a = ELL.spawn_actor_from_class(unreal.HouseInteractiveProp, row['loc'], R(row.get('yaw', 0.0)))
    a.set_actor_label(row['label']); a.set_folder_path(FOLDER_BED)
    base = row['mesh']
    a.set_editor_property('prop_id', unreal.Name(row['prop_id']))
    a.set_editor_property('prop_name', row.get('name', 'furniture'))
    a.set_editor_property('intact_mesh', asset(base))
    for key, suffix in (('broken_mesh', '_BrokenV2'), ('damaged_mesh', '_DamageV2'), ('heavily_damaged_mesh', '_HeavyV2')):
        m = maybe(base + suffix)
        if m: a.set_editor_property(key, m)
    a.set_editor_property('mass_kg', float(row.get('mass', 8.0)))
    a.set_editor_property('movable', bool(row.get('movable', True)))
    a.set_editor_property('breakable', bool(row.get('breakable', True)))
    a.set_editor_property('wall_mounted', bool(row.get('wall', False)))
    if row.get('materials'):
        a.set_editor_property('material_overrides', [(m if isinstance(m, unreal.MaterialInterface) else asset(m)) if m else None for m in row['materials']])
    if row.get('scale'): a.set_actor_scale3d(V(*row['scale']))
    a.refresh_prop()
    return a

def spawn_light(row):
    a = ELL.spawn_actor_from_class(unreal.PointLight, row['loc'])
    a.set_actor_label(row['label']); a.set_folder_path(FOLDER_BED)
    c = a.get_component_by_class(unreal.PointLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
    c.set_intensity(row['intensity']); c.set_attenuation_radius(row['radius'])
    c.set_light_color(unreal.LinearColor(*row['color'], 1.0))
    c.set_editor_property('cast_shadows', False)
    return a

def snapshot(a):
    r = {'class': a.get_class().get_name(), 'loc': [round(v, 2) for v in (a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z)],
         'rot': [round(v, 2) for v in (a.get_actor_rotation().pitch, a.get_actor_rotation().yaw, a.get_actor_rotation().roll)], 'folder': str(a.get_folder_path())}
    return r

# ---------------------------------------------------------------- run
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
actors = {a.get_actor_label(): a for a in ELL.get_all_level_actors()}
before = {k: snapshot(a) for k, a in actors.items()}
report = {'moved': [], 'reskinned': [], 'removed': [], 'created': [], 'pigs': {}}

# 0. Remove any previous bedroom build so the script is repeatable.
for label, a in list(actors.items()):
    if label.startswith('BW_'):
        ELL.destroy_actor(a); report['removed'].append(label); del actors[label]

# 1. Pig pen into the closet.
missing = [k for k in PEN_MOVES if k not in actors]
if missing: raise RuntimeError('Pig pen actors missing: ' + ', '.join(missing))
for label, (loc, yaw) in PEN_MOVES.items():
    a = actors[label]
    a.modify()
    a.set_actor_location(loc, False, True)
    a.set_actor_rotation(R(yaw), True)
    a.set_folder_path(FOLDER_PEN)
    report['moved'].append(label)
for label in ('PIG_Pepper', 'PIG_Coal'):
    pig = actors[label]
    for key, val in PIG_COMMON.items(): pig.set_editor_property(key, val)
    for key, val in PIG_PROPS[label].items(): pig.set_editor_property(key, val)
    pig.set_editor_property('roam_graph', [])          # use the rebuilt C++ default graph (closet + library)
    report['pigs'][label] = {'door': str(pig.get_editor_property('pigsty_door_id')), 'bed': str(pig.get_editor_property('bed_location')),
                             'min': str(pig.get_editor_property('pigsty_minimum')), 'max': str(pig.get_editor_property('pigsty_maximum'))}
pile = actors['PIG_EdibleCheetos']
pile.set_editor_property('feeding_offset_a', V(-65.0, 0.0, -.5)); pile.set_editor_property('feeding_offset_b', V(65.0, 0.0, -.5))
pile.set_editor_property('scatter_radius', 42.0)
for label in PEN_STAYS:
    if label in actors: actors[label].set_folder_path(FOLDER_BED)

# 2. The old pigsty shell is replaced by the BW_ shell (room enlarged to Room 204's size).
for label in ('PIG_ExpandedRoom', 'PIG_ExpandedRoomCollision'):
    c = actors[label].static_mesh_component
    c.set_visibility(False); actors[label].set_actor_hidden_in_game(True)
    c.set_collision_profile_name('NoCollision'); c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    report['reskinned'].append(label + ' hidden')
vent = actors['PIG_Vent']
vent.set_actor_location(V(-2468.0, YN - 1.2, 486.0), False, True); vent.set_actor_rotation(R(180.0), True)
vent.set_folder_path(FOLDER_BED)
unreal.EditorAssetLibrary.make_directory(BWDIR)

# 3. Build the bedroom.
for row in bedroom_actors():
    k = row['kind']
    a = spawn_sm(row) if k == 'sm' else spawn_prop(row) if k == 'prop' else spawn_light(row)
    report['created'].append(row['label'])

# 4. Guard: nothing else moved.
after = {a.get_actor_label(): snapshot(a) for a in ELL.get_all_level_actors()}
expected = set(PEN_MOVES) | PEN_STAYS | {'PIG_ExpandedRoom', 'PIG_Vent'}
changed = [k for k, v in before.items() if k in after and after[k] != v and k not in expected and not k.startswith('BW_')]
gone = [k for k in before if k not in after and not k.startswith('BW_')]
if changed or gone: raise RuntimeError('Unexpected changes: changed=%s gone=%s' % (changed, gone))
if not ELL.save_current_level(): raise RuntimeError('Level save failed')
os.makedirs(os.path.dirname(REPORT), exist_ok=True)
json.dump(report, open(REPORT, 'w'), indent=1)
LOG('OK moved=%d created=%d removed=%d' % (len(report['moved']), len(report['created']), len(report['removed'])))
