"""Card trading through the same service-faction gates as album purchases."""
import struct
from plugin_records import walk,parse_fields,build_record,apply,ctda,zstring,shared_response

IDS=(0x02000FD7,0x02000FD8,0x02000FD9)

def generate_shop(blob):
    records,_=walk(blob)
    def clone(source,target,update):
        start,end,_=records[source]
        header,fields=parse_fields(blob[start:end]);header=bytearray(header)
        struct.pack_into('<I',header,12,target)
        return build_record(header,update(fields))
    branch=clone(0x02000830,IDS[0],lambda f:[(k,zstring('TTCG_CardShopBranch') if k==b'EDID' else struct.pack('<I',IDS[1]) if k==b'SNAM' else v) for k,v in f])
    topic=clone(0x02000831,IDS[1],lambda f:[(k,zstring('TTCG_CardShopTopic') if k==b'EDID' else zstring("I'd like to trade Tessera cards.") if k==b'FULL' else struct.pack('<I',IDS[0]) if k==b'BNAM' else v) for k,v in f])
    def response(fields):
        out=[]
        for k,v in fields:
            if k==b'CTDA':continue
            if k==b'EDID':v=zstring('TTCG_CardShopResponse')
            if k==b'VMAD':
                old=b'TTCG_AlbumPurchaseFragment';new=b'TTCG_CardShopFragment'
                v=v.replace(struct.pack('<H',len(old))+old,struct.pack('<H',len(new))+new)
            out.append((k,v))
        out=shared_response(out,0x0007F6BC) # Vanilla "Take a look.", local actions only.
        return out+[(b'CTDA',ctda(0x60,1,74,0x02000850)),(b'CTDA',ctda(1,1,71,0x51599)),(b'CTDA',ctda(0,1,71,0x02000882))]
    info=clone(0x02000832,IDS[2],response)
    if IDS[0] in records:
        for form,record in zip(IDS,(branch,topic,info)):
            records,_=walk(blob);a,b,parents=records[form];blob=apply(blob,a,b,parents,record)
        return blob
    assert not any(form in records for form in IDS), 'Card-shop FormIDs are already occupied'
    parent=records[0x02000830][2][0];end=parent+struct.unpack_from('<I',blob,parent+4)[0]
    blob=apply(blob,end,end,[parent],branch)
    records,_=walk(blob);parent=records[0x02000831][2][0];end=parent+struct.unpack_from('<I',blob,parent+4)[0]
    group=b'GRUP'+struct.pack('<IIIHHHH',24+len(info),IDS[1],7,0,0,0,0)+info
    blob=apply(blob,end,end,[parent],topic+group)
    records,_=walk(blob);a,b,parents=records[0];header,fields=parse_fields(blob[a:b])
    fields=[(k,v[:4]+struct.pack('<II',struct.unpack_from('<I',v,4)[0]+4,max(struct.unpack_from('<I',v,8)[0],0xFDA)) if k==b'HEDR' else v) for k,v in fields]
    return apply(blob,a,b,parents,build_record(header,fields))
