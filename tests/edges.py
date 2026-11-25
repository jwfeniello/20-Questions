import ctypes as C, hashlib, json, pathlib, struct
from compare import Native,U,P,ROOT,WORDS,RANGES,compare_state,setword,getword
from oracle import Oracle,Stream,STATE,SIZE

def reader(dll):
    results=[]
    for lang in ('en','de','es','fr','it','jp'):
        n=Native(dll,lang);o=Oracle(lang);n.d.test_read_all.argtypes=[P,C.c_int,C.POINTER(C.c_ubyte)]
        for fmt,file in [(4,'parameters.codes.u8'),(2,'parameters.format2.u8')]:
            buf=(C.c_ubyte*(o.nt*o.nq))();n.d.test_read_all(n.model,fmt,buf);actual=bytes(buf)
            original=(ROOT.parent/'output/data/banks'/lang/file).read_bytes()
            expected=b''.join(original[t*o.nq:t*o.nq+o.nq-1]+b'\xff' for t in range(o.nt-1))+b'\xff'*o.nq
            assert actual==expected
            # Execute the original reader for every accepted pair with a MIPS driver.
            code=[];labels={};fix=[]
            def i(op,rs,rt,imm):code.append(op<<26|rs<<21|rt<<16|(imm&65535))
            def move(rd,rs):code.append(rs<<21|rd<<11|0x21)
            def branch(rs,rt,label):fix.append((len(code),label));i(5,rs,rt,0)
            i(9,0,16,1);labels['outer']=len(code);i(9,0,17,1);labels['inner']=len(code)
            i(15,0,4,0x40);move(5,17);move(6,16);i(9,0,7,fmt);code.extend([3<<26|(0x10b3c>>2),0]);i(40,18,2,0);i(9,18,18,1);i(9,17,17,1)
            code.append(17<<21|20<<16|8<<11|0x2a);branch(8,0,'inner');code.append(0);i(9,16,16,1);code.append(16<<21|19<<16|8<<11|0x2a);branch(8,0,'outer');code.append(0)
            for at,label in fix:code[at]|=(labels[label]-at-1)&65535
            instructions=struct.pack('<'+'I'*len(code),*code);o.u.mem_write(0x300000,instructions);o.u.ctl_remove_cache(0x300000,0x300000+len(instructions))
            from unicorn.mips_const import UC_MIPS_REG_S2,UC_MIPS_REG_S3,UC_MIPS_REG_S4,UC_MIPS_REG_SP,UC_MIPS_REG_PC
            for reg,v in [(UC_MIPS_REG_S2,0x500000),(UC_MIPS_REG_S3,o.nt),(UC_MIPS_REG_S4,o.nq),(UC_MIPS_REG_SP,0x4ff000)]:o.u.reg_write(reg,v)
            o.u.emu_start(0x300000,0x300000+len(instructions),count=300000000);assert o.u.reg_read(UC_MIPS_REG_PC)==0x300000+len(instructions)
            refs=bytes(o.u.mem_read(0x500000,(o.nt-1)*(o.nq-1)));assert refs==b''.join(actual[t*o.nq:t*o.nq+o.nq-1] for t in range(o.nt-1))
            results.append({'bank':lang,'format':fmt,'accepted_pairs':len(refs),'native_physical_slots_including_rejected_endpoints':len(actual),'sha256':hashlib.sha256(refs).hexdigest()})
        for q,t in [(0,1),(1,0),(-1,1),(1,-1),(o.nq,1),(1,o.nt),(o.nq-1,o.nt-1)]:assert o.call(0x10b3c,[STATE,q,t,4])==n.d.q20_read(n.model,q,t,4)
        n.close();print('reader',lang,'PASS',flush=True)
    return results

def flow_edges(dll):
    result=[]
    # Stored Unknown must remain weak-negative, not neutral. A previous-leader
    # bonus keeps this target eligible so its exact penalty is observable.
    original=(ROOT.parent/'output/data/banks/en/parameters.codes.u8').read_bytes()
    idx=next(i for i,c in enumerate(original) if c==0 and 4<=i%369<368 and i//369<3082)
    target=idx//369+1;question=idx%369+1
    a=Stream(1,True);b=Stream(1,True);o=Oracle('en',a);n=Native(dll,'en',b);s=bytearray(o.state())
    for off,v in [(16,10),(20,1),(28,target),(0x2ac,question),(0x2b0,2),(0x2b4,2)]:setword(s,off,v)
    o.load(bytes(s));n.load(s);assert o.call(0xeb94,[STATE,4])==n.d.q20_score(n.game,4)
    compare_state(o.state(),n.state(),[(0x1c,0x2a8),(0x67a8,0x67bc)])
    score=n.d.test_target_score(n.game,target-1);assert score==6==o.scores()[target-1]
    result.append({'case':'stored-unknown-is-weak-negative','question':question,'target':target,'raw_code':0,'previous_leader_bonus':10,'yes_mismatch_penalty':4,'score':score});n.close()
    # Complete history permits no questions. Force Random's 20 failed attempts;
    # its ignored return must lead to guess, with matching RNG consumption.
    for mode in ['random-discard','unknown-eviction','history-full','counter-limit','undo','close-guess']:
        a=Stream(413);b=Stream(413);o=Oracle('de',a);n=Native(dll,'de',b);s=bytearray(o.state());setword(s,0x10,8)
        setword(s,0x14,1);setword(s,0x2a8+8,1);setword(s,0x2a8+12,0)
        address=0xf7ec;args=[STATE];fn=lambda:n.d.q20_advance(n.game)&0xffffffff
        if mode=='random-discard':
            # Deterministic zero avoids the original null-endpoint dereference.
            a.zero=b.zero=True;setword(s,0x679c,7);setword(s,0x14,146)
            # category Animal makes positive scoring with a zero stream
            setword(s,0x2a8+12,1)
            for i,q in enumerate(range(5,150),1):setword(s,0x2a8+36*i+4,q);setword(s,0x2a8+36*i+8,2)
        elif mode in ('unknown-eviction','history-full'):
            count=17 if mode=='unknown-eviction' else 260;setword(s,0x14,count);setword(s,8,2);setword(s,12,20)
            for i in range(count):setword(s,0x2a8+36*i+4,i%145+5);setword(s,0x2a8+36*i+8,2);setword(s,0x2a8+36*i+12,0 if mode=='unknown-eviction' or i==5 else 1)
            address=0xfa80;args=[STATE,0];fn=lambda:n.d.q20_submit(n.game,0)&0xffffffff
        elif mode=='counter-limit':setword(s,16,31)
        elif mode=='undo':
            setword(s,0x2a8+12,1);address=0xfa80;args=[STATE,-1];fn=lambda:n.d.q20_submit(n.game,-1)&0xffffffff
        else:
            setword(s,8,3);setword(s,12,1);address=0xfa80;args=[STATE,3];fn=lambda:n.d.q20_submit(n.game,3)&0xffffffff
        o.load(bytes(s));n.load(s);x=o.call(address,args);y=fn();assert x==y,(mode,x,y);compare_state(o.state(),n.state(),RANGES);assert a.count==b.count
        result.append({'case':mode,'return_u32':x,'kind':getword(n.state(),8),'random_calls':a.count});n.close();print('edge',mode,'PASS',flush=True)
    # Seeded generator itself executes without interception for 4096 calls per seed.
    for seed in (0,1,0x7fffffff,0xffffffff):
        o=Oracle('en');n=Native(dll);n.d.test_rng.argtypes=[P,U];n.d.q20_rand.argtypes=[P];n.d.q20_rand.restype=U;n.d.test_rng(n.game,seed);o.call(0x155b0,[seed])
        for _ in range(4096):assert o.call(0x155c0)==n.d.q20_rand(n.game)
        n.close();result.append({'case':'original-rng','seed':seed,'outputs':4096});print('rng',seed,'PASS',flush=True)
    # Observe the original null dereference, without substituting any reader or
    # selection code. Stop on its data access because address zero is mapped
    # for zero-based ELF code in this isolated harness.
    from unicorn import UC_HOOK_MEM_READ
    a=Stream();b=Stream();a.next=b.next=lambda:145
    o=Oracle('de',a);n=Native(dll,'de',b);observed=[]
    def low_read(u,access,address,size,value,user):
        from unicorn.mips_const import UC_MIPS_REG_PC
        observed.append({'address':address,'size':size,'pc':u.reg_read(UC_MIPS_REG_PC)});u.emu_stop()
    o.u.hook_add(UC_HOOK_MEM_READ,low_read,begin=0,end=0xff)
    try:o.call(0xe96c,[STATE])
    except AssertionError:pass
    assert observed and observed[0]['address']==0x18,observed
    assert n.d.q20_random_question(n.game)==-2;n.close()
    result.append({'case':'random-invalid-endpoint','helper_output':145,'question_id':150,'original_null_read':observed,'native_result':-2,'parity':'explicit safety departure'})
    return result
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--library',type=pathlib.Path,default=ROOT/'build/libq20_test.dll');a=p.parse_args()
    results={'reader':reader(a.library.resolve()),'edges':flow_edges(a.library.resolve())};(ROOT/'reports/edges.json').write_text(json.dumps(results,indent=2)+'\n')
