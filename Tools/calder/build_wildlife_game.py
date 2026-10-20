"""Standalone compile/sign check; remove only FinderInfo on generated app root.
Keeps normal Xcode signing,source,content,project config and quarantine unchanged.
"""
import argparse,ctypes,errno,hashlib,json,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];bundle=ROOT/'Binaries/Mac/SeniorSendoff.app'
lib=ctypes.CDLL('/usr/lib/libSystem.B.dylib',use_errno=True);lib.removexattr.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_int];lib.removexattr.restype=ctypes.c_int
parser=argparse.ArgumentParser();parser.add_argument('name',nargs='?',default='codex_wildlife_game_sign_retry');args=parser.parse_args();assert re.fullmatch('[a-z0-9_]+',args.name)
log=ROOT/'Saved/CalderWork'/(args.name+'.log');assert not log.exists(),log
inventory=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines()
assert not any(('/UnrealEditor' in s and 'Services' not in s) or s.endswith('/SeniorSendoff') for s in inventory),'Close Unreal/game before the standalone build'
freeze={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in ['Source','Config'] for p in (ROOT/folder).rglob('*') if p.is_file()}
started=time.monotonic()
cmd=['/Users/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh','SeniorSendoff','Mac','Development',f'-Project={ROOT}/SeniorSendoff.uproject','-WaitMutex','-NoHotReloadFromIDE','-DisableUnity']
removed=0
with log.open('w') as stream:
 job=subprocess.Popen(cmd,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
 try:
  while job.poll() is None:
   result=lib.removexattr(bytes(bundle),b'com.apple.FinderInfo',0)
   if result==0:removed+=1
   elif ctypes.get_errno() not in (errno.ENOENT,93):raise OSError(ctypes.get_errno(),'generated app FinderInfo cleanup failed')
   time.sleep(.05)
 except BaseException:
  job.terminate();job.wait();raise
print('WILDLIFE_GAME_BUILD exit=%d FinderInfoRemovals=%d log=%s'%(job.returncode,removed,log),flush=True)
signature_passed=False;signature_removed=0
if job.returncode==0:
 signature_log=ROOT/'Saved/CalderWork'/(args.name+'_signature.log');assert not signature_log.exists()
 with signature_log.open('w') as stream:
  # iCloud/Finder can reapply this generated-root attribute immediately after
  # the successful Xcode sign step. Keep the same narrow cleanup through verify.
  lib.removexattr(bytes(bundle),b'com.apple.FinderInfo',0)
  verify=subprocess.Popen(['codesign','--verify','--deep','--strict',str(bundle)],stdout=stream,stderr=subprocess.STDOUT)
  while verify.poll() is None:
   status=lib.removexattr(bytes(bundle),b'com.apple.FinderInfo',0)
   if status==0:signature_removed+=1
   elif ctypes.get_errno() not in (errno.ENOENT,93):raise OSError(ctypes.get_errno(),'generated app FinderInfo verification cleanup failed')
   time.sleep(.005)
 signature_passed=verify.returncode==0
unchanged=all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in freeze.items())
receipt={'passed':job.returncode==0 and signature_passed and unchanged,'build_exit':job.returncode,'strict_signature_passed':signature_passed,'source_config_frozen':unchanged,'source_sha256':freeze,'finder_info_root_removals_build':removed,'finder_info_root_removals_verify':signature_removed,'security_cloud_attributes_otherwise_preserved':True,'seconds':round(time.monotonic()-started,2),'standalone_gameplay_tested':False}
path=ROOT/'Saved/CalderWork'/(args.name+'_build.json');assert not path.exists();path.write_text(json.dumps(receipt,indent=2)+'\n');print('WILDLIFE_GAME_BUILD_SIGNATURE '+('PASS' if receipt['passed'] else 'FAIL')+' '+str(path),flush=True)
raise SystemExit(0 if receipt['passed'] else 1)
