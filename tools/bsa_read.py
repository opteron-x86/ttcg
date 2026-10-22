"""Read the installed Skyrim SE BSA directory and uncompressed file entries."""
import struct
from pathlib import Path
class Bsa:
 def __init__(self,path):
  self.path=Path(path);self.files={}
  with self.path.open('rb') as f:
   magic,version,offset,flags,folders,files,folder_bytes,name_bytes,types=struct.unpack('<4s8I',f.read(36))
   if magic!=b'BSA\0' or version!=105 or flags&3!=3:raise ValueError('Expected SSE BSA with named folders/files')
   self.flags=flags;f.seek(offset);records=[struct.unpack('<QIIQ',f.read(24)) for _ in range(folders)]
   entries=[]
   for _,count,_,_ in records:
    length=f.read(1)[0];folder=f.read(length).rstrip(b'\0').decode('cp1252')
    for _ in range(count):
     _,size,pos=struct.unpack('<QII',f.read(16));entries.append((folder,size,pos))
   names=f.read(name_bytes).split(b'\0');assert names[-1]==b'' and len(names)-1==files and len(entries)==files
   for (folder,size,pos),name in zip(entries,names):self.files[(folder+'\\'+name.decode('cp1252')).lower()]=(size,pos)
 def read(self,name):
  size,pos=self.files[name.replace('/','\\').lower()]
  if bool(self.flags&4)^bool(size&0x40000000):raise ValueError('Compressed BSA entry is not supported by this narrow extractor')
  with self.path.open('rb') as f:f.seek(pos);data=f.read(size&0x3fffffff)
  if self.flags&0x100:data=data[1+data[0]:]
  return data
