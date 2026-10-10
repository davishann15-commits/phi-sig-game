"""Two club finishing assets. Offline Blender only, does not edit Unreal.
Run Blender --background --python thisfile. Front is Unreal +Y.
"""
from pathlib import Path
SRC=Path(__file__).with_name('club_assets.py').read_text()
# Reuse the stable art helpers without executing or replacing the fourteen main assets.
exec(SRC[:SRC.index('for name,text,ma,h in')])
exec(SRC[SRC.index('def finish(name,objects):'):SRC.index('OBJS={}')])
LEATHER=mat('SoftBlackLeather',(.018,.022,.027),.38,.02)
SEAM=mat('BlackLeatherSeam',(.006,.008,.01),.66)
PAINT=mat('EntranceCharcoal',(.027,.033,.041),.46,.12)
PANEL=mat('EntrancePanelInset',(.012,.018,.024),.54,.08)
# Leather sofa, 3 metres wide. Floor pivot, audience front is Blender -Y.
g='SM_NC_ClubSofa'
for x in(-1.33,1.33):
 for y in(-.34,.34):
  cyl('Brushed metal tapered sofa foot',(x,y,.075),.029,.15,ALU,g,24)
  cyl('Rubber sofa floor protector',(x,y,.009),.027,.018,RUBBER,g,24)
cube('Black upholstered platform',(0,0,.235),(2.89,.83,.21),LEATHER,g,.055,6)
cube('Sofa bottom metal support rail',(0,-.409,.20),(2.81,.022,.044),ALU,g,.009,3)
for x in(-1.40,1.40):
 cube('Plump upholstered sofa arm',(x,.007,.455),(.20,.96,.53),LEATHER,g,.075,7)
 tube('Arm top leather welt',[(x-.07,-.34,.699),(x-.07,.36,.699),(x+.07,.36,.699),(x+.07,-.34,.699),(x-.07,-.34,.699)],.0035,SEAM,g,8)
cube('Soft sofa back structure',(0,.343,.550),(2.78,.215,.59),LEATHER,g,.060,7)
# Three separate seat cushions, shaped rounded edge, seam welt and natural seated dip.
for i,x in enumerate((-.865,0,.865)):
 cube('Individual plush seat cushion '+str(i),(x,-.095,.400),(.84,.67,.175),LEATHER,g,.065,7)
 tube('Seat cushion stitched leather piping '+str(i),[(x-.342,-.432,.407),(x+.342,-.432,.407),(x+.392,-.382,.407),(x+.392,.190,.407),(x+.342,.235,.407),(x-.342,.235,.407),(x-.392,.190,.407),(x-.392,-.382,.407),(x-.342,-.432,.407)],.003,SEAM,g,8)
# Real button depressions on back cushions; shaped geometry, not painted circles.
def tufted_back(x,width=.865,height=.398):
 nx,nz=38,24;v=[];f=[];cy=.214;cz=.642
 for back in (False,True):
  for j in range(nz+1):
   t=j/nz;z=cz+(t-.5)*height
   for k in range(nx+1):
    s=k/nx;xx=x+(s-.5)*width
    edge=(math.sin(math.pi*s)*math.sin(math.pi*t))**.35
    y=cy+(.11 if back else -.04*edge)
    if not back:
     for bx in (.27,.73):
      for bz in (.32,.72):y+=.019*math.exp(-(((s-bx)/.05)**2+((t-bz)/.075)**2))
    y+=(z-cz)*.16
    v.append((xx,y,z))
 n=(nx+1)*(nz+1)
 for j in range(nz):
  for k in range(nx):
   a=j*(nx+1)+k;f.append((a,a+1,a+nx+2,a+nx+1));f.append((n+a+nx+1,n+a+nx+2,n+a+1,n+a))
 for k in range(nx):f.append((k,n+k,n+k+1,k+1));a=nz*(nx+1)+k;f.append((a+1,n+a+1,n+a,a))
 for j in range(nz):
  a=j*(nx+1);f.append((a+nx+1,n+a+nx+1,n+a,a));a+=nx;f.append((a,n+a,n+a+nx+1,a+nx+1))
 mesh('Contoured tufted back cushion',v,f,LEATHER,g,True)
 for bx in(.27,.73):
  for bz in(.32,.72):
   xx=x+(bx-.5)*width;zz=cz+(bz-.5)*height;edge=(math.sin(math.pi*bx)*math.sin(math.pi*bz))**.35;yy=cy-.04*edge+.019+(zz-cz)*.16
   o=ball('Recessed upholstered tuft button',(xx,yy-.002,zz),.011,SEAM,g,16,8);o.scale.y=.25
 for z in(cz-height/2+.012,cz+height/2-.012):
  yy=cy+(z-cz)*.16-.001
  tube('Back cushion edge double stitching',[(x-width/2+.014,yy,z),(x+width/2-.014,yy,z)],.0025,SEAM,g,8)
for x in(-.876,0,.876):tufted_back(x)
# A few fine seams define the backs of the arms and the upholstered base.
for z in(.215,.308):tube('Sofa lower leather seam',[(-1.32,-.420,z),(1.32,-.420,z)],.0025,SEAM,g,8)
# Club entrance leaf. Already final centimetre dimensions: root actor scale = 1.
g='SM_NC_ClubEntranceDoor'
cube('Solid six panel charcoal club door',(.79,0,1.095),(1.58,.05,2.19),PAINT,g,.010,4)
for sign in(-1,1):
 for x in(.404,1.176):
  for z,h in((.29,.37),(.86,.55),(1.64,.62)):
   cube('Silver grey recessed panel bevel',(x,sign*.029,z),(.612,.013,h),PAINT,g,.025,5)
   cube('Dark recessed panel field',(x,sign*.038,z),(.548,.008,h-.066),PANEL,g,.014,4)
 # Both sides carry practical escutcheon, stem and shaped brushed lever.
 cube('Satin lever rectangular backplate',(1.43,sign*.037,1.03),(.068,.020,.166),ALU,g,.010,5)
 cyl('Lever circular hub',(1.43,sign*.055,1.03),.028,.042,CHROME,g,32,'Y')
 tube('Rounded satin metal door lever',[(1.43,sign*.080,1.03),(1.395,sign*.100,1.026),(1.290,sign*.100,1.026),(1.259,sign*.093,1.041)],.012,ALU,g,16)
 for z in(.970,1.089):
  cyl('Lever backplate real fastening screw',(1.43,sign*.049,z),.005,.005,CHROME,g,16,'Y')
  cube('Backplate screw slot',(1.43,sign*.053,z),(.005,.001,.0015),BLACK,g)
 cyl('Thumb turn latch face',(1.43,sign*.054,.992),.012,.009,BLACK,g,24,'Y')
 cube('Latch thumbturn paddle',(1.43,sign*.064,.992),(.021,.006,.009),ALU,g,.003,3)
# Hinge knuckles and plates extend subtly from the true origin at the door edge.
for z in(.24,1.095,1.95):
 cube('Door side steel hinge leaf',(.028,0,z),(.070,.056,.11),ALU,g,.004,3)
 for k in range(5):cyl('Hinge interlocking knuckle',(-.006,0,z-.044+k*.022),.012,.019,CHROME,g,24)
 cyl('Hinge pin cap',(-.006,0,z+.059),.013,.009,ALU,g,24)
# A small single stroke neon word at eye height; honest emissive tubes.
FONT['C']=[circ(.5,.5,.5,.5,.23,math.tau-.23,30)]
FONT['B']=[[(0,0),(0,1),(.55,1)]+circ(.55,.75,.4,.25,math.pi/2,-math.pi/2,18)[1:]+[(0,.5),(.55,.5)]+circ(.55,.25,.4,.25,math.pi/2,-math.pi/2,18)[1:]+[(0,0)]]
old=list(G[g]);neon_text('CLUB',g,CYAN,.145,1.62)
for o in G[g]:
 if o not in old:o.location.x+=.79;o.location.y-=.018
cube('Club label dark backing',(.79,-.028,1.62),(.637,.010,.218),BLACK,g,.015,4)
OBJS={}
for name,objects in list(G.items()):OBJS[name]=finish(name,objects)
MAN['notes']=['These two assets are authored at final world size; place with actor scale 1.','Sofa front Unreal +Y. Sofa collision should use one compact box.','Door hinge at local origin, closed leaf spans +X. Real geometry dimensions 158 by 219 centimetres.']
with open(R+'/addendummanifest.json','w') as f:json.dump(MAN,f,indent=2)
bpy.ops.wm.save_as_mainfile(filepath=R+'/NightClub_extra_source.blend')
# Small product preview supporting visual quality; no map lighting is changed.
for o in OBJS.values():o.hide_render=True
so=OBJS['SM_NC_ClubSofa'].copy();so.data=OBJS['SM_NC_ClubSofa'].data;bpy.context.collection.objects.link(so);so.hide_render=False
leaf=OBJS['SM_NC_ClubEntranceDoor'].copy();leaf.data=OBJS['SM_NC_ClubEntranceDoor'].data;bpy.context.collection.objects.link(leaf);leaf.hide_render=False;leaf.location=(2.0,.30,0)
cube('Preview floor',(0,0,-.04),(12,9,.08),BLACK,'__PREVIEW__',.01,3)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24;scene.render.resolution_x=1300;scene.render.resolution_y=800;scene.render.resolution_percentage=100;scene.world.color=(.04,.045,.06)
for loc,power,col in (((-3,-3,4),650,(.5,.7,1)),((3,-1,4),600,(1,.7,.52)),((0,3,3),500,(.7,.8,1))):
 d=bpy.data.lights.new('Soft preview studio','AREA');d.energy=power;d.color=col;d.shape='DISK';d.size=4;o=bpy.data.objects.new('Soft preview studio',d);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,.6))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Preview camera');o=bpy.data.objects.new('Preview camera',d);bpy.context.collection.objects.link(o);o.location=(5.7,-7.9,3.4);o.rotation_euler=(Vector((.70,0,.95))-o.location).to_track_quat('-Z','Y').to_euler();d.lens=46;scene.camera=o;scene.render.filepath=R+'/club-extra-assets-preview.png';bpy.ops.render.render(write_still=True)
print('NIGHTCLUB_EXTRA_ASSETS_PASS assets='+str(len(MAN['assets']))+' triangles='+str(sum(a['triangles'] for a in MAN['assets'])))
