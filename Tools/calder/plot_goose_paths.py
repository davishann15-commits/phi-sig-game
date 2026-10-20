"""Plot native landing/departure CSVs against the actual authored pond outline."""
import argparse,csv,re,sys,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
os.environ.setdefault('MPLCONFIGDIR',str(ROOT/'Saved/CalderWork/plot_mpl_config'))
try:import matplotlib
except ModuleNotFoundError:
 sys.path.insert(0,str(ROOT/'Saved/CalderWork/plot_python_deps'))
 import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
run=ROOT/'Saved/CalderWork/runs'/args.tag;folder=run/'Saved/CalderWildlifeSkyPathsReview';out=folder/'goose_paths_plot.png';assert not out.exists()
rows=list(csv.DictReader((folder/'goose_curves.csv').open(encoding='utf-8-sig')))
data=(ROOT/'Source/SeniorSendoff/HouseCalderLandformData.inl').read_text();text=data.split('static const float PondOutline[10][2] = {',1)[1].split('};',1)[0]
pond=[tuple(float(x)/100 for x in pair) for pair in re.findall(r'\{\s*([-.\d]+)f,\s*([-.\d]+)f\s*\}',text)];assert len(pond)==10
fig,axes=plt.subplots(1,2,figsize=(13,6));colors=plt.cm.tab10.colors
for member in range(8):
 for role in ('arrival','departure'):
  r=[q for q in rows if q['role']==role and int(q['member'])==member];assert r
  x=[float(q['x'])/100 for q in r];y=[float(q['y'])/100 for q in r];z=[float(q['z'])/100 for q in r];arc=[float(q['arcCm'])/100 for q in r]
  style='-' if role=='arrival' else ':';color=colors[member] if role=='arrival' else '.6'
  axes[0].plot(x,y,style,color=color,lw=1,label='Arrival '+str(member+1) if role=='arrival' else None)
  axes[1].plot(arc,z,style,color=color,lw=1)
  if role=='arrival':axes[0].scatter([x[-1]],[y[-1]],s=15,color=color)
inset=axes[0].inset_axes([.59,.03,.38,.32]);inset.fill([q[0] for q in pond],[q[1] for q in pond],color='#69a9c5',alpha=.5)
for member in range(8):
 r=[q for q in rows if q['role']=='arrival' and int(q['member'])==member];inset.scatter([float(r[-1]['x'])/100],[float(r[-1]['y'])/100],s=12,color=colors[member])
inset.set(xlim=(225,280),ylim=(-165,-105),title='Touchdown spacing',aspect='equal');inset.tick_params(labelsize=6);inset.title.set_fontsize(7)
axes[0].fill([q[0] for q in pond],[q[1] for q in pond],color='#69a9c5',alpha=.5,label='Gander Pond')
axes[0].set(xlabel='World X (m)',ylabel='World Y (m)',title='Native paths and touchdown points',aspect='equal');axes[0].legend(fontsize=7,loc='best')
axes[1].axhline(6.4,color='#69a9c5',lw=1,label='Pond water6.4m');axes[1].set(xlabel='Distance along each path (m)',ylabel='World Z (m)',title='Arrival descent and resident departures')
for ax in axes:ax.grid(alpha=.2)
fig.suptitle('Goose landing data only — no server/pose/landing behavior acceptance',fontsize=11);fig.tight_layout();fig.savefig(out,dpi=150);plt.close(fig);print(out)
