"""Build portable containers from the verified extraction; never modify inputs."""
import argparse, hashlib, json, pathlib, struct, zlib
def main(source,dest):
    manifest=json.loads((source/'manifest.json').read_text())
    assert manifest['decrypted_elf_sha256']=='52efc8494b0ea0be342e66632912eb6272ee97f541198f0f45959d2488aeb359'
    hashes={r['path']:r['sha256'] for r in manifest['outputs']}
    used={}
    def read(name):
        b=(source/name).read_bytes();h=hashlib.sha256(b).hexdigest()
        if hashes.get(name)!=h:raise ValueError('Source hash mismatch: '+name)
        used[name]=h;return b
    tables=json.loads(read('auxiliary/tables.json'));fixed=read('auxiliary/fixed_tables.bin')
    assert len(fixed)==1228
    dest.mkdir(parents=True,exist_ok=True);outputs=[]
    for lang in ('en','de','es','fr','it','jp'):
        bank=manifest['banks'][lang];nt=bank['target_count'];nq=bank['question_count']
        packed=read(f'banks/{lang}/parameters.packed4');pool=read(f'banks/{lang}/string_pool.bin')
        text=bytearray();records=bytearray();labels=bytearray()
        def string(s):
            off=len(text);text.extend(s.encode('utf8')+b'\0');return off
        for kind,n in [('targets',nt),('questions',nq)]:
            rows=json.loads(read(f'banks/{lang}/{kind}.json'));assert len(rows)==n
            for i,r in enumerate(rows):
                assert r['slot']==i and r['reader_id']==i+1 and r['stored_record_id']==i
                raw=pool[r['text_offset']:].split(b'\0')[0];assert raw.hex()==r['raw_hex']
                prefix=r['decimal_text_id'];num=int(prefix) if prefix else 0xffffffff
                records.extend(struct.pack('<4I',r['stored_record_id'],num,string(r['display_utf8']),r['text_offset']))
        for name in ('answers','categories','guess_answers'):
            for r in tables[name]:labels.extend(struct.pack('<I',string(r['label'] or '')))
        assert len(labels)==96
        body=packed+pool+records+fixed+labels+text
        header=b'Q20PSP1\0'+struct.pack('<8I',1,nt,nq,len(packed),len(pool),len(text),zlib.crc32(body),len(body))+lang.encode()+b'\0\0'+struct.pack('<I',1228)+bytes(16)
        assert len(header)==64
        result=header+body;path=dest/(lang+'.q20');path.write_bytes(result)
        outputs.append({'file':path.name,'size':len(result),'sha256':hashlib.sha256(result).hexdigest(),'targets':nt,'questions':nq,'packed_sha256':hashlib.sha256(packed).hexdigest()})
    (dest/'manifest.json').write_text(json.dumps({'schema':'q20-container-1','sources':used,'outputs':outputs,'unsupported':{'el':'Incomplete zero stub, not a usable model'}},indent=2)+'\n')
    print('Prepared six exact packed banks:',dest)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args();main(a.source,a.out)
