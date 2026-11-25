import argparse, ctypes as C, json, pathlib, struct
from oracle import ROOT,Oracle,Stream,STATE,SIZE
U=C.c_uint32;P=C.c_void_p;WORDS=U*(SIZE//4)
class Native:
    def __init__(self,dll,lang='en',stream=None):
        self.d=C.CDLL(str(dll));d=self.d
        defs={'q20_load':([C.c_char_p,C.c_char_p,C.c_size_t],P),'q20_create':([P,U],P),'test_import':([P,C.POINTER(U)],None),'test_export':([P,C.POINTER(U)],None),'q20_score':([P,U],U),'q20_read':([P,C.c_int32,C.c_int32,C.c_int],U),'test_calls':([P],C.c_uint64),'test_target_score':([P,U],U),'q20_destroy':([P],None),'q20_unload':([P],None)}
        defs.update({n:([P],C.c_int32) for n in ('q20_divide','q20_isolate','q20_random_question','q20_advance')})
        defs.update({n:([P],None) for n in ('q20_feedback','q20_history_feedback')})
        defs['q20_submit']=([P,C.c_int32],C.c_int32)
        for n,(a,r) in defs.items():f=getattr(d,n);f.argtypes=a;f.restype=r
        err=C.create_string_buffer(256);self.model=d.q20_load(str(ROOT/'data'/f'{lang}.q20').encode(),err,256);assert self.model,err.value
        self.game=d.q20_create(self.model,1);assert self.game
        self.callback=None
        if stream:
            T=C.CFUNCTYPE(U,P);self.callback=T(lambda _:stream.next());d.q20_set_random.argtypes=[P,T,P];d.q20_set_random(self.game,self.callback,None)
    def load(self,b):self.d.test_import(self.game,WORDS.from_buffer_copy(b))
    def state(self):a=WORDS();self.d.test_export(self.game,a);return bytes(a)
    def close(self):self.d.q20_destroy(self.game);self.d.q20_unload(self.model)
def setword(b,o,v):struct.pack_into('<I',b,o,v&0xffffffff)
def getword(b,o):return struct.unpack_from('<I',b,o)[0]
def compare_state(a,b,ranges):
    diffs=[]
    for start,end in ranges:
        for o in range(start,end,4):
            if getword(a,o)!=getword(b,o):diffs.append((hex(o),getword(a,o),getword(b,o)))
    assert not diffs, diffs[:12]
def scoring(dll):
    results=[]
    histories=json.loads((ROOT/'tests/fixtures.json').read_text())['scoring_histories']
    for index,history in enumerate(histories):
        for zero in [False,True]:
            a=Stream(123,zero);b=Stream(123,zero);oracle=Oracle('en',a);native=Native(dll,'en',b);state=bytearray(oracle.state())
            setword(state,0x14,len(history));setword(state,0x10,19 if index==7 else 1+len(history));setword(state,0x1c,1 if index%2 else 0)
            for i,(id,kind,answer) in enumerate(history):
                for j,v in enumerate([i+1,id,kind,answer,0,0,0,0,0]):setword(state,0x2a8+36*i+4*j,v)
            oracle.load(bytes(state));native.load(state)
            expected=oracle.call(0xeb94,[STATE,0]);actual=native.d.q20_score(native.game,0)
            assert expected==actual,(index,expected,actual)
            compare_state(oracle.state(),native.state(),[(0x1c,0x2a8),(0x67a8,0x67bc)])
            assert oracle.scores()==[native.d.test_target_score(native.game,i) for i in range(oracle.nt-1)]
            assert a.count==b.count and a.digest.digest()==b.digest.digest()
            results.append({'fixture':index,'zero_random':zero,'retained':actual,'random_calls':a.count,'random_sha256':a.digest.hexdigest()});native.close();print('score',index,zero,'PASS',flush=True)
    return results
def selection(dll):
    results=[]
    for fixture in range(6):
        for addr,name in [(0xe474,'q20_divide'),(0xe6d0,'q20_isolate')]:
            a=Stream(271,fixture==4);b=Stream(271,fixture==4);o=Oracle('en',a);n=Native(dll,'en',b);state=bytearray(o.state())
            candidates=[[],[(1,100)],[(1,100),(2,100),(3,100)],[(77,25),(17,24),(56,22),(1,20)],[(1,0),(2,0)],[(1,0x7ffffff0),(2,0x80000010),(3,0x20000000)]][fixture]
            setword(state,0x20,len(candidates))
            for i,(id,score) in enumerate(candidates):setword(state,0x28+16*i,id);setword(state,0x2c+16*i,score)
            setword(state,0x14,2);setword(state,0x2a8+4,5);setword(state,0x2a8+8,2);setword(state,0x2a8+36+4,77);setword(state,0x2a8+36+8,2)
            o.load(bytes(state));n.load(state);x=o.call(addr,[STATE]);y=getattr(n.d,name)(n.game)&0xffffffff
            assert x==y,(fixture,name,x,y);assert a.count==b.count and a.digest.digest()==b.digest.digest()
            results.append({'fixture':fixture,'routine':name,'selected':x,'random_calls':a.count});n.close();print(name,fixture,'PASS',flush=True)
    return results
RANGES=[(0,0x2738),(0x6788,0x6840)]
def transcripts(dll):
    results=[]
    configs=json.loads((ROOT/'tests/fixtures.json').read_text())['transcripts']
    for lang,mode,target in configs:
        a=Stream(1234);b=Stream(1234);o=Oracle(lang,a);n=Native(dll,lang,b);n.load(o.state());steps=[]
        for turn in range(80):
            x=o.call(0xf7ec,[STATE]);y=n.d.q20_advance(n.game)&0xffffffff
            assert x==y,(lang,mode,turn,'advance',x,y)
            try:compare_state(o.state(),n.state(),RANGES)
            except AssertionError as e:raise AssertionError((lang,mode,turn,e))
            s=n.state();kind=getword(s,8);id=getword(s,12);number=getword(s,16)
            entry={'turn':turn,'kind':kind,'id':id,'number':number,'count':getword(s,32),'leader':getword(s,28),'rng_calls':a.count,'candidates':[[getword(s,0x28+16*i),C.c_int32(getword(s,0x2c+16*i)).value] for i in range(getword(s,32))]};steps.append(entry)
            assert a.count==b.count and a.digest.digest()==b.digest.digest()
            if kind==99 or x!=0:break
            if kind==1:
                codes=[n.d.q20_read(n.model,c,target,2) for c in range(1,5)];positives=[(v,-i) for i,v in enumerate(codes,1) if v&1];answer=-max(positives)[1] if positives else 4
            elif kind==3:answer=2 if mode=='accept' or (mode=='truth' and id==target) else 1
            elif mode=='unknown':answer=0 if turn<20 else 1
            elif mode=='uncertain':answer=[0,5,6,7,8,9,10,11,3,4][turn%10]
            else:answer={0:0,1:5,2:7,4:1,9:6,10:8,12:2}[n.d.q20_read(n.model,id,target,4)]
            entry['answer']=answer;x=o.call(0xfa80,[STATE,answer]);y=n.d.q20_submit(n.game,answer)&0xffffffff
            assert x==y,(lang,mode,turn,'submit',x,y);compare_state(o.state(),n.state(),RANGES)
        assert kind==99,(lang,mode,'transcript did not terminate',steps[-1]);n.close();results.append({'language':lang,'mode':mode,'target':target,'steps':steps,'random_sha256':a.digest.hexdigest()});print('transcript',lang,mode,len(steps),'PASS',flush=True)
    return results
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--library',type=pathlib.Path,default=ROOT/'build/libq20_test.dll');p.add_argument('--stage',choices=['scoring','selection','transcripts','all'],default='all');a=p.parse_args()
    for name,fn in [('scoring',scoring),('selection',selection),('transcripts',transcripts)]:
        if a.stage in ('all',name):r=fn(a.library.resolve());(ROOT/'reports'/f'{name}.json').write_text(json.dumps(r,indent=2)+'\n')
