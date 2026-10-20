"""Strict verification with the known iCloud generated-app metadata guard."""
import argparse,ctypes,errno,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('tag');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
base=ROOT/'Saved/CalderWork'/args.tag;log=base.with_suffix('.log');receipt=base.with_suffix('.json');assert not log.exists() and not receipt.exists()
bundle=ROOT/'Binaries/Mac/SeniorSendoff.app';assert bundle.is_dir()
lib=ctypes.CDLL('/usr/lib/libSystem.B.dylib',use_errno=True);lib.removexattr.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_int];lib.removexattr.restype=ctypes.c_int
removed=0
def clear():
 global removed
 result=lib.removexattr(bytes(bundle),b'com.apple.FinderInfo',0)
 if result==0:removed+=1
 else:assert ctypes.get_errno() in (errno.ENOENT,93),ctypes.get_errno()
clear()
with log.open('w') as out:
 job=subprocess.Popen(['/usr/bin/codesign','--verify','--deep','--strict','--verbose=2',str(bundle)],stdout=out,stderr=subprocess.STDOUT)
 while job.poll() is None:clear();time.sleep(.005)
d={'passed':job.returncode==0,'exit_code':job.returncode,'finder_info_removals':removed,'guard':'Generated app root FinderInfo only, cleared before and during verification','bundle':str(bundle),'log':str(log),'standalone_gameplay_claimed':False}
receipt.write_text(json.dumps(d,indent=2)+'\n');print('STRICT_SIGNATURE '+('PASS' if d['passed'] else 'FAIL')+' '+str(receipt));assert d['passed']
