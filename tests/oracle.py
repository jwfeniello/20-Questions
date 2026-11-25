"""Original MIPS execution. Only RNG outputs may be intercepted, explicitly.
No scoring, reader, selection or flow instructions are replaced.
"""
import pathlib, sys, struct, hashlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'tools/python'))
from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN, UC_HOOK_CODE
from unicorn.mips_const import *
STATE=0x400000;META=0x410000;TARGETS=0x420000;QUESTIONS=0x460000
SIZE=0x6844
DESCS={'en':0x182124,'de':0xc7d98,'es':0x1a1bdc,'fr':0x1c1a5c,'it':0x1e116c,'jp':0x200edc}
class Stream:
    def __init__(self,seed=1,zero=False): self.state=seed;self.count=0;self.zero=zero;self.digest=hashlib.sha256()
    def next(self):
        self.state=(self.state*1664525+1013904223)&0xffffffff
        v=0 if self.zero else self.state&0x7fffffff
        self.count+=1;self.digest.update(struct.pack('<I',v));return v
class Oracle:
    def __init__(self,lang='en',stream=None):
        self.b=(ROOT.parent/'output/data/source/decrypted_eboot.bin').read_bytes()
        assert hashlib.sha256(self.b).hexdigest()=='52efc8494b0ea0be342e66632912eb6272ee97f541198f0f45959d2488aeb359'
        self.u=Uc(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_LITTLE_ENDIAN);self.u.mem_map(0,0x1000000);self.u.mem_write(0,self.b[192:0x28058c]);self.stream=stream
        if stream:self.u.hook_add(UC_HOOK_CODE,self.random,begin=0x155c0,end=0x155c0)
        desc=DESCS[lang];nt,nq,tp,qp,sp,mp=struct.unpack_from('<6I',self.b,desc+192);self.nt=nt;self.nq=nq;self.desc=desc
        self.meta=struct.pack('<13I',4,nt,nq,nt,nq,nt,nq,TARGETS,QUESTIONS,nt,nq,mp,sp);self.u.mem_write(META,self.meta)
        for count,ptr,base,kind in [(nt,tp,TARGETS,3),(nq,qp,QUESTIONS,2)]:
            records=bytearray()
            for i in range(count):
                off=struct.unpack_from('<I',self.b,ptr+192+4*i)[0];a=sp+off
                if kind==2 and i<4 and self.b[a+192]==0:a=[0x8e91c,0x8e924,0x8e930,0x8e938][i]
                while 48<=self.b[a+192]<=57:a+=1
                while self.b[a+192] in b' \r\n\t\v\f':a+=1
                records.extend(struct.pack('<9I',i,kind,a,0,0,off,0,0,0))
            self.u.mem_write(base,bytes(records))
        self.load(bytes(SIZE));self.call(0xfdcc,[STATE]);self.call(0xfe24,[STATE])
    def random(self,u,address,size,user):
        u.reg_write(UC_MIPS_REG_V0,self.stream.next());u.reg_write(UC_MIPS_REG_PC,u.reg_read(UC_MIPS_REG_RA))
    def load(self,state):
        self.u.mem_write(STATE,state);self.word(STATE+0x2738,META);self.word(STATE+0x2784,self.desc)
    def word(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
    def state(self):return bytes(self.u.mem_read(STATE,SIZE))
    def call(self,address,args=()):
        for reg,v in zip([UC_MIPS_REG_A0,UC_MIPS_REG_A1,UC_MIPS_REG_A2,UC_MIPS_REG_A3],args):self.u.reg_write(reg,v&0xffffffff)
        self.u.reg_write(UC_MIPS_REG_SP,0x4ff000);self.u.reg_write(UC_MIPS_REG_RA,0x3f0000)
        self.u.emu_start(address,0x3f0000,count=80000000)
        if self.u.reg_read(UC_MIPS_REG_PC)!=0x3f0000:raise AssertionError(f'Original routine {address:x} failed to return; PC={self.u.reg_read(UC_MIPS_REG_PC):x}')
        return self.u.reg_read(UC_MIPS_REG_V0)&0xffffffff
    def scores(self):return [struct.unpack('<I',self.u.mem_read(TARGETS+i*36+32,4))[0] for i in range(self.nt-1)]
