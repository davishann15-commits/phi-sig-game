"""Photo-led Room101/102 entry art only. Run with Blender --background --python.
Emits unscaled local FBXs; map installer applies current uniform house root1.1.
Both leaves extend local+X, hallwayface local+Y; hingeorigin(0,0,0).
101 closedYaw-90/openYaw-180;102closedYaw0/openYaw-90. No map writes.
"""
import bpy,bmesh,os,math,json,collections,random,shutil
from mathutils import Vector
R=os.path.dirname(os.path.abspath(__file__));MODEL=R+'/models';TEX=R+'/textures'
for p in(MODEL,TEX):os.makedirs(p,exist_ok=True)
for suffix in ('BaseColor','Roughness'):
 shutil.copyfile(os.path.join(R,'../HallwayEnd','T_3Doors_OffWhitePaint_'+suffix+'.png'),os.path.join(TEX,'T_R101Entry_OffWhitePaint_'+suffix+'.png'))
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
G=collections.defaultdict(list);manifest={'coordinateSystem':'Blender metres => Unrealcm [100X,-100Y,100Z]. LeafcanvasextendsUE+X0..90cm. FrontUE+Y. Localhinge-floorpivot(0,0,0), verticalZhingeaxis. Allpartsonevisualleafsoturntogether. Actor root1.1 preserves enlargedhouse; leaf already authored144/169.2cmwide199.75cmtall.','assets':[],'materials':{},'hingeOriginCm':[0,0,0],'leafCanvasBoundsUnrealCm':{'101':[[0,-2.5,0],[144,2.5,199.75]],'102':[[0,-2.5,0],[169.2,2.5,199.75]]},'leafCollision':'ExistingHouseDoor movableboxroothandlesleaf; do not add staticleafcollider.','frameInnerClearUnrealCm':{'101':[[-.8,-7.5,0],[144.8,7.5,200.70]],'102':[[-.94,-7.5,0],[170.14,7.5,200.70]]},'notes':['Six true recessed panels onbothsides, not raisedslaboverlays.','Latchleverand12-keyelectronickeypadmoveaspartofleaf.','Black serif101/102numbers and light localizedpaintwear match userreference.','FramesarestaticandCOL_frameisplayerblockersforpostsandlintelonly.']}
def mat(name,c,rough=.6,metal=0,tex=None):
 m=bpy.data.materials.new('M_R101Entry_'+name);m.diffuse_color=(*c,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 row={'color':list(c),'roughness':rough,'metallic':metal,'uv_scale':1}
 if tex:
  file=TEX+'/'+tex+'_BaseColor.png';im=bpy.data.images.load(file);tx=m.node_tree.nodes.new('ShaderNodeTexImage');tx.image=im;m.node_tree.links.new(tx.outputs['Color'],p.inputs['Base Color']);row['base_texture']=file
  roughfile=TEX+'/'+tex+'_Roughness.png'
  if os.path.exists(roughfile):row['roughness_texture']=roughfile
 manifest['materials'][m.name]=row;return m
PAINT=mat('WarmOffWhitePaint',(.69,.67,.60),.64,tex='T_R101Entry_OffWhitePaint');PANE=mat('PanelRecessPaint',(.61,.60,.54),.65,tex='T_R101Entry_OffWhitePaint')
FRAME=mat('MatchingWhiteJamb',(.70,.68,.62),.64);STEEL=mat('BrushedSilverLever',(.43,.45,.46),.28,.93);DARK=mat('CharcoalElectronicHousing',(.008,.010,.011),.35);KEY=mat('RubberKeypadKeys',(.031,.035,.036),.67)
LABEL=mat('KeypadLegends',(.58,.61,.59),.48);SCREW=mat('DarkHardware',(.016,.018,.019),.36,.7);COL=mat('Collider',(.01,.01,.01),.9)
def mesh(name,v,f,ma,g,smooth=False):
 d=bpy.data.meshes.new(name);d.from_pydata(v,[],f);d.update();o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o)
 if ma:d.materials.append(ma)
 for p in d.polygons:p.use_smooth=smooth
 G[g].append(o);return o

def cube(name,loc,size,ma,g,b=.0,seg=3):
 x,y,z=[a/2 for a in size];v=[(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),(-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)]
 o=mesh(name,v,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],ma,g);o.location=loc
 if b:
  mod=o.modifiers.new('Clean softened edge','BEVEL');mod.width=min(b,min(size)*.45);mod.segments=seg;mod.harden_normals=True
  bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.modifier_apply(modifier=mod.name);o.select_set(False)
  for p in o.data.polygons:p.use_smooth=True
  o['weighted']=True
 return o

def cyl(name,loc,r,h,ma,g,axis='Z',n=24):
 bpy.ops.mesh.primitive_cylinder_add(vertices=n,radius=r,depth=h,location=loc);o=bpy.context.object;o.name=name
 if axis=='Y':o.rotation_euler.x=math.pi/2
 if axis=='X':o.rotation_euler.y=math.pi/2
 o.data.materials.append(ma)
 for p in o.data.polygons:p.use_smooth=len(p.vertices)==4
 o['weighted']=True;G[g].append(o);return o

def tube(name,pts,r,ma,g,sides=12):
 pts=[Vector(p) for p in pts];v=[];f=[]
 for i,p in enumerate(pts):
  t=pts[min(i+1,len(pts)-1)]-pts[max(i-1,0)];t.normalize();ref=Vector((0,0,1)) if abs(t.z)<.9 else Vector((0,1,0));u=t.cross(ref).normalized();w=t.cross(u)
  for j in range(sides):a=j*math.tau/sides;v.append(tuple(p+r*(math.cos(a)*u+math.sin(a)*w)))
 for i in range(len(pts)-1):
  for j in range(sides):f.append((i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j))
 f.extend([tuple(range(sides-1,-1,-1)),tuple((len(pts)-1)*sides+j for j in range(sides))])
 o=mesh(name,v,f,ma,g,True)
 for p in o.data.polygons[-2:]:p.use_smooth=False
 return o

def label(text,x,y,z,size,g):
 d=bpy.data.curves.new('Keypad digit '+text,'FONT');d.body=text;d.size=size;d.align_x='CENTER';d.align_y='CENTER';d.extrude=.00010;d.resolution_u=4
 o=bpy.data.objects.new('Keypad digit '+text,d);bpy.context.collection.objects.link(o);o.location=(x,y,z);o.rotation_euler=(math.pi/2,0,0);d.materials.append(LABEL)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH');G[g].append(bpy.context.object)

# Door structural slab has actual holesforrecesses, closedbyprofiledinsetpanels.
g='SM_3Doors_RecessedSixPanelLeaf'
xgrid=[0,.105,.400,.500,.795,.90];zgrid=[0,.15,.79,.89,1.57,1.69,1.98,2.10]
v=[];f=[]
for side in(-1,1):
 offset=len(v)
 for z in zgrid:
  for x in xgrid:v.append((x,side*.025,z))
 for j in range(len(zgrid)-1):
  for i in range(len(xgrid)-1):
   if i in(1,3) and j in(1,3,5):continue
   a=offset+j*len(xgrid)+i;q=(a,a+1,a+1+len(xgrid),a+len(xgrid));f.append(q if side<0 else q[::-1])
# Outer perimeter sidebands close frontbackcanvases.
for x in(0,.9):
 for j in range(len(zgrid)-1):
  z0,z1=zgrid[j:j+2];a=len(v);v.extend([(x,-.025,z0),(x,.025,z0),(x,.025,z1),(x,-.025,z1)]);f.append((a,a+1,a+2,a+3))
for z in(0,2.10):
 for j in range(len(xgrid)-1):
  x0,x1=xgrid[j:j+2];a=len(v);v.extend([(x0,-.025,z),(x1,-.025,z),(x1,.025,z),(x0,.025,z)]);f.append((a,a+1,a+2,a+3))
canvas=mesh('Solid door rails and stiles',v,f,PAINT,g);canvas['weighted']=True
for side in(-1,1):
 for x0,x1 in((.105,.400),(.500,.795)):
  for z0,z1 in((.15,.79),(.89,1.57),(1.69,1.98)):
   v=[];f=[]
   for inset,depth in((0,.025),(.004,.027),(.014,.015),(.020,.008)):
    v.extend([(x0+inset,side*depth,z0+inset),(x1-inset,side*depth,z0+inset),(x1-inset,side*depth,z1-inset),(x0+inset,side*depth,z1-inset)])
   for k in range(3):
    for j in range(4):q=(k*4+j,k*4+(j+1)%4,(k+1)*4+(j+1)%4,(k+1)*4+j);f.append(q if side<0 else q[::-1])
   f.append((12,13,14,15) if side<0 else (15,14,13,12))
   o=mesh('Recessed bevel moulding and panel',v,f,PANE,g);o['weighted']=True
# Thin eased painted perimeter detail remains close toleafcanvas and avoidsbulgingfaces.
for x in(.002,.898):cube('Door outer softened stile edge',(x,0,1.05),(.004,.050,2.10),PAINT,g,.0018,3)
for z in(.002,2.098):cube('Door outer softened rail edge',(.45,0,z),(.894,.050,.004),PAINT,g,.0018,3)
# Tallblack electronic escutcheon matchingreference; visible12-keynumerals.
cube('Charcoal electronic lock housing',(.847,-.037,1.139),(.078,.025,.360),DARK,g,.007,4)
cube('Raised numeric keypad bed',(.847,-.052,1.217),(.065,.006,.121),KEY,g,.002,3)
for row,digits in enumerate(('123','456','789','*0#')):
 for col,digit in enumerate(digits):
  x=.825+col*.022;z=1.256-row*.023
  cube('Inset rubber numeric key',(x,-.058,z),(.0175,.006,.018),KEY,g,.0025,3)
  label(digit,x,-.0615,z,.011,g)
# Small rounded statuswindowandsecurebottomkeycylinder.
cube('Keypad upper statuswindow',(.847,-.052,1.296),(.025,.003,.007),SCREW,g,.002)
cyl('Front mechanical key cylinder',(.847,-.053,.983),.010,.006,STEEL,g,'Y',24)
cube('Key cylinder keyslot',(.847,-.057,.983),(.002,.001,.012),SCREW,g)
# Leverfacebothsides; warmbrushedfinishatwellspacedfrontgrip.
for side in(-1,1):
 if side>0:cube('Inside electronic backplate',(.847,.035,1.055),(.067,.020,.194),DARK,g,.005,4)
 cyl('Lever circularhub',(.847,side*.056,1.019),.026,.022,STEEL,g,'Y',32)
 pts=[]
 for j in range(17):
  t=j/16;pts.append((.847-.126*t,side*(.069+.018*math.sin(t*math.pi/2)),1.019+.010*math.sin(t*math.pi*.8)))
 tube('Brushed inward door lever',pts,.010,STEEL,g,16)
 for z in(1.031,.998):cyl('Handlemount screw',(.847,side*.061,z),.0032,.002,SCREW,g,'Y',12)
# Movingleafhingebarrelsfollowhingeaxis; staticframeplateprovidedseparately.
for z in(.255,1.055,1.82):
 cyl('Dark cylindrical hinge knuckle',(0,0,z),.011,.110,SCREW,g,'Z',28)
 for zz in(z-.055,z+.055):cyl('Hinge pin cap',(0,0,zz),.012,.003,STEEL,g,'Z',24)
 cube('Leaf hinge mounting plate',(.021,-.021,z),(.040,.004,.094),SCREW,g,.0015,2)
 for zz in(z-.032,z+.032):cyl('Hinge small fastening head',(.021,-.024,zz),.0028,.0015,STEEL,g,'Y',12)
# Door latchbolt geometryatfreeedge, insettoleafnotframeblocking.
cube('Door edge steel strikeplate',(.901,0,1.019),(.003,.022,.092),STEEL,g,.001,2)
cube('Latch tongue',(.904,0,1.018),(.005,.015,.030),STEEL,g,.001,2)

# Matchingstaticjamb/casingwithclearopeningthroughit.
g='SM_3Doors_MatchingStaticFrame'
for x in(-.040,.940):
 cube('Painted jamb post',(x,0,1.055),(.070,.150,2.11),FRAME,g,.003,3)
 for side in(-1,1):
  cube('Casing vertical outertrim',(x,side*.083,1.055),(.075,.020,2.11),FRAME,g,.003,3)
  cube('Casing narrow edgebead',(x+.021,side*.095,1.055),(.009,.006,2.11),FRAME,g,.002,3)
 # Stoprabbetplanes oninsideofframe, outsideleafcanvasedges.
 stopx=-.001 if x<0 else .901
 cube('Jambdoor stop strip',(stopx,.042,1.055),(.010,.018,2.11),FRAME,g,.002,3)
# Upperlinteljamb,casing,copingbead.
cube('Upper door lintel',(.45,0,2.135),(.91,.15,.050),FRAME,g,.0025,3)
for side in(-1,1):
 cube('Casing horizontal header',(.45,side*.083,2.15),(1.055,.020,.080),FRAME,g,.003,3)
 cube('Casing horizontal finebead',(.45,side*.095,2.175),(1.055,.006,.009),FRAME,g,.002,3)
for z in(.255,1.055,1.82):
 cube('Fixedframe hinge mounting plate',(-.020,-.021,z),(.040,.004,.094),SCREW,g,.0015,2)
 for zz in(z-.032,z+.032):cyl('Fixedhinge fastening screw',(-.020,-.024,zz),.0028,.0015,STEEL,g,'Y',12)
# Leaf latchstrikeholeframedonthandlebeingclosecollide.
cube('Fixedframe silver striker',(.902,-.008,1.019),(.002,.027,.104),STEEL,g,.001,2)
cube('Fixedframe black strike recess',(.900,-.008,1.019),(.001,.012,.033),SCREW,g)

# SimpleCOLposts/lintelleave163.8cmwideinstalledopeningclear.
g='COL_3Doors_MatchingStaticFrame'
for x in(-.040,.940):cube('Static frame post blocker',(x,0,1.055),(.070,.15,2.11),COL,g)
cube('Static frame lintel blocker',(.45,0,2.135),(.91,.15,.050),COL,g)

def finish(name,objects):
 for o in objects:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);o.select_set(False)
  bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='FIXED',ngon_method='BEAUTY');small=[f for f in bm.faces if f.calc_area()<1e-10]
  if small:bmesh.ops.delete(bm,geom=small,context='FACES_ONLY')
  bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(o.data);bm.free();o.data.update()
  if o.get('weighted'):
   o.select_set(True);bpy.context.view_layer.objects.active=o;wm=o.modifiers.new('Baked smoothhardedge normals','WEIGHTED_NORMAL');wm.weight=50;wm.keep_sharp=True;wm.thresh=.01;bpy.ops.object.modifier_apply(modifier=wm.name);o.select_set(False)
   nn=[q.vector.copy() for q in o.data.corner_normals]
   for p in o.data.polygons:
    if p.area>.0003 and max(abs(v) for v in p.normal)>.99999:
     for li in p.loop_indices:nn[li]=p.normal.copy()
   o.data.normals_split_custom_set(nn)
  if not o.data.uv_layers:
   uv=o.data.uv_layers.new(name='UVMap')
   for p in o.data.polygons:
    axis=max(range(3),key=lambda k:abs(p.normal[k]));aa,bb=((1,2),(0,2),(0,1))[axis]
    for li in p.loop_indices:
     q=o.data.vertices[o.data.loops[li].vertex_index].co
     uv.data[li].uv=(q.x/.9,q.z/2.10) if axis==1 else (q[aa],q[bb])
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0]
 if len(objects)>1:bpy.ops.object.join()
 o=bpy.context.object;o.name=name;o.data.name=name;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 path=MODEL+'/'+name+'.fbx';bpy.ops.export_scene.fbx(filepath=path,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',use_mesh_modifiers=True,bake_space_transform=False,add_leaf_bones=False)
 v=[o.matrix_world@q.co for q in o.data.vertices];lo=[min(q[k] for q in v) for k in range(3)];hi=[max(q[k] for q in v) for k in range(3)]
 row={'name':name,'fbx':path,'triangles':len(o.data.polygons),'vertices':len(o.data.vertices),'boundsMetres':[lo,hi],'dimensionsCm':[(hi[k]-lo[k])*100 for k in range(3)],'origin':'Local hinge floor(0,0,0); apply root scale1.1 in map','front':'Unreal+Y;leafextends+X;hingeaxis+Z','materials':[m.name for m in o.data.materials],'hasCustomNormals':o.data.has_custom_normals,'collision':'Hidden BlockAll complex as simple frame only' if name.startswith('COL_') else 'NoCollision'}
 row['boundsUnrealCm']=[[100*lo[0],-100*hi[1],100*lo[2]],[100*hi[0],-100*lo[1],100*hi[2]]];manifest['assets'].append(row);o.select_set(False)
 if name.startswith('COL_'):o.hide_render=True;o.hide_set(True)
 return o
# Separate101/102leaves andframes keep sourcehinges/doorwidths exact.
NUMBER=mat('BlackRoomNumbers',(.004,.005,.006),.55)
SCUFF=mat('PaintScuff',(.43,.42,.37),.88)
CHIP=mat('OldPaintChip',(.45,.35,.23),.94)
PLAQUE=mat('PlaqueWarmWood',(.16,.095,.042),.67)
FACE=mat('PlaqueBlackFace',(.008,.009,.010),.42)
FONT=bpy.data.fonts.load('/System/Library/Fonts/Supplemental/Times New Roman Italic.ttf')

def number_geometry(number,group):
 d=bpy.data.curves.new('Room '+str(number)+' black serif numerals','FONT');d.body=' '.join(str(number));d.size=.155;d.align_x='CENTER';d.align_y='CENTER';d.extrude=.0004;d.resolution_u=6;d.font=FONT
 o=bpy.data.objects.new('Black individually spaced room numerals',d);bpy.context.collection.objects.link(o);o.location=(.45,-.0276,1.63);o.rotation_euler=(math.pi/2,0,0);d.materials.append(NUMBER)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH');G[group].append(bpy.context.object)

def wear_geometry(number,group):
 rr=random.Random(number)
 # Irregular small missingpaintpatches onfaces; largebottomscuffs are texturebased.
 patches=[(.245,.46,.007,.016),(.71,1.38,.008,.014),(.585,1.16,.006,.009),(.32,.19,.012,.005)] if number==102 else [(.455,1.31,.007,.014),(.65,.98,.007,.009),(.77,.58,.005,.008),(.13,.20,.009,.014)]
 for i,(x,z,rx,rz) in enumerate(patches):
  pts=[]
  for j in range(7):
   angle=j*math.tau/7;shape=rr.uniform(.68,1.15);pts.append((x+rx*math.cos(angle)*shape,-.028,z+rz*math.sin(angle)*shape))
  mesh('Small irregular creampaintchip '+str(i),pts,[tuple(range(7))],CHIP,group)
 for i in range(22 if number==102 else 12):
  x=rr.uniform(.10,.80);z=rr.uniform(.025,.13);length=rr.uniform(.005,.019)
  pts=[(x,-.0255,z),(x+length,-.0255,z+.001),(x+length*.6,-.0255,z+.002),(x,-.0255,z+.0014)]
  mesh('Fine doorbottomkickmark '+str(i),pts,[(0,1,2,3)],SCUFF,group)

base_leaf=G['SM_3Doors_RecessedSixPanelLeaf'];base_frame=G['SM_3Doors_MatchingStaticFrame'];base_col=G['COL_3Doors_MatchingStaticFrame']
new_groups={}
for number,width in ((101,144.0),(102,169.2)):
 leafgroup='Door_'+str(number);G[leafgroup]=[]
 for original in base_leaf:
  copy=original.copy();copy.data=original.data.copy();bpy.context.collection.objects.link(copy);G[leafgroup].append(copy)
 number_geometry(number,leafgroup);wear_geometry(number,leafgroup)
 groups=[('SM_R101Entry_Door'+str(number)+'Leaf',G[leafgroup]),('SM_R101Entry_Frame'+str(number),base_frame),('COL_R101Entry_Frame'+str(number),base_col)]
 for name,objects in groups:
  copies=[]
  for original in objects:
   copy=original.copy();copy.data=original.data.copy();bpy.context.collection.objects.link(copy)
   # UVs laid out onoriginalcanvas before width/height fit. hardware shares root scale.
   if not copy.data.uv_layers:
    uv=copy.data.uv_layers.new(name='UVMap')
    for p in copy.data.polygons:
     for li in p.loop_indices:
      q=copy.matrix_world@copy.data.vertices[copy.data.loops[li].vertex_index].co;uv.data[li].uv=(q.x/.9,q.z/2.10)
   matrix=copy.matrix_world.copy()
   for v in copy.data.vertices:
    p=matrix@v.co;v.co=(p.x*(width/90),p.y,p.z*(199.75/210))
   copy.location=(0,0,0);copy.rotation_euler=(0,0,0);copy.scale=(1,1,1);copies.append(copy)
  new_groups[name]=copies

# Blank blacknameplaque inwoodframe, independent wallpiece like thephoto.
g='SM_R101Entry_BlankNamePlaque'
cube('Small wood donornameplate',(0,0,0),(.242,.017,.110),PLAQUE,g,.0025)
cube('Black inset blank plaque',(0,-.010,0),(.213,.003,.076),FACE,g,.001)
for x in(-.115,.115):
 for z in(-.047,.047):cyl('Small nameplatefixing screw',(x,-.009,z),.0015,.001,STEEL,g,'Y',10)
new_groups[g]=G[g]
for name,objects in new_groups.items():finish(name,objects)
manifest['textures']=['T_R101Entry_OffWhitePaint_BaseColor.png','T_R101Entry_OffWhitePaint_Roughness.png']
manifest['doors']={
 '101':{'leaf':'SM_R101Entry_Door101Leaf','frame':'SM_R101Entry_Frame101','frame_collision':'COL_R101Entry_Frame101','widthCm':144.0,'heightCm':199.75,'panelCenterLocalCm':[72.0,0,99.875],'panelHalfExtentLocalCm':[72.0,2.5,99.875],'closedYaw':-90,'openYaw':-180,'frameYaw':-90,'front':'Local+Y turnsworld+X facinghall'},
 '102':{'leaf':'SM_R101Entry_Door102Leaf','frame':'SM_R101Entry_Frame102','frame_collision':'COL_R101Entry_Frame102','widthCm':169.2,'heightCm':199.75,'panelCenterLocalCm':[84.6,0,99.875],'panelHalfExtentLocalCm':[84.6,2.5,99.875],'closedYaw':0,'openYaw':-90,'frameYaw':0,'front':'Local+Y hallface'}
}
manifest['placementScale']=1.1
manifest['totalVisualTriangles']=sum(a['triangles'] for a in manifest['assets'] if a['name'].startswith('SM_'))
json.dump(manifest,open(R+'/door_manifest.json','w'),indent=2)
# Hideconstructionprototypes in authoringscene; finishedexports are visibleseparately.
exported=set(new_groups)
for o in bpy.context.scene.objects:
 if o.type=='MESH' and o.name not in exported:o.hide_render=True;o.hide_set(True)
bpy.ops.wm.save_as_mainfile(filepath=R+'/Room101EntryDoors_source.blend')
print('ROOM101_ENTRY_ART_READY',len(manifest['assets']),'assets',manifest['totalVisualTriangles'],'visualtriangles')
