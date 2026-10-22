#!/usr/bin/env python3
"""Explicitly refresh alandtse's pin; normal builds continue using immutable commits."""
import argparse,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(*args,cwd=None):return subprocess.check_output(args,cwd=cwd,text=True).strip()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--commonlib-ref',default='ng');p.add_argument('--vcpkg-ref');a=p.parse_args()
 path=ROOT/'cmake/dependencies.json';lock=json.loads(path.read_text())
 for name,ref in [('commonlib',a.commonlib_ref),('vcpkg',a.vcpkg_ref)]:
  if not ref:continue
  source=ROOT/'build/deps'/name
  if run('git','status','--porcelain','--untracked-files=no',cwd=source):raise SystemExit(f'{name} contains local edits')
  repository=lock[name]['repository']
  run('git','fetch',repository,ref,cwd=source)
  commit=run('git','rev-parse','FETCH_HEAD',cwd=source)
  if name=='commonlib':
   manifest=json.loads(run('git','show',commit+':vcpkg.json',cwd=source));lock[name]['version']=manifest['version-semver']
  lock[name]['commit']=commit
  run('git','checkout','--detach',commit,cwd=source)
  print(name,commit)
 path.write_text(json.dumps(lock,indent=2)+'\n')
 if a.vcpkg_ref:
  p=ROOT/'vcpkg.json';v=json.loads(p.read_text());v['builtin-baseline']=lock['vcpkg']['commit'];p.write_text(json.dumps(v,indent=2)+'\n')
 print('Run the full build and test suite before committing the updated lock.')
if __name__=='__main__':main()
