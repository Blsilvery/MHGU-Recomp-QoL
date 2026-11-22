import struct
from pathlib import Path

class Elf:
    def __init__(self,path):
        self.raw=Path(path).read_bytes(); b=self.raw
        assert b[:5]==b'\x7fELF\x01'
        phoff,shoff=struct.unpack_from('<II',b,0x1c)
        phent,phnum,shent,shnum,shstridx=struct.unpack_from('<HHHHH',b,0x2a)
        shstr=struct.unpack_from('<I',b,shoff+shstridx*shent+16)[0]
        def string(off): return b[off:b.index(b'\0',off)].decode()
        sections=[]
        for i in range(shnum):
            row=struct.unpack_from('<10I',b,shoff+i*shent)
            sections.append({'name':string(shstr+row[0]),'type':row[1],'addr':row[3],
                             'off':row[4],'size':row[5],'link':row[6],'entsize':row[9]})
        self.sections={x['name']:x for x in sections}
        self.symbols={}; self.dynsyms=[]
        for sec in sections:
            if sec['type'] not in (2,11): continue
            strs=sections[sec['link']]['off']; syms=[]
            for off in range(sec['off'],sec['off']+sec['size'],16):
                name,val,size,info,other,index=struct.unpack_from('<IIIBBH',b,off)
                sym={'name':string(strs+name),'value':val,'size':size,'info':info,'index':index}
                syms.append(sym)
                if sym['name']: self.symbols[sym['name']]=sym
            if sec['type']==11:self.dynsyms=syms
        self.relocs=[]
        for sec in sections:
            if sec['type']==9:
                for off in range(sec['off'],sec['off']+sec['size'],8):
                    addr,info=struct.unpack_from('<II',b,off)
                    self.relocs.append((addr,info&255,info>>8))
        self.needed=[]
        for sec in sections:
            if sec['type']==6:
                strs=sections[sec['link']]['off']
                for off in range(sec['off'],sec['off']+sec['size'],8):
                    tag,val=struct.unpack_from('<II',b,off)
                    if tag==1:self.needed.append(string(strs+val))

if __name__=='__main__':
    import sys,json
    e=Elf(sys.argv[1])
    print(json.dumps({'imports':[s['name'] for s in e.dynsyms if s['name'] and not s['index']],
                      'exports':[s['name'] for s in e.dynsyms if s['index']],
                      'relocation_types':sorted(set(x[1] for x in e.relocs)),
                      'needed':e.needed,
                      'test_symbols':{n:hex(s['value']) for n,s in e.symbols.items()
                                      if n in ['g_gather_all','g_toolless','g_hold_single','g_since_r','gather_hook','tool_check_hook','tool_break_hook','forge_onInit','g_playing_dead']}},indent=2))
