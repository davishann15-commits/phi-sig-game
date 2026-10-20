"""Read exported Unreal binary FBX geometry without launching an editor.

Outputs are analysis data only. Mesh vertices are converted from FBX's Y-left
export convention to Unreal Y-right and then transformed by the saved actor.
    The model transform is inspected: Unreal's static
mesh export should already contain the mesh coordinates in centimeters.
"""
import json, math, pathlib, struct, zlib

def read_fbx(path):
    data=pathlib.Path(path).read_bytes()
    if not data.startswith(b'Kaydara FBX Binary  \x00\x1a\x00'):
        raise ValueError('Expected binary FBX')
    version=struct.unpack_from('<I',data,23)[0]
    wide=version>=7500
    head='<QQQB' if wide else '<IIIB'
    hs=struct.calcsize(head)
    def prop(pos):
        kind=chr(data[pos]);pos+=1
        scalar={'Y':'h','C':'?','I':'i','F':'f','D':'d','L':'q'}
        if kind in scalar:
            fmt='<'+scalar[kind];value=struct.unpack_from(fmt,data,pos)[0]
            return value,pos+struct.calcsize(fmt)
        if kind in ('S','R'):
            length=struct.unpack_from('<I',data,pos)[0];pos+=4
            value=data[pos:pos+length]
            return value.decode('utf-8',errors='replace') if kind=='S' else value,pos+length
        if kind in 'fdilbc':
            length,encoding,size=struct.unpack_from('<III',data,pos);pos+=12
            raw=data[pos:pos+size]
            if encoding==1:raw=zlib.decompress(raw)
            fmt={'f':'f','d':'d','i':'i','l':'q','b':'?','c':'B'}[kind]
            return list(struct.unpack('<'+str(length)+fmt,raw)),pos+size
        raise ValueError('Unhandled property '+kind)
    def node(pos):
        end,nprops,propsbytes,namelen=struct.unpack_from(head,data,pos)
        if end==0:return None,pos+hs
        pos+=hs;name=data[pos:pos+namelen].decode();pos+=namelen
        values=[]
        for _ in range(nprops):value,pos=prop(pos);values.append(value)
        children=[]
        while pos<end-hs:
            child,pos=node(pos)
            if child is None:break
            children.append(child)
        return {'name':name,'props':values,'children':children},end
    roots=[];pos=27
    while pos<len(data):
        n,pos=node(pos)
        if n is None:break
        roots.append(n)
    return version,roots

def child(node,name):return next((c for c in node['children'] if c['name']==name),None)

def mesh_data(path,actor=None):
    version,roots=read_fbx(path)
    objects=next(n for n in roots if n['name']=='Objects')
    model_transforms=[]
    for n in objects['children']:
        if n['name']!='Model':continue
        pn=child(n,'Properties70');props={p['props'][0]:p['props'][4:] for p in pn['children']} if pn else {}
        model_transforms.append({'name':n['props'][1],**{k:props[k] for k in ['Lcl Translation','Lcl Rotation','Lcl Scaling','GeometricTranslation','GeometricRotation','GeometricScaling'] if k in props}})
    models=[]
    actor=actor or {'location':[0,0,0],'rotation':[0,0,0],'scale':[1,1,1]}
    loc=actor.get('location',actor.get('loc',[0,0,0]));scale=actor.get('scale',[1,1,1]);rot=actor.get('rotation',[0,0,0])
    # Actor snapshots store rotation as Unreal's pitch/yaw/roll.
    pitch,yaw,roll=[math.radians(x) for x in rot]
    cp,sp=math.cos(pitch),math.sin(pitch);cy,sy=math.cos(yaw),math.sin(yaw);cr,sr=math.cos(roll),math.sin(roll)
    def world(v):
        x,y,z=v[0]*scale[0],-v[1]*scale[1],v[2]*scale[2]
        return [loc[0]+cp*cy*x+(sr*sp*cy-cr*sy)*y+(-cr*sp*cy-sr*sy)*z,
                loc[1]+cp*sy*x+(sr*sp*sy+cr*cy)*y+(-cr*sp*sy+sr*cy)*z,
                loc[2]+sp*x-sr*cp*y+cr*cp*z]
    for n in objects['children']:
        if n['name']!='Geometry' or n['props'][-1]!='Mesh':continue
        vc=child(n,'Vertices');ic=child(n,'PolygonVertexIndex')
        if not vc or not ic:continue
        raw=vc['props'][0];verts=[world(raw[i:i+3]) for i in range(0,len(raw),3)]
        polys=[];poly=[]
        for idx in ic['props'][0]:
            poly.append(idx if idx>=0 else -idx-1)
            if idx<0:polys.append(poly);poly=[]
        bounds=[[min(v[i] for v in verts) for i in range(3)],[max(v[i] for v in verts) for i in range(3)]]
        models.append({'name':n['props'][1],'vertices':verts,'polygons':polys,'bounds':bounds})
    return {'version':version,'model_transforms':model_transforms,'meshes':models}

if __name__=='__main__':
    import sys
    data=mesh_data(sys.argv[1]);print(json.dumps({**data,'meshes':[{k:v for k,v in m.items() if k not in ('vertices','polygons')}|{'vertices':len(m['vertices']),'polygons':len(m['polygons'])} for m in data['meshes']]},indent=2))
