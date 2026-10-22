#!/usr/bin/env python3
"""Refresh TTCG.ini comments and supported keys without changing configured values."""
from pathlib import Path
import argparse, configparser, re, shutil
from datetime import datetime, timezone
ROOT=Path(__file__).resolve().parents[1]
def refresh(template,existing):
 config=configparser.ConfigParser(interpolation=None,strict=False);config.read_string(existing)
 section='';supported=set();result=[]
 for line in template.splitlines(True):
  match=re.match(r'\s*\[([^]]+)\]',line)
  if match:section=match[1]
  match=re.match(r'([A-Za-z][A-Za-z0-9]*)\s*=.*',line)
  if match:
   key=match[1];supported.add((section,key.lower()))
   if config.has_option(section,key):line=key+'='+config.get(section,key).strip()+'\n'
  result.append(line)
 removed=[section+'.'+key for section in config.sections() for key in config[section] if (section,key) not in supported]
 return ''.join(result),removed

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('path',type=Path);parser.add_argument('--check',action='store_true');parser.add_argument('--all-tournaments',action='store_true',help='Enable development and all three tournament tests');args=parser.parse_args()
 template=(ROOT/'SKSE/Plugins/TTCG.ini.example').read_text();old=args.path.read_text();updated,removed=refresh(template,old)
 if args.all_tournaments:
  section='';lines=[]
  for line in updated.splitlines(True):
   match=re.match(r'\s*\[([^]]+)\]',line)
   if match:section=match[1]
   if section=='Development' and re.match(r'(Enabled|RegularTournaments|InvitationalTournaments|MastersTournaments)=',line):line=line.split('=',1)[0]+'=1\n'
   lines.append(line)
  updated=''.join(lines)
 if args.check:print('Retired settings:',', '.join(removed) or 'none');return
 if updated==old:print('INI already current');return
 backup=args.path.with_name('TTCG.ini.backup-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S'));shutil.copy2(args.path,backup)
 temp=args.path.with_name('TTCG.ini.tmp');temp.write_text(updated);temp.replace(args.path)
 print('Updated',args.path,'; preserved configured values; removed:',', '.join(removed) or 'no settings','; backup:',backup)
if __name__=='__main__':main()
