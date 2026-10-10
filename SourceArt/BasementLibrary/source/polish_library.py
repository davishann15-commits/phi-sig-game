"""Add the small reference details, without rebuilding the installed room shell."""
import json,math
from pathlib import Path
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/basement-library')
saved=json.loads((R/'manifest.json').read_text())
exec(compile((R/'source/build_library.py').read_text().split('def swivel(')[0],str(R/'source/build_library.py'),'exec'))
orange=mat('OrangeBadge',(.8,.20,.012),.91)
charcoal=mat('CharcoalCloth',(.023,.027,.029),.97)
glass=mat('JarGlass',(.50,.61,.59),.17)
manifest['materials']['M_Lib_JarGlass']['opacity']=.20
manifest['materials']['M_Lib_IvoryPaint']['base_color']=[.62,.58,.47]

# The paired wells are actual holes through the octagonal table and rails.
g='OctagonalCardTable'
base=cyl('octagonal oak tabletop',(0,0,.732),.82,.06,wood,g,vertices=8)
felt=cyl('raised patterned felt',(0,0,.770),.625,.018,greencloth,g,vertices=8)
for loop in felt.data.uv_layers.active.data:loop.uv*=2.8
for a in range(8):
 t=a*math.tau/8+math.pi/8;x=math.cos(t)*.71;y=math.sin(t)*.71
 rail=box('rounded cup rail',(x,y,.775),(.285,.139,.033),wood,g,.007);rail.rotation_euler.z=t+math.pi/2
 for off in [-.054,.054]:
  cx=x-math.sin(t)*off;cy=y+math.cos(t)*off
  bpy.ops.mesh.primitive_cylinder_add(vertices=32,radius=.032,depth=.17,location=(cx,cy,.750));cut=bpy.context.object;cut.data.materials.append(wood)
  for obj in [base,rail]:
   bpy.context.view_layer.objects.active=obj
   modifier=obj.modifiers.new('Recessed cup socket','BOOLEAN');modifier.operation='DIFFERENCE';modifier.solver='EXACT';modifier.object=cut
   bpy.ops.object.modifier_apply(modifier=modifier.name)
  bpy.data.objects.remove(cut,do_unlink=True)
  # Hollow, dark cylindrical liner and a lower bottom, not a disk on top.
  n=32;v=[(cx+r*math.cos(i*math.tau/n),cy+r*math.sin(i*math.tau/n),z)for z,r in [(.722,.031),(.791,.031)]for i in range(n)]
  mesh('socket inner liner',v,[(i,(i+1)%n,(i+1)%n+n,i+n)for i in range(n)],plastic,g,True)
  cyl('well bottom',(cx,cy,.723),.031,.003,plastic,g,vertices=32)
for x in [-.43,.43]:
 for y in [-.43,.43]:box('game-table leg',(x,y,.35),(.045,.045,.70),wood,g,.005)
box('leg crossbar',(0,0,.18),(.91,.045,.035),wood,g,.003)
finalize(g,85,95)

for g,m in [('DarkThrowPillow',charcoal),('OrangeThrowPillow',graycloth)]:
 box('soft cushion',(0,0,.065),(.48,.44,.13),m,g,.055,7)
 tube('stitched cushion seam',[(.24*math.cos(a),.22*math.sin(a),.065)for a in [i*math.tau/48 for i in range(48)]],.0018,m,g,closed=True)
 if g=='OrangeThrowPillow':
  cyl('orange woven badge',(0,0,.131),.13,.001,orange,g,vertices=48)
  text('white T badge','T',(0,-.059,.133),.14,paper,g,rotation=(0,0,0))
 finalize(g,2,12)

g='ShelfFramedPhoto';w=.19;h=.14
photomat=mat('ShelfPhoto',(1,1,1),.91,tex=R/'references/IMG_2272.png')
box('black photograph frame',(0,0,h/2),(w,.019,h),plastic,g,.003)
uv=[(350/1125,1-649/1500),(478/1125,1-635/1500),(460/1125,1-550/1500),(341/1125,1-571/1500)]
flat('supplied shelf photograph',(0,-.010,h/2),w-.013,h-.013,photomat,g,True,uv)
tube('rear picture stand',[(0,.007,.115),(0,.061,.009),(0,.005,.009)],.004,plastic,g)
finalize(g,1,14)

g='RosterFrame';w=.26;h=.33
box('roster frame',(0,0,h/2),(w,.018,h),plastic,g,.004)
flat('cream roster page',(0,-.010,h/2),w-.022,h-.02,paper,g,True)
text('roster title','MEMBERS',(0,-.011,h-.05),.017,black,g)
for k in range(12):
 for s in [-1,1]:box('printed roster name',(s*.064,-.011,h-.079-k*.015),(.091,.0006,.0027),black,g,.0001)
finalize(g,2,18)

g='GlassJar';N=40
profile=[(.0,.033),(.008,.038),(.092,.038),(.112,.033),(.125,.032),(.125,.028),(.114,.028),(.094,.034),(.011,.034)]
v=[(r*math.cos(i*math.tau/N),r*math.sin(i*math.tau/N),z)for z,r in profile for i in range(N)]
f=[(k*N+i,k*N+(i+1)%N,(k+1)*N+(i+1)%N,(k+1)*N+i)for k in range(len(profile)-1)for i in range(N)]
mesh('open glass jar',v,f,glass,g,True)
for z in [.113,.118,.123]:tube('threaded jar rim',[(.033*math.cos(i*math.tau/N),.033*math.sin(i*math.tau/N),z)for i in range(N)],.0013,glass,g,closed=True)
cyl('jar base',(0,0,.007),.034,.004,glass,g,vertices=40)
finalize(g,1,12)

g='LoosePaperStack'
for k in range(8):
 o=box('loose paper',(0,0,.001+k*.001),(.22,.15,.0008),paper,g,.0002);o.rotation_euler.z=(k%3-1)*.023
flat('top typed page',(0,0,.009),.22,.15,notes,g)
finalize(g,1,12)

g='RedGameBookend'
box('red wooden die',(0,0,.05),(.097,.097,.097),red,g,.007)
for x,z in [(-.025,.025),(.025,.025),(0,.05),(-.025,.075),(.025,.075)]:cyl('die white pip',(x,-.050,z),.008,.001,paper,g,axis='Y',vertices=16)
finalize(g,2,15)

for a in manifest['assets']:
 saved['assets']=[old for old in saved['assets'] if old['id']!=a['id']]+[a]
saved['materials'].update(manifest['materials'])

def add(g,u,v,h=0,yaw=0,**extra):
 p=dict(asset=g,name=extra.pop('name',g),location=[round(-3004.2+u*180,3),round(773.1-v*180,3),round(-329.06+h*100,3)],yaw=yaw,**extra);saved['placements'].append(p)
new_ids={'ShelfFramedPhoto','RosterFrame','GlassJar','LoosePaperStack','RedGameBookend'}
saved['placements']=[p for p in saved['placements'] if p['asset'] not in new_ids]
for side,vs in [(0,[3.95,8.13,10.45]),(1,[2.27,7.9,10.15])]:
 for i,v in enumerate(vs):
  u=.155 if side==0 else 3.30;yaw=-90 if side==0 else 90
  # Stacked-book shelves have room at their sides for the photographed frames.
  add('ShelfFramedPhoto',u+(.055 if side==0 else -.055),v+.25,.331,yaw,roll=-9,name='Small framed shelf photograph')
  add('ShelfFramedPhoto',u+(.055 if side==0 else -.055),v-.25,1.241,yaw,roll=-9,name='Small framed shelf photograph')
  add('LoosePaperStack',u,v+.15,.032,yaw)
  if i==2:add('RedGameBookend',u,v-.25,1.571,yaw)
add('GlassJar',3.20,4.06,.572)
add('RosterFrame',3.22,11.04,.574,90,roll=-12,name='Framed membership roster beside drawers')
add('LoosePaperStack',3.22,5.10,.572)
add('LoosePaperStack',.155,8.13,.931,-90)
for p in saved['placements']:
 if p['asset'] in ['DarkThrowPillow','OrangeThrowPillow'] and p['location'][1]<0:
  p['roll']=-62;p['location'][2]=-262.06;p['location'][0]-=9
saved['refresh_assets']=[a['id'] for a in manifest['assets']]
saved['refresh_materials']=['M_Lib_IvoryPaint']
saved['refresh_textures']=['Walnut','Walnut_Roughness','Oak','Oak_Roughness','DamaskRug']
(R/'manifest.json').write_text(json.dumps(saved,indent=2))
print('LIBRARY_POLISHED',len(saved['assets']),len(saved['placements']),flush=True)
