"""Author a seamless fired-brick PBR tile. Reference photos guide colours and bond only."""
from pathlib import Path
import json
import numpy as np
from PIL import Image, ImageFilter

D = Path(__file__).resolve().parent / 'textures'
D.mkdir(exist_ok=True)
S = 1024
rng = np.random.default_rng(202610103)
y, x = np.mgrid[:S, :S].astype(float)

def field(n):
    a = (rng.random((n,n))*255).astype('uint8')
    return np.asarray(Image.fromarray(a).resize((S,S),Image.Resampling.BICUBIC),float)/255-.5

fine = rng.random((S,S))-.5
medium = field(128)
cloud = field(32)
large = field(8)
row = np.floor(y/(S/24)).astype(int)
xx = (x+(row%2)*(S/16))%S
col = np.floor(xx/(S/8)).astype(int)
u = (xx%(S/8))/(S/8)
v = (y%(S/24))/(S/24)
edge_x = np.minimum(u,1-u)*24
edge_y = np.minimum(v,1-v)*8
# Irregular edges, a rounded arris, and mortar set roughly half a centimetre back.
dist = np.minimum(edge_x,edge_y) - (.48 + medium*.10 + cloud*.13)
face = np.clip(dist/.26,0,1)
face = face*face*(3-2*face)
colours = np.array([[.57,.295,.215],[.52,.255,.185],[.61,.325,.235],
                    [.46,.22,.17],[.55,.28,.22],[.62,.34,.255],[.50,.265,.205]])
pick = rng.integers(0,len(colours),(24,8))
shade = rng.uniform(.86,1.12,(24,8))
clay = colours[pick[row,col]]*shade[row,col,None]
clay *= (1+cloud*.18+medium*.13+fine*.085)[...,None]
firing = np.clip((large+.08)*1.4,0,.45)
clay *= (1-firing*.27)[...,None]
# Tiny aggregate, pinholes, and varied roughness avoid a printed grid look.
pore = np.clip((rng.random((S,S))-.976)*37,0,1)
clay *= (1-pore*.22)[...,None]
mortar = np.array([.655,.622,.535])[None,None,:]*(1+fine*.12+cloud*.10)[...,None]
colour = mortar*(1-face[...,None])+clay*face[...,None]
height = (.17+fine*.018)*(1-face) + (.84+cloud*.032+medium*.020+fine*.009-pore*.06)*face
rough = (.97+fine*.025)*(1-face)+(.80+cloud*.12+fine*.10)*face
ao = .80+face*.20

def save(name, a):
    Image.fromarray((np.clip(a,0,1)*255+.5).astype('uint8')).save(D/f'{name}.png')

def normal(h, cm_per_pixel, depth):
    gx = (np.roll(h,-1,1)-np.roll(h,1,1))*.5*depth/cm_per_pixel
    gy = (np.roll(h,-1,0)-np.roll(h,1,0))*.5*depth/cm_per_pixel
    n = np.stack((-gx,-gy,np.ones_like(gx)),axis=-1)
    n /= np.linalg.norm(n,axis=-1)[...,None]
    return n*.5+.5

save('Brick_Base',colour)
save('Brick_Normal',normal(height,192/S,.85))
save('Brick_Surface',np.stack((rough,ao,height),axis=-1))
# The separate brick face is used for real bevelled header and radial arch geometry.
single = colour[4:39,4:124]
single_h = height[4:39,4:124]
single_r = rough[4:39,4:124]
save('BrickFace_Base',single)
save('BrickFace_Normal',normal(single_h,24/120,.4))
save('BrickFace_Surface',np.stack((single_r,np.ones_like(single_r),single_h),axis=-1))
(D/'source.json').write_text(json.dumps({'source':'Original procedural assets authored for SeniorSendoff',
 'tile_cm':192,'bond':'8 bricks across / 24 courses, running bond','joint_cm':.96,
 'reference_images':['front.jpg','arcade.jpg'],'maps':['Base','DirectX normal','Roughness/AO/Height']},indent=2)+'\n')
print('Authored six brick textures; tile 192 cm; brick module 24 x 8 cm.')
