import bpy,bmesh,runpy,json,math,collections,random
from pathlib import Path
from mathutils import Vector,Matrix
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs');D=R/'models';DEST='/Game/SeniorSendOff/House/GreatRoomTVs'
DM=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/generate_damage_v2.py');G=collections.defaultdict(list);manifest=dict(materials={},assets=[],placements=[]);M={}
bpy.ops.wm.read_factory_settings(use_empty=True)
def mat(k,c,rough,metal=0,texture=None,emission=0):
 name='M_GRTV_'+k;m=bpy.data.materials.new(name);m.use_nodes=True;m.diffuse_color=(*c,1);p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 d=dict(base_color=c,roughness=rough,metallic=metal)
 if texture:
  file=str(R/'textures'/texture);d['texture']=file;node=m.node_tree.nodes.new('ShaderNodeTexImage');node.image=bpy.data.images.load(file);m.node_tree.links.new(node.outputs['Color'],p.inputs['Base Color'])
 if emission:d['emission']=emission
 M[k]=m;manifest['materials'][name]=d;return m
plastic=mat('GraphitePlastic',(.015,.018,.022),.39);rear=mat('RearTexturedPlastic',(.008,.010,.013),.62);metal=mat('AnodizedAluminum',(.070,.076,.085),.30,.86);chrome=mat('ScrewSteel',(.21,.23,.25),.26,.9);rubber=mat('Rubber',(.005,.006,.008),.91);copper=mat('CopperContacts',(.40,.22,.078),.3,.8);board=mat('CircuitBoard',(.013,.06,.043),.69);silver=mat('PrintedLabel',(.32,.36,.36),.56);led=mat('PowerLED',(.3,.5,.55),.2,emission=1.4)
screen=[mat('Screen'+str(i),(1,1,1),.20 if i<2 else .35,.12,'Screen'+str(i)+'.png',.50 if i<2 else .28 if i==2 else 0)for i in range(4)]
exec(compile(Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen/source/helpers.py').read_text(),'<shared-geometry>','exec'))
def export(g,name,collision=False):
 parts=G[g]
 for o in parts:
  bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);uv_auto(o)
 mats=[]
 for o in parts:
  for m in o.data.materials:
   if m not in mats:mats.append(m)
 for o in parts:
  old=list(o.data.materials);indices=[mats.index(old[p.material_index])for p in o.data.polygons];o.data.materials.clear()
  for m in mats:o.data.materials.append(m)
  for p,i in zip(o.data.polygons,indices):p.material_index=i
 ob=DM['join_meshes'](parts,name,mats);hulls=[]
 if collision:
  # One fitted chassis body avoids tiny detail hulls catching on the mantel.
  h=box('UCX_'+name+'_00',(0,.003,0),(1.84,.085,1.065),None,'temp',bevel=0);hulls=[h];G.pop('temp')
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True)
 for h in hulls:h.select_set(True)
 bpy.context.view_layer.objects.active=ob;file=D/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False,mesh_smooth_type='FACE')
 lo,hi=DM['bbox'](ob);manifest['assets'].append(dict(name=name,fbx=str(file),materials=[DEST+'/'+m.name+'.'+m.name for m in mats],material_names=[m.name for m in mats],bounds=[list(lo),list(hi)],faces=len(ob.data.polygons),collision=collision))
 for o in [ob,*hulls,*parts]:DM['discard'](o)
 G.pop(g,None)
for stage in range(4):
 g='TV'+str(stage)
 box('Slim aluminum screen surround',(0,-.018,0),(1.84,.044,1.065),metal,g,.007,4)
 box('Injection molded rear enclosure',(0,.023,0),(1.79,.039,1.017),rear,g,.012,4)
 box('Recessed front panel',(0,-.041,0),(1.812,.003,1.030),rubber,g,.003)
 flat('LCD picture',(0,-.0431,.006),1.800,1.0125,screen[stage],g,vertical=True)
 for x in [-.916,.916]:box('Soft edge bezel',(x,-.036,0),(.008,.01,1.042),plastic,g,.002)
 box('Lower speaker bezel',(0,-.043,-.515),(1.824,.006,.028),plastic,g,.002)
 for i in range(60):
  x=-.50+i/59*1.0;box('Speaker grille slot',(x,-.047,-.515),(.003,.001,.010),rear,g,.0004,1)
 text('Fine front wordmark','SENIOR',(0,-.048,-.519),.009,silver,g)
 cyl('Status light',(.795,-.048,-.515),.0025,.002,led if stage<2 else rubber,g,16,'Y')
 # Back ribs, vent apertures, screw wells and recessed VESA attachment points.
 for side in [-1,1]:
  for i in range(18):box('Rear ventilation aperture',(side*(.45+i*.018),.044,.365),(.010,.002,.078),rubber,g,.002)
 for x in [-.70,-.2,.2,.70]:
  for z in [-.44,.44]:
   cyl('Screw recessed socket',(x,.045,z),.006,.002,rubber,g,16,'Y');cyl('Phillips steel screw',(x,.046,z),.0038,.001,chrome,g,12,'Y');box('Phillips cross',(x,.047,z),(.005,.001,.0007),rear,g,.0001,1)
 for x in [-.2,.2]:
  for z in [-.2,.2]:cyl('VESA threaded mount hole',(x,.046,z),.007,.002,rubber,g,16,'Y')
 box('Port recess',(.45,.046,-.18),(.40,.001,.24),rubber,g,.004)
 for i in range(3):
  box('HDMI metal port',(.32+i*.075,.049,-.12),(.045,.006,.016),chrome,g,.002);box('HDMI contact slot',(.32+i*.075,.053,-.12),(.036,.002,.006),rubber,g,.001)
 box('USB port',(.34,.049,-.21),(.027,.006,.012),chrome,g,.001);box('Ethernet socket',(.42,.050,-.21),(.026,.007,.024),plastic,g,.001)
 for x in [.41,.414,.418,.422,.426,.43]:box('Ethernet gold contact',(x,.054,-.213),(.001,.001,.01),copper,g,.0001,1)
 cyl('Antenna connector',(.51,.052,-.22),.009,.014,chrome,g,16,'Y')
 box('Rear certification label',(-.30,.044,-.32),(.26,.001,.08),silver,g,.002)
 text('Certification print','SENIOR  /  4K UHD',(-.30,.045,-.31),.010,rear,g,rotation=(-math.pi/2,0,0))
 text('Serial print','SS-84   110-240V',(-.30,.045,-.337),.007,rear,g,rotation=(-math.pi/2,0,0))
 for i in range(5):box('Rear tactile buttons',(.82,.027,-.33+i*.028),(.018,.012,.017),plastic,g,.003)
 tube('Attached power lead',[(.58,.041,-.34),(.60,.052,-.40),(.62,.055,-.49),(.61,.053,-.56),(.57,.048,-.61)],.004,rubber,g)
 if stage>=2:
  # Casing breaks expose metal backing and traces; no flattened replacement.
  chip=box('Exposed corner shielding',(.855,-.046,.488),(.090,.006,.036),chrome,g,.001)
  tube('Chipped upper bezel seam',[(.81,-.049,.529),(.83,-.05,.513),(.848,-.052,.530),(.884,-.051,.517),(.916,-.049,.526)],.0017,rubber,g)
 if stage==3:
  box('Exposed fractured PCB',(.84,.048,.45),(.115,.004,.065),board,g,.001)
  for j in range(5):box('Visible copper traces',(.80+j*.013,.051,.45),(.001,.001,.046),copper,g,.0001,1)
  # A bent top corner remains attached to the rigid chassis while it falls.
  for o in G[g]:
   if o.name.startswith(('Slim aluminum','Soft edge','Injection molded')):
    bpy.context.view_layer.update()
    for v in o.data.vertices:
     world=o.matrix_world@v.co
     if world.x>.77 and world.z>.45:
      world.y-=.025*(world.x-.77)/.15;world.z-=.014*(world.x-.77)/.15;v.co=o.matrix_world.inverted()@world
 export(g,'SM_GRTV_Television_'+str(stage),True)
g='Mount'
box('Flush wall mount plate',(0,0,0),(.65,.012,.26),metal,g,.007)
for x in [-.225,.225]:
 box('Vertical adjustable rail',(x,-.025,0),(.045,.042,.42),rear,g,.004)
 for z in [-.16,.16]:cyl('Visible mounting bolt',(x,-.049,z),.008,.010,chrome,g,6,'Y')
for x in [-.28,.28]:
 for z in [-.09,.09]:cyl('Anchor bolt',(x,-.008,z),.007,.014,chrome,g,6,'Y')
export(g,'SM_GRTV_WallBracket',False)
manifest['placements']=[{'id': 'GreatRoom_WestTV', 'location': [-1136.0, 189.8999486, 251.0], 'yaw': -90, 'scale': [1.65, 1, 1.65], 'mount_location': [-1149.24, 189.8999486, 251.0], 'mount_scale': [1.65, 1.6, 1.65]}, {'id': 'GreatRoom_EastTV', 'location': [1235.0, 169.1999073, 251.0], 'yaw': 90, 'scale': [1.65, 1, 1.65], 'mount_location': [1248.24, 169.1999073, 251.0], 'mount_scale': [1.65, 1.6, 1.65]}]
(R/'manifest.json').write_text(json.dumps(manifest,indent=2));print('TV_MODELS_READY',len(manifest['assets']))
