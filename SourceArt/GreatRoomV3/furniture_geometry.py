"""Great room reference furniture in real metres, for Blender 4/5.

Every public function returns only its new mesh objects. XY is local to the
furniture, the floor is Z=0, width is X, and upholstered seating faces -Y.
All materials are supplied by the caller. Objects have useful local transforms;
apply transforms after world placement, before joining/exporting.

Sofa and chair include soft, lightly creased upholstery, piping, a skirt,
low feet, and combined nailhead strips. Defaults follow IMG_2171..IMG_2175.
Sofa supports left_arm/right_arm=False for a composed sectional. ``chaise`` is
an optional matching long extension, with a side arm and no rear cushion.
"""

import bpy
import math
import hashlib
from mathutils import Vector


def _mat(ob, material):
    if material is not None:
        ob.data.materials.append(material)
    return ob


def _mesh(name, vertices, faces, material, smooth=False):
    data = bpy.data.meshes.new(name + "_Mesh")
    data.from_pydata(vertices, [], faces)
    data.update()
    ob = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(ob)
    _mat(ob, material)
    for polygon in data.polygons:
        polygon.use_smooth = smooth
    # Planar box coordinates give each surface predictable metre-scale UVs.
    # Leather/wood shaders can use these or their object/position coordinates.
    uv = data.uv_layers.new(name="UVMap")
    for polygon in data.polygons:
        n = polygon.normal
        axis = max(range(3), key=lambda a: abs(n[a]))
        a, b = ((1, 2), (0, 2), (0, 1))[axis]
        for loop_index in polygon.loop_indices:
            p = data.vertices[data.loops[loop_index].vertex_index].co
            uv.data[loop_index].uv = (p[a], p[b])
    return ob


def _seed(name):
    return int(hashlib.md5(name.encode()).hexdigest()[:6], 16) / 16777215 * math.tau


def _surface_point(raw, half, radius, puff, wrinkles, seat_sag, back_puff,
                   phase, profile='soft'):
    """One tailored surface sample, shared by the leather skin and its welt."""
    core = [max(-h + radius, min(h - radius, q)) for q, h in zip(raw, half)]
    normal = Vector([q - c for q, c in zip(raw, core)]).normalized()
    p = Vector(core) + normal * radius
    x, y, z = [p[a] / max(half[a], 1e-6) for a in range(3)]
    faceaxis = max(range(3), key=lambda a: abs(normal[a]))
    u, v = [(y, z), (x, z), (x, y)][faceaxis]
    fullness = (max(0, 1-u*u) * max(0, 1-v*v)) ** .75
    broad = puff * fullness * (1 + .06*math.sin(phase + u*3.1 + v*2.3))
    # Leather has broad panels with creases near pressure points. Avoid a
    # repeated sine displacement over every edge, which reads as melted cloth.
    fold = 0.0
    for side in (-1, 1):
        pressure = math.exp(-((u-side*.70)/.22)**2-((v+.52)/.43)**2)
        fold += pressure * math.sin(u*17 + v*7 + phase) * .70
        corner = math.exp(-((u-side*.75)/.20)**2-((v-.76)/.22)**2)
        fold += corner * math.sin(v*19-u*5+phase*.5) * .45
    crease = wrinkles * fold
    if profile == 'frame':
        broad *= .30
        crease *= .35
    p += normal * (broad + crease)
    if normal.z > .60 and seat_sag:
        p.z -= seat_sag * math.exp(-((x/.65)**2+(y/.63)**2))
    if normal.y < -.6 and back_puff:
        p.y -= back_puff * fullness
        # A shallow vertical compression fold, rather than a central notch.
        p.y += wrinkles*.65*math.exp(-((x-.17)/.20)**2)*(1-z*z)
    return p


def _softbox(name, loc, size, material, radius=.045, puff=.008, wrinkles=.0025,
             seat_sag=0.0, back_puff=0.0, lean=0.0, detail=False, profile='soft'):
    """Closed upholstery cage, with finished skin and no export modifiers."""
    half = [v / 2 for v in size]
    radius = min(radius, min(half) * .90)
    grid = []
    for h in half:
        core = h - radius
        interior = [-core*.50, 0.0, core*.50] if detail else [0.0]
        grid.append([-h, -h + radius * .30, -core, *interior,
                     core, h - radius * .30, h])
    vertices = []
    indices = {}
    phase = _seed(name)
    def point(raw):
        key = tuple(round(x, 8) for x in raw)
        if key in indices:
            return indices[key]
        p = _surface_point(raw, half, radius, puff, wrinkles, seat_sag,
                           back_puff, phase, profile)
        idx = len(vertices)
        indices[key] = idx
        vertices.append(tuple(p))
        return idx
    faces=[]
    for axis in range(3):
        a,b = [k for k in range(3) if k != axis]
        for sign in (-1,1):
            rows=[]
            for va in grid[a]:
                row=[]
                for vb in grid[b]:
                    q=[0.,0.,0.]
                    q[axis]=sign*half[axis]; q[a]=va; q[b]=vb
                    row.append(point(q))
                rows.append(row)
            for i in range(len(rows)-1):
                for j in range(len(rows[0])-1):
                    face=[rows[i][j],rows[i+1][j],rows[i+1][j+1],rows[i][j+1]]
                    aa=Vector(vertices[face[1]])-Vector(vertices[face[0]])
                    bb=Vector(vertices[face[2]])-Vector(vertices[face[0]])
                    if aa.cross(bb)[axis]*sign < 0:
                        face.reverse()
                    faces.append(face)
    ob=_mesh(name, vertices, faces, material, True)
    ob.location=loc
    ob.rotation_euler.x=math.radians(lean)
    # The matching seam samples the same surface and receives the same lean.
    ob['_tailoring_size'] = list(size)
    ob['_tailoring_values'] = [radius,puff,wrinkles,seat_sag,back_puff,phase]
    ob['_tailoring_profile'] = profile
    return ob


def _box(name, loc, size, material, bevel=.006):
    x,y,z=[v/2 for v in size]
    verts=[(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),
           (-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)]
    faces=[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    ob=_mesh(name,verts,faces,material)
    ob.location=loc
    if bevel:
        mod=ob.modifiers.new("Small eased edge",'BEVEL'); mod.width=bevel; mod.segments=2
        # Apply here so callers receive finished exportable meshes.
        bpy.context.view_layer.objects.active=ob; ob.select_set(True)
        bpy.ops.object.modifier_apply(modifier=mod.name); ob.select_set(False)
    return ob


def _tube(name, points, radius, material, closed=False, sides=6):
    points=[Vector(p) for p in points]
    verts=[]; faces=[]
    count=len(points)
    for i,p in enumerate(points):
        prev=points[(i-1)%count] if closed or i else p
        nxt=points[(i+1)%count] if closed or i<count-1 else p
        tangent=(nxt-prev).normalized()
        ref=Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((0,1,0))
        u=tangent.cross(ref).normalized(); v=tangent.cross(u).normalized()
        for j in range(sides):
            ang=j*math.tau/sides
            verts.append(tuple(p+radius*(math.cos(ang)*u+math.sin(ang)*v)))
    steps=count if closed else count-1
    for i in range(steps):
        for j in range(sides):
            faces.append((i*sides+j,i*sides+(j+1)%sides,
                          ((i+1)%count)*sides+(j+1)%sides,((i+1)%count)*sides+j))
    if not closed:
        faces.append(tuple(range(sides-1,-1,-1)))
        faces.append(tuple((count-1)*sides+j for j in range(sides)))
    return _mesh(name,verts,faces,material,True)


def _rect_points(width,height,radius=.035):
    x=width/2; y=height/2; r=min(radius,x*.7,y*.7)
    result=[]
    for cx,cy,start in [(x-r,y-r,0),(-x+r,y-r,90),(-x+r,-y+r,180),(x-r,-y+r,270)]:
        for j in range(4):
            angle=math.radians(start+j*90/3)
            result.append((cx+r*math.cos(angle),cy+r*math.sin(angle)))
    return result


def _piping(name,loc,width,height,material,axis='Z',radius=.0035,lean=0.):
    loop=_rect_points(width,height)
    pts=[(a,b,0) if axis=='Z' else (a,0,b) for a,b in loop]
    ob=_tube(name,pts,radius,material,True,4)
    ob.location=loc; ob.rotation_euler.x=math.radians(lean)
    return ob


def _fitted_welt(name, body, material, axis='Z', offset=0., radius=.0026):
    """A stitched rolled edge fitted to the cushion's real rounded surface."""
    size = list(body['_tailoring_size'])
    values = list(body['_tailoring_values'])
    bevel, puff, wrinkles, seat_sag, back_puff, phase = values
    half = [v/2 for v in size]
    a,b = (0,1) if axis == 'Z' else (0,2)
    fixed = 2 if axis == 'Z' else 1
    spans=[]
    for k in (a,b):
        h=half[k];core=h-bevel
        spans.append([-h,-h+bevel*.30,-core,0.,core,h-bevel*.30,h])
    raw_loop=[]
    for u in spans[0]:raw_loop.append((u,-half[b]))
    for v in spans[1][1:]:raw_loop.append((half[a],v))
    for u in reversed(spans[0][:-1]):raw_loop.append((u,half[b]))
    for v in reversed(spans[1][1:-1]):raw_loop.append((-half[a],v))
    points=[]
    for u,v in raw_loop:
        raw=[0.,0.,0.];raw[a]=u;raw[b]=v
        raw[fixed]=offset if axis=='Z' else -half[1]
        p=_surface_point(raw,half,bevel,puff,wrinkles,seat_sag,back_puff,
                         phase,body['_tailoring_profile'])
        points.append(p)
    ob=_tube(name,points,radius,material,True,4)
    ob.location=body.location.copy()
    ob.rotation_euler=body.rotation_euler.copy()
    ob.scale=body.scale.copy()
    return ob


def _nailheads(name,points,material,radius=.0045):
    # Tiny eight sided domes share one mesh, rather than one draw item per tack.
    verts=[]; faces=[]
    for xyz,normal in points:
        center=Vector(xyz); n=Vector(normal).normalized()
        ref=Vector((0,0,1)) if abs(n.z)<.9 else Vector((0,1,0))
        u=n.cross(ref).normalized(); v=n.cross(u).normalized()
        start=len(verts)
        verts.append(tuple(center+n*radius*.60))
        for j in range(8):
            ang=j*math.tau/8
            verts.append(tuple(center+radius*(u*math.cos(ang)+v*math.sin(ang))))
        for j in range(8):
            faces.append((start,start+1+j,start+1+(j+1)%8))
    return _mesh(name,verts,faces,material,True)


def _legs(name,width,depth,height,wood,parts):
    for x in (-width*.37,width*.37):
        for y in (-depth*.34,depth*.34):
            # A short tapered hardwood block, slightly recessed under the skirt.
            lo=.061/2;hi=.077/2
            verts=[(-lo,-lo,0),(lo,-lo,0),(lo,lo,0),(-lo,lo,0),
                   (-hi,-hi,height),(hi,-hi,height),(hi,hi,height),(-hi,hi,height)]
            faces=[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
            foot=_mesh(name+"_TaperedWalnutFoot",verts,faces,wood)
            foot.location=(x,y,0);parts.append(foot)


def sofa(name,width=1.88976,depth=.98,height=.92,material=None,wood=None,
         metal=None,seat_count=3,left_arm=True,right_arm=True):
    parts=[]; h=height/.92; arm=.155; feet=.070*h
    _legs(name,width,depth,feet,wood,parts)
    parts.append(_softbox(name+"_LowerSkirt",(0,0,.235*h),(width-.04,depth-.04,.33*h),material,.055,.005,.002))
    parts.append(_piping(name+"_ApronSeam",(0,-depth/2+.045,.255*h),width-.10,.205*h,material,'Y',.0025))
    inner=width-(arm if left_arm else .02)-(arm if right_arm else .02)
    seat_w=(inner-.015*(seat_count-1))/seat_count
    start=-width/2+(arm if left_arm else .02)+seat_w/2
    for i in range(seat_count):
        x=start+i*(seat_w+.015)
        seat=_softbox(f"{name}_Seat{i+1}",(x,-.095,.438*h),(seat_w,.715,.165*h),material,.047,.013,.0028,.017*h)
        parts.append(seat)
        parts.append(_fitted_welt(f"{name}_SeatPiping{i+1}",seat,material,'Z',-.014*h,.0022))
        loc=(x,depth/2-.21,.695*h)
        back=_softbox(f"{name}_BackCushion{i+1}",loc,(seat_w+.004,.245,.47*h),material,.057,.007,.0038,0,.021,-10,True)
        parts.append(back)
        parts.append(_fitted_welt(f"{name}_BackPiping{i+1}",back,material,'Y',radius=.0022))
    parts.append(_softbox(name+"_RearUpholsteredFrame",(0,depth/2-.07,.575*h),(width-.035,.145,.65*h),material,.033,.003,.002,profile='frame'))
    for i in range(1,seat_count):
        xx=start+(i-.5)*(seat_w+.015)
        pts=[(xx,depth/2+.0025,z*h) for z in (.26,.43,.62,.81,.89)]
        parts.append(_tube(name+"_RearPanelStitch",pts,.0014,material,False,4))
    nail_points=[]
    for side,enabled in ((-1,left_arm),(1,right_arm)):
        if not enabled: continue
        x=side*(width/2-arm/2)
        parts.append(_softbox(name+"_ArmPanel",(x,-.015,.415*h),(arm,depth-.07,.59*h),material,.040,.006,.003))
        cap=_softbox(name+"_ArmCap",(x,-.023,.688*h),(arm+.025,depth-.055,.105*h),material,.031,.004,.0017)
        parts.append(cap)
        parts.append(_fitted_welt(name+"_ArmCapSeam",cap,material,'Z',-.011*h,.0020))
        # Reference front arm seams carry understated brass upholstery studs.
        for i in range(21):
            z=.14*h+i*.026*h
            nail_points.append(((side*(width/2-.004),-depth/2+.075,z),(side,0,0)))
    if metal and nail_points:
        parts.append(_nailheads(name+"_NailheadTrim",nail_points,metal))
    return parts


def club_chair(name,material=None,wood=None,metal=None,width=.85,depth=.92,height=.94):
    parts=[]; h=height/.94; arm=.145
    _legs(name,width,depth,.055*h,wood,parts)
    parts.append(_softbox(name+"_Skirt",(0,0,.233*h),(width-.035,depth-.025,.36*h),material,.054,.008,.003))
    parts.append(_piping(name+"_ApronSeam",(0,-depth/2+.027,.248*h),width-.080,.25*h,material,'Y',.0026))
    # Reference club chairs have a flat broad outside leather back. Its lower
    # panel narrows gently toward the seat; the shoulder edge stays square.
    rear=_softbox(name+"_RearFrame",(0,depth/2-.074,.570*h),(width-.030,.170,.73*h),material,.027,.002,.0018,detail=True,profile='frame')
    for v in rear.data.vertices:
        t=max(0.,min(1.,v.co.z/(.73*h)+.5))
        v.co.x*=.92+.08*t
        v.co.y+=.018*(t-.5)
    rear.data.update();parts.append(rear)
    seat_w=width-2*arm+.015
    seat=_softbox(name+"_Seat",(0,-.095,.439*h),(seat_w,.625,.155*h),material,.042,.014,.003,.019*h)
    parts.append(seat)
    parts.append(_fitted_welt(name+"_SeatSeam",seat,material,'Z',-.016*h,.0023))
    loc=(0,.235,.699*h)
    back=_softbox(name+"_Back",loc,(width-2*arm+.04,.194,.476*h),material,.044,.005,.0033,0,.012,-10,True)
    parts.append(back)
    parts.append(_fitted_welt(name+"_BackSeam",back,material,'Y',radius=.0022))
    nails=[]
    for side in (-1,1):
        x=side*(width/2-arm/2)
        parts.append(_softbox(name+"_SideArm",(x,-.015,.414*h),(arm,depth-.065,.57*h),material,.032,.004,.002,profile='frame'))
        cap=_softbox(name+"_PaddedArm",(x,-.025,.672*h),(arm+.030,depth-.07,.090*h),material,.025,.003,.0018)
        parts.append(cap)
        parts.append(_fitted_welt(name+"_ArmSeam",cap,material,'Z',-.010*h,.0021))
        # A recessed side-panel stitch follows the flat outside leather face.
        xx=side*(width/2-.001)
        pts=[(xx,-depth*.20,.17*h),(xx,-depth*.20,.31*h),
             (xx,-depth*.20,.47*h),(xx,-depth*.20,.60*h)]
        parts.append(_tube(name+"_SidePanelStitch",pts,.0013,material,False,4))
        # Outside rectangular stitching plus evenly spaced nailheads at front.
        for i in range(20):
            z=.125*h+i*.027*h
            nails.append(((side*(width/2-.001),-depth/2+.075,z),(side,0,0)))
        for i in range(13):
            y=-depth/2+.082+i*.052
            nails.append(((side*(width/2-.001),y,.650*h),(side,0,0)))
    if metal:
        parts.append(_nailheads(name+"_NailheadTrim",nails,metal,.0042))
    return parts


def chaise(name,width=1.06,length=2.7432,height=.92,material=None,wood=None,
           metal=None,side='left'):
    """Matching sectional return. Rear is +Y; no rear back or inside arm.

    The caller overlaps its rear 0.92m with a sofa's end seat. The long single
    cushion and outside upholstered arm run to the forward (-Y) extension.
    Width and total length are the real complete footprint, before house scaling.
    """
    parts=[]; h=height/.92; s=-1 if side=='left' else 1; arm=.155
    _legs(name,width,length,.070*h,wood,parts)
    parts.append(_softbox(name+"_LowerSkirt",(0,0,.235*h),(width-.035,length-.045,.33*h),material,.055,.006,.002))
    seat=_softbox(name+"_LongSeat",(-s*.055,-.025,.438*h),(width-arm-.025,length-.08,.165*h),material,.046,.014,.003,.018*h,detail=True)
    parts.append(seat)
    parts.append(_fitted_welt(name+"_LongSeatSeam",seat,material,'Z',-.014*h,.0023))
    x=s*(width/2-arm/2)
    arm_len=length-.13
    parts.append(_softbox(name+"_OuterArm",(x,.015,.415*h),(arm,arm_len,.59*h),material,.045,.006,.003))
    cap=_softbox(name+"_OuterArmCap",(x,.015,.688*h),(arm+.025,arm_len,.105*h),material,.031,.004,.0018)
    parts.append(cap)
    parts.append(_fitted_welt(name+"_OuterArmSeam",cap,material,'Z',-.012*h,.0021))
    return parts


def _disc(name,radius,depth,z,material,segments=48,edge_wave=0.):
    verts=[]; faces=[]
    for zz in (z-depth/2,z+depth/2):
        for i in range(segments):
            a=math.tau*i/segments
            rr=radius+edge_wave*(math.sin(a*7+.9)+.4*math.sin(a*13))
            verts.append((rr*math.cos(a),rr*math.sin(a),zz))
    faces.append(tuple(range(segments-1,-1,-1)))
    faces.append(tuple(segments+i for i in range(segments)))
    for i in range(segments):
        j=(i+1)%segments; faces.append((i,j,segments+j,segments+i))
    return _mesh(name,verts,faces,material)


def round_table(name,wood=None,metal=None,diameter=1.05,height=.46):
    parts=[]; r=diameter/2; top=.041
    parts.append(_disc(name+"_WoodTop",r,top,height-top/2,wood,64,.003))
    # Four slightly outward legs and two actual structural rings like the photo.
    for a in (45,135,225,315):
        angle=math.radians(a)
        x=r*.77*math.cos(angle); y=r*.77*math.sin(angle)
        parts.append(_tube(name+"_Leg",[(x*1.12,y*1.12,.025),(x,y,height-top)],.012,metal,False,6))
    for zz,rr in ((.068,r*.86),(.268,r*.80),(height-top-.012,r*.80)):
        pts=[(rr*math.cos(math.tau*i/48),rr*math.sin(math.tau*i/48),zz) for i in range(48)]
        parts.append(_tube(name+"_IronRing",pts,.010,metal,True,6))
    return parts


def _live_top(name,width,depth,height,wood,thickness=.044):
    """Walnut slab with subtly irregular long edges, no exaggerated bark."""
    steps=12; verts=[]; faces=[]
    for zz in (height-thickness,height):
        for side in (-1,1):
            for i in range(steps+1):
                x=-width/2+width*i/steps
                y=side*(depth/2+.010*math.sin(i*1.5+.3)+.003*math.sin(i*3.9))
                verts.append((x,y,zz))
    n=steps+1
    for i in range(steps):
        faces.append((i,i+1,n+i+1,n+i))
        faces.append((2*n+i,3*n+i,3*n+i+1,2*n+i+1))
        faces.append((i,2*n+i,2*n+i+1,i+1))
        faces.append((n+i,n+i+1,3*n+i+1,3*n+i))
    faces.append((0,n,3*n,2*n))
    faces.append((steps,2*n+steps,3*n+steps,n+steps))
    # Slab loops are authored from the bottom; reverse to outward normals.
    return _mesh(name,verts,[tuple(reversed(face)) for face in faces],wood)


def end_table(name,wood=None,metal=None,width=.55,depth=.55,height=.56):
    parts=[_live_top(name+"_LiveEdgeTop",width,depth,height,wood)]
    t=.027; ztop=height-.044
    for x in (-width*.34,width*.34):
        for y in (-depth*.34,depth*.34):
            parts.append(_box(name+"_SquareLeg",(x,y,ztop/2),(t,t,ztop),metal,.003))
        parts.append(_box(name+"_BottomCrossbar",(x,0,.027),(t,depth*.73,t),metal,.003))
        parts.append(_box(name+"_TopCrossbar",(x,0,ztop-.023),(t,depth*.73,t),metal,.003))
    parts.append(_box(name+"_LongBrace",(0,0,.13),(width*.72,.023,.023),metal,.003))
    return parts


def console(name,wood=None,metal=None,width=1.35,depth=.40,height=.76):
    parts=[_live_top(name+"_LiveEdgeSlab",width,depth,height,wood,.055)]
    t=.035; h=height-.055
    # Two inverted-U rectangular steel frames and lower connection beam.
    for x in (-width*.34,width*.34):
        for y in (-depth*.33,depth*.33):
            parts.append(_box(name+"_Upright",(x,y,h/2),(t,t,h),metal,.003))
        parts.append(_box(name+"_FrameFloor",(x,0,t/2),(t,depth*.74,t),metal,.003))
        parts.append(_box(name+"_FrameTop",(x,0,h-.015),(t,depth*.74,t),metal,.003))
    parts.append(_box(name+"_SteelBrace",(0,depth*.30,.23),(width*.71,t,t),metal,.003))
    return parts


def stool(name,metal=None):
    parts=[]; height=.68; top=.027; seat=.35
    # Four strips enclose the photo's carrying slot in the metal seat.
    parts.append(_box(name+"_SeatFront",(0,-.0875,height-top/2),(seat,.14,top),metal,.008))
    parts.append(_box(name+"_SeatRear",(0,.0875,height-top/2),(seat,.14,top),metal,.008))
    for x in (-.143,.143):
        parts.append(_box(name+"_SeatSide",(x,0,height-top/2),(.064,.038,top),metal,.005))
    for x in (-1,1):
        for y in (-1,1):
            parts.append(_tube(name+"_TaperedLeg",[(x*.208,y*.208,.013),(x*.126,y*.126,height-top)],.019,metal,False,4))
            parts.append(_box(name+"_FootCap",(x*.208,y*.208,.012),(.047,.047,.024),metal,.003))
    zz=.23; spread=.18
    pts=[(-spread,-spread,zz),(spread,-spread,zz),(spread,spread,zz),(-spread,spread,zz)]
    parts.append(_tube(name+"_Footrest",pts,.014,metal,True,4))
    return parts


def pillow(name,material=None,size=(.48,.48,.18)):
    """A stuffed cloth envelope, with pinched corners and local diagonal folds.

    It lies in XY for callers to tilt on a seat. Width/depth remain exact, and
    the bottom still meets Z=0. Cloth panels and the folded welt are separate
    returned meshes, with no extra scene or material state.
    """
    width,depth,thick=size
    steps=16;verts=[];faces=[];phase=_seed(name)
    def sample(u,v,side):
        x=u*width/2*(1-.095*v*v)
        y=v*depth/2*(1-.095*u*u)
        fullness=(max(0.,1-u*u)*max(0.,1-v*v))**.65
        corner=math.exp(-((abs(u)-.73)/.29)**2-((abs(v)-.70)/.30)**2)
        folds=math.sin((u+v)*19+phase)*corner*min(.006,thick*.04)
        folds+=math.sin((u-v)*14+phase*.6)*corner*min(.0025,thick*.017)
        # One gathered corner has a longer compression fold fading toward the
        # stuffed center. Its groove is cloth scale, rather than a hard crease.
        diagonal=u-.58*v-.36
        gather=math.exp(-(diagonal/.075)**2-((v-.58)/.36)**2)
        folds-=min(.007,thick*.047)*gather
        z=side*(thick/2*(.13+.87*fullness)+folds)
        # The two skins meet a narrow gusset rather than a pointy bevel box.
        return (x,y,z)
    for side in (-1,1):
        for j in range(steps+1):
            for i in range(steps+1):
                verts.append(sample(-1+2*i/steps,-1+2*j/steps,side))
    row=steps+1;count=row*row
    for side_index in (0,1):
        start=side_index*count
        for j in range(steps):
            for i in range(steps):
                f=(start+j*row+i,start+j*row+i+1,
                   start+(j+1)*row+i+1,start+(j+1)*row+i)
                faces.append(f if side_index else tuple(reversed(f)))
    border=[i for i in range(row)]
    border += [j*row+steps for j in range(1,row)]
    border += [steps*row+i for i in range(steps-1,-1,-1)]
    border += [j*row for j in range(steps-1,0,-1)]
    for index,b in enumerate(border):
        c=border[(index+1)%len(border)]
        faces.append((b,c,c+count,b+count))
    cloth=_mesh(name+"_Cloth",verts,faces,material,True)
    cloth.location.z=thick/2
    seam_points=[]
    for b in border:
        low=Vector(verts[b]);high=Vector(verts[b+count])
        seam_points.append((low+high)/2)
    welt=_tube(name+"_FoldedWelt",seam_points,.0020,material,True,4)
    welt.location.z=thick/2
    parts=[cloth,welt]
    return parts
