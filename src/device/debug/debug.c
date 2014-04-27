/* This file is a part of NXVM project. */

#include "../../utils.h"
#include "../vport.h"
#include "../vcpu.h"
#include "../vcpuins.h"
#include "../vram.h"
#include "../vmachine.h"

#include "../device.h"
#include "../../machine.h"

#include "aasm32.h"
#include "dasm32.h"
#include "debug.h"

t_debug vdebug;

#define DEBUG_MAXNARG 256
#define DEBUG_MAXNASMARG 4

static t_nubitcc nErrPos;
static t_nubitcc narg;
static char **arg;
static t_bool flagexit;
static t_string strCmdBuff, strCmdCopy, strFileName;

static void seterr(t_nubitcc pos)
{
	nErrPos = (t_nubitcc)(arg[pos] - strCmdCopy + strlen(arg[pos]) + 1);
}
static t_nubit8 scannubit8(const t_strptr s)
{
	t_nubit8 ans = 0;
	int i = 0;
	if(s[0] == '\'' && s[2] == '\'') return s[1];
	utilsLowerStr(s);
	while(s[i] != '\0' && s[i] != '\n') {
		if(i > 1) {seterr(narg-1);break;}
		ans <<= 4;
		if(s[i] > 0x2f && s[i] < 0x3a) ans += s[i] - 0x30;
		else if(s[i] > 0x60 && s[i] < 0x67) ans += s[i] - 0x57;
		else seterr(narg-1);
		++i;
	}
	return ans;
}
static t_nubit16 scannubit16(const t_strptr s)
{
	t_nubit16 ans = 0;
	int i = 0;
	utilsLowerStr(s);
	while(s[i] != '\0' && s[i] != '\n') {
		if(i > 3) {seterr(narg-1);break;}
		ans <<= 4;
		if(s[i] > 0x2f && s[i] < 0x3a) ans += s[i] - 0x30;
		else if(s[i] > 0x60 && s[i] < 0x67) ans += s[i] - 0x57;
		else seterr(narg-1);
		++i;
	}
	return ans;
}
static t_nubit32 scannubit32(const t_strptr s)
{
	t_nubit32 ans = 0;
	int i = 0;
	utilsLowerStr(s);
	while(s[i] != '\0' && s[i] != '\n') {
		if(i > 7) {seterr(narg-1);break;}
		ans <<= 4;
		if(s[i] > 0x2f && s[i] < 0x3a) ans += s[i] - 0x30;
		else if(s[i] > 0x60 && s[i] < 0x67) ans += s[i] - 0x57;
		else seterr(narg-1);
		++i;
	}
	return ans;
}

static t_nubit16 dumpSegRec;
static t_nubit16 dumpPtrRec;
static t_nubit16 asmSegRec;
static t_nubit16 asmPtrRec;
static t_nubit16 uasmSegRec;
static t_nubit16 uasmPtrRec;

static t_nubit16 seg;
static t_nubit16 ptr;
static void addrparse(t_nubit16 defseg, const t_strptr addr)
{
	t_strptr cseg,cptr;
	char ccopy[MAXLINE];
	STRCPY(ccopy,addr);
	cseg = STRTOK(ccopy,":");
	cptr = STRTOK(NULL,"");
	if(!cptr) {
		seg = defseg;
		ptr = scannubit16(cseg);
	} else {
		if(!STRCMP(cseg,"es")) seg = vcpu.es.selector;
		else if(!STRCMP(cseg,"cs")) seg = vcpu.cs.selector;
		else if(!STRCMP(cseg,"ss")) seg = vcpu.ss.selector;
		else if(!STRCMP(cseg,"ds")) seg = vcpu.ds.selector;
		else seg = scannubit16(cseg);
		ptr = scannubit16(cptr);
	}
}

/* DEBUG CMD BEGIN */
/* assemble */
static void aconsole()
{
	t_nubitcc i;
	t_nubit8 len;
	char cmdAsmBuff[MAXLINE];
	t_nubit8 acode[15];
	t_nubitcc errAsmPos;
	t_bool flagexitasm = 0;
	while(!flagexitasm) {
		utilsPrint("%04X:%04X ", asmSegRec, asmPtrRec);
		fflush(stdin);
		FGETS(cmdAsmBuff,MAXLINE,stdin);
		utilsLowerStr(cmdAsmBuff);
		if(!strlen(cmdAsmBuff)) {
			flagexitasm = 1;
			continue;
		}
		if(cmdAsmBuff[0] == ';' ) continue;
		errAsmPos = 0;
		len = aasm32(cmdAsmBuff, (t_vaddrcc)acode);
		if(!len) errAsmPos = (t_nubitcc)strlen(cmdAsmBuff) + 9;
		else {
			if (vcpuinsWriteLinear((asmSegRec << 4) + asmPtrRec, (t_vaddrcc)acode, len)) {
				utilsPrint("debug: fail to write to L%08X\n", (asmSegRec << 4) + asmPtrRec);
				return;
			}
			asmPtrRec += len;
		}
		if(errAsmPos) {
			for(i = 0;i < errAsmPos;++i) utilsPrint(" ");
			utilsPrint("^ Error\n");
		}
	}
}
static void a()
{
	if(narg == 1) {
		aconsole();
	} else if(narg == 2) {
		addrparse(vcpu.cs.selector, arg[1]);
		if(nErrPos) return;
		asmSegRec = seg;
		asmPtrRec = ptr;
		aconsole();
	} else seterr(2);
}
/* compare */
static void c()
{
	t_nubit8 val1,val2;
	t_nubit16 i,seg1,ptr1,range,seg2,ptr2;
	if(narg != 4) seterr(narg-1);
	else {
		addrparse(vcpu.ds.selector,arg[1]);
		seg1 = seg;
		ptr1 = ptr;
		addrparse(vcpu.ds.selector,arg[3]);
		seg2 = seg;
		ptr2 = ptr;
		range = scannubit16(arg[2])-ptr1;
		if(!nErrPos) {
			for(i = 0;i <= range;++i) {
				val1 = vramRealByte(seg1,ptr1+i);
				val2 = vramRealByte(seg2,ptr2+i);
				if(val1 != val2) {
					utilsPrint("%04X:%04X  ", seg1, ptr1 + i);
					utilsPrint("%02X  %02X", val1, val2);
					utilsPrint("  %04X:%04X\n", seg2, ptr2 + i);
				}
			}
		}
	}
}
/* dump */
static void dprint(t_nubit16 segment,t_nubit16 start,t_nubit16 end)
{
	char t,c[0x11];
	t_nubit16 i;
	if(start > end) end = 0xffff;
	if((t_nubit32)((segment<<4)+end) > 0xfffff) end = (0xfffff - (segment << 4));
	c[0x10] = '\0';
	if(end < start) end = 0xffff;
	for(i = start-(start%0x10);i <= end+(0x10-end%0x10)-1;++i) {
		if(i%0x10 == 0) utilsPrint("%04X:%04X  ", segment, i);
		if(i < start || i > end) {
			utilsPrint("  ");
			c[i%0x10] = ' ';
		} else {
			c[i%0x10] = vramRealByte(segment,i);
			utilsPrint("%02X",c[i%0x10] & 0xff);
			t = c[i%0x10];
			if((t >=1 && t <= 7) || t == ' ' ||
				(t >=11 && t <= 12) ||
				(t >=14 && t <= 31) ||
				(t >=33)) ;
			else c[i%0x10] = '.';
		}
		utilsPrint(" ");
		if(i%0x10 == 7 && i >= start && i < end) utilsPrint("\b-");
		if((i+1)%0x10 == 0) {
			utilsPrint("  %s\n",c);
		}
		if(i == 0xffff) break;
	}
	dumpSegRec = segment;
	dumpPtrRec = end+1;
}
static void d()
{
	t_nubit16 ptr2;
	if(narg == 1) dprint(dumpSegRec,dumpPtrRec,dumpPtrRec+0x7f);
	else if(narg == 2) {
		addrparse(vcpu.ds.selector,arg[1]);
		if(nErrPos) return;
		dprint(seg,ptr,ptr+0x7f);
	} else if(narg == 3) {
		addrparse(vcpu.ds.selector,arg[1]);
		ptr2 = scannubit16(arg[2]);
		if(nErrPos) return;
		if(ptr > ptr2) seterr(2);
		else dprint(seg,ptr,ptr2);
	} else seterr(3);
}
/* enter */
static void e()
{
	t_nubit8 i, val;
	char s[MAXLINE];
	if(narg == 1) seterr(0);
	else if(narg == 2) {
		addrparse(vcpu.ds.selector,arg[1]);
		if(nErrPos) return;
		utilsPrint("%04X:%04X  ", seg, ptr);
		utilsPrint("%02X",vramRealByte(seg,ptr));
		utilsPrint(".");
		FGETS(s,MAXLINE,stdin);
		utilsLowerStr(s);//!!
		val = scannubit8(s);//!!
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			vramRealByte(seg,ptr) = val;
	} else if(narg > 2) {
		addrparse(vcpu.ds.selector,arg[1]);
		if(nErrPos) return;
		for(i = 2;i < narg;++i) {
			val = scannubit8(arg[i]);//!!
			if(!nErrPos) vramRealByte(seg,ptr) = val;
			else break;
			ptr++;
		}
	}
}
/* fill */
static void f()
{
	t_nubit8 nbyte;
	t_nubit8 val;
	t_nubit16 i,j,end;
	if(narg < 4) seterr(narg-1);
	else {
		addrparse(vcpu.ds.selector,arg[1]);
		end = scannubit16(arg[2]);
		if(end < ptr) seterr(2);
		if(!nErrPos) {
			nbyte = (t_nubit8) narg - 3;
			for(i = ptr,j = 0;i <= end;++i,++j) {
				val = scannubit8(arg[j%nbyte+3]);
				if(!nErrPos) vramRealByte(seg,i) = val;
				else return;
			}
		}
	}
}
/* go */
static void rprintregs();
static void g()
{
	if (device.flagRun) {
		utilsPrint("NXVM is already running.\n");
		return;
	}
	switch(narg) {
	case 1:
		vdebug.flagbreak = 0;
		break;
	case 2:
		vdebug.flagbreak = 1;
		addrparse(vcpu.cs.selector,arg[1]);
		vdebug.breakcs = seg;
		vdebug.breakip = ptr;
		break;
	case 3:
		vdebug.flagbreak = 1;
		addrparse(vcpu.cs.selector,arg[1]);
		if (vcpuinsLoadSreg(&vcpu.cs, seg)) {
			utilsPrint("debug: fail to load cs from %04X\n", seg);
			return;
		}
		_ip = ptr;
		addrparse(vcpu.cs.selector,arg[2]);
		vdebug.breakcs = seg;
		vdebug.breakip = ptr;
		break;
	default:seterr(narg-1);break;}
	if(nErrPos) return;
	machineResume();
	while (device.flagRun) utilsSleep(1);
	vdebug.flagbreak = 0;
	rprintregs();
}
/* hex */
static void h()
{
	t_nubit16 val1,val2;
	if(narg != 3) seterr(narg-1);
	else {
		val1 = scannubit16(arg[1]);
		val2 = scannubit16(arg[2]);
		if(!nErrPos) {
			utilsPrint("%04X",GetMax16(val1+val2));
			utilsPrint("  ");
			utilsPrint("%04X",GetMax16(val1-val2));
			utilsPrint("\n");
		}
	}
}
/* input */
static void i()
{
	t_nubit16 in;
	if(narg != 2) seterr(narg-1);
	else {
		in = scannubit16(arg[1]);
		if (nErrPos) return;
		ExecFun(vport.in[in]);
		utilsPrint("%08X\n", vport.iodword);
	}
}
/* load */
static void l()
{
	t_nubit8 c;
	t_nubit16 i = 0;
	t_nubit32 len = 0;
	FILE *load = FOPEN(strFileName,"rb");
	if(!load) utilsPrint("File not found\n");
	else {
		switch(narg) {
		case 1:
			seg = vcpu.cs.selector;
			ptr = 0x100;
			break;
		case 2:
			addrparse(vcpu.cs.selector,arg[1]);
			break;
		default:seterr(narg-1);break;}
		if(!nErrPos) {
			c = fgetc(load);
			while(!feof(load)) {
				vramRealByte(seg+i,ptr+len++) = c;
				i = len / 0x10000;
				c = fgetc(load);
			}
			_cx = len&0xffff;
			if(len > 0xffff) _bx = (len>>16);
			else _bx = 0x0000;
		}
		fclose(load);
	}
}
/* move */
static void m()
{
	int i;
	t_nubit16 seg1,ptr1,range,seg2,ptr2;
	if(narg != 4) seterr(narg-1);
	else {
		addrparse(vcpu.ds.selector,arg[1]);
		seg1 = seg;
		ptr1 = ptr;
		addrparse(vcpu.ds.selector,arg[3]);
		seg2 = seg;
		ptr2 = ptr;
		range = scannubit16(arg[2])-ptr1;
		if(!nErrPos) {
			if(((seg1<<4)+ptr1) < ((seg2<<4)+ptr2)) {
				for(i = range;i >= 0;--i)
					vramRealByte(seg2,ptr2+i) = vramRealByte(seg1,ptr1+i);
			} else if(((seg1<<4)+ptr1) > ((seg2<<4)+ptr2)) {
				for(i = 0;i <= range;++i)
					vramRealByte(seg2,ptr2+i) = vramRealByte(seg1,ptr1+i);
			}
		}
	}
}
/* name */
static void n()
{
	if(narg != 2) seterr(narg-1);
	else STRCPY(strFileName,arg[1]);
}
/* output */
static void o()
{
	t_nubit16 out;
	if(narg != 3) seterr(narg-1);
	else {
		out = scannubit16(arg[1]);
		if (nErrPos) return;
		vport.iodword = scannubit32(arg[2]);
		if (nErrPos) return;
		ExecFun(vport.out[out]);
	}
}
/* quit */
static void q()
{flagexit = 1;}
/* register */
static t_nubit8 uprintins(t_nubit16 seg, t_nubit16 off)
{
	t_nubitcc i;
	t_nubit8 len;
	t_nubit8  ucode[15];
	t_string str, stmt, sbin;
	if (vcpuinsReadLinear((seg << 4) + off, (t_vaddrcc)ucode, 15)) {
		len = 0;
		SPRINTF(str, "%04X:%04X <ERROR>", seg, off);
	} else {
		len = dasm32(stmt, (t_vaddrcc)ucode);
		sbin[0] = 0;
		for (i = 0;i < len;++i) SPRINTF(sbin, "%s%02X", sbin, GetMax8(ucode[i]));
		SPRINTF(str, "%04X:%04X %s", seg, off, sbin);
		for (i = strlen(str);i < 24;++i) STRCAT(str, " ");
		STRCAT(str, stmt);
	}
	utilsPrint("%s\n", str);
	return len;
}
static void rprintflags()
{
	utilsPrint("%s ", _GetEFLAGS_OF ? "OV" : "NV");
	utilsPrint("%s ", _GetEFLAGS_DF ? "DN" : "UP");
	utilsPrint("%s ", _GetEFLAGS_IF ? "EI" : "DI");
	utilsPrint("%s ", _GetEFLAGS_SF ? "NG" : "PL");
	utilsPrint("%s ", _GetEFLAGS_ZF ? "ZR" : "NZ");
	utilsPrint("%s ", _GetEFLAGS_AF ? "AC" : "NA");
	utilsPrint("%s ", _GetEFLAGS_PF ? "PE" : "PO");
	utilsPrint("%s ", _GetEFLAGS_CF ? "CY" : "NC");
}
static void rprintregs()
{
	utilsPrint(  "AX=%04X", _ax);
	utilsPrint("  BX=%04X", _bx);
	utilsPrint("  CX=%04X", _cx);
	utilsPrint("  DX=%04X", _dx);
	utilsPrint("  SP=%04X", _sp);
	utilsPrint("  BP=%04X", _bp);
	utilsPrint("  SI=%04X", _si);
	utilsPrint("  DI=%04X", _di);
	utilsPrint("\nDS=%04X", vcpu.ds.selector);
	utilsPrint("  ES=%04X", vcpu.es.selector);
	utilsPrint("  SS=%04X", vcpu.ss.selector);
	utilsPrint("  CS=%04X", vcpu.cs.selector);
	utilsPrint("  IP=%04X", _ip);
	utilsPrint("   ");
	rprintflags();
	utilsPrint("\n");
	uprintins(vcpu.cs.selector, _ip);
	uasmSegRec = vcpu.cs.selector;
	uasmPtrRec = _ip;
}
static void rscanregs()
{
	t_nubit16 value;
	char s[MAXLINE];
	if(!STRCMP(arg[1],"ax")) {
		utilsPrint("AX ");
		utilsPrint("%04X",_ax);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_ax = value;
	} else if(!STRCMP(arg[1],"bx")) {
		utilsPrint("BX ");
		utilsPrint("%04X",_bx);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_bx = value;
	} else if(!STRCMP(arg[1],"cx")) {
		utilsPrint("CX ");
		utilsPrint("%04X",_cx);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_cx = value;
	} else if(!STRCMP(arg[1],"dx")) {
		utilsPrint("DX ");
		utilsPrint("%04X",_dx);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_dx = value;
	} else if(!STRCMP(arg[1],"bp")) {
		utilsPrint("BP ");
		utilsPrint("%04X",_bp);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_bp = value;
	} else if(!STRCMP(arg[1],"sp")) {
		utilsPrint("SP ");
		utilsPrint("%04X",_sp);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_sp = value;
	} else if(!STRCMP(arg[1],"si")) {
		utilsPrint("SI ");
		utilsPrint("%04X",_si);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_si = value;
	} else if(!STRCMP(arg[1],"di")) {
		utilsPrint("DI ");
		utilsPrint("%04X",_di);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_di = value;
	} else if(!STRCMP(arg[1],"ss")) {
		utilsPrint("SS ");
		utilsPrint("%04X",vcpu.ss.selector);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			if (vcpuinsLoadSreg(&vcpu.ss, GetMax16(value)))
				utilsPrint("debug: fail to load ss from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1],"cs")) {
		utilsPrint("CS ");
		utilsPrint("%04X",vcpu.cs.selector);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			if (vcpuinsLoadSreg(&vcpu.cs, GetMax16(value)))
				utilsPrint("debug: fail to load cs from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1],"ds")) {
		utilsPrint("DS ");
		utilsPrint("%04X",vcpu.ds.selector);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			if (vcpuinsLoadSreg(&vcpu.ds, GetMax16(value)))
				utilsPrint("debug: fail to load ds from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1],"es")) {
		utilsPrint("ES ");
		utilsPrint("%04X",vcpu.es.selector);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			if (vcpuinsLoadSreg(&vcpu.es, GetMax16(value)))
				utilsPrint("debug: fail to load es from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1],"ip")) {
		utilsPrint("IP ");
		utilsPrint("%04X",_ip);
		utilsPrint("\n:");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_ip = value;
	} else if(!STRCMP(arg[1],"f")) {
		rprintflags();
		utilsPrint(" -");
		FGETS(s,MAXLINE,stdin);
		utilsLowerStr(s);
		if(!STRCMP(s,"ov"))      _SetEFLAGS_OF;
		else if(!STRCMP(s,"nv")) _ClrEFLAGS_OF;
		else if(!STRCMP(s,"dn")) _SetEFLAGS_DF;
		else if(!STRCMP(s,"up")) _ClrEFLAGS_DF;
		else if(!STRCMP(s,"ei")) _SetEFLAGS_IF;
		else if(!STRCMP(s,"di")) _ClrEFLAGS_IF;
		else if(!STRCMP(s,"ng")) _SetEFLAGS_SF;
		else if(!STRCMP(s,"pl")) _ClrEFLAGS_SF;
		else if(!STRCMP(s,"zr")) _SetEFLAGS_ZF;
		else if(!STRCMP(s,"nz")) _ClrEFLAGS_ZF;
		else if(!STRCMP(s,"ac")) _SetEFLAGS_AF;
		else if(!STRCMP(s,"na")) _ClrEFLAGS_AF;
		else if(!STRCMP(s,"pe")) _SetEFLAGS_PF;
		else if(!STRCMP(s,"po")) _ClrEFLAGS_PF;
		else if(!STRCMP(s,"cy")) _SetEFLAGS_CF;
		else if(!STRCMP(s,"nc")) _ClrEFLAGS_CF;
		else utilsPrint("bf Error\n");
	} else utilsPrint("br Error\n");
}
static void r()
{
	if(narg == 1) {
		rprintregs();
	} else if(narg == 2) {
		rscanregs();
	} else seterr(2);
}
/* search */
static void s()
{
	t_nubit8 i;
	t_bool flagfound = 0;
	t_nubit16 p,pfront,start,end;
	t_nubit8 cstart;
	if(narg < 4) seterr(narg-1);
	else {
		addrparse(vcpu.ds.selector,arg[1]);
		start = ptr;
		end = scannubit16(arg[2]);
		if(!nErrPos) {
			p = start;
			cstart = scannubit8(arg[3]);
			while(p <= end) {
				if(vramRealByte(seg,p) == cstart) {
					pfront = p;
					flagfound = 1;
					for(i = 3;i < narg;++i) {
						if(vramRealByte(seg,p) != scannubit8(arg[i])) {
							flagfound = 0;
							p = pfront+1;
							break;
						} else ++p;
					}
					if(flagfound) {
						utilsPrint("%04X:%04X  ", seg, pfront);
						utilsPrint("\n");
					}
				} else ++p;
			}
		}
	}
}
/* trace */
static void t()
{
	t_nubit16 i;
	t_nubit16 count;
	if (device.flagRun) {
		utilsPrint("NXVM is already running.\n");
		return;
	}
	switch(narg) {
	case 1:
		count = 1;
		break;
	case 2:
		count = scannubit16(arg[1]);
		break;
	case 3:
		addrparse(vcpu.cs.selector,arg[1]);
		if (vcpuinsLoadSreg(&vcpu.cs, seg)) {
			utilsPrint("debug: fail to load cs from %04X\n", seg);
			return;
		}
		_ip = ptr;
		count = scannubit16(arg[2]);
		break;
	default:seterr(narg-1);break;}
	if(nErrPos) return;
	if (count < 0x0100) {
		for(i = 0;i < count;++i) {
			vdebug.tracecnt = 0x01;
			machineResume();
			while (device.flagRun) utilsSleep(10);
			rprintregs();
			if (i != count - 1) utilsPrint("\n");
		}
	} else {
		vdebug.tracecnt = count;
		machineResume();
		while (device.flagRun) utilsSleep(10);
		rprintregs();
	}
	vdebug.tracecnt = 0x00;
//	gexec(ptr1,ptr2);
}
/* unassemble */
static void uprint(t_nubit16 segment,t_nubit16 start,t_nubit16 end)
{
	t_nubit8 len;
	t_nubit32 boundary;
	if(start > end) end = 0xffff;
	if ((t_nubit32)((segment<<4) + end) > 0xfffff) end = (0xfffff-(segment<<4));
	while(start <= end) {
		len = uprintins(segment, start);
		start += len;
		boundary = (t_nubit32)start + (t_nubit32)len;
		if (boundary > 0xffff) break;
	}
	uasmSegRec = segment;
	uasmPtrRec = start;
	return;
}
static void u()
{
	t_nubit16 ptr2;
	if(narg == 1) uprint(uasmSegRec,uasmPtrRec,uasmPtrRec+0x1f);
	else if(narg == 2) {
		addrparse(vcpu.cs.selector,arg[1]);
		if(nErrPos) return;
		uprint(seg,ptr,ptr+0x1f);
	} else if(narg == 3) {
		addrparse(vcpu.ds.selector,arg[1]);
		ptr2 = scannubit16(arg[2]);
		if(nErrPos) return;
		if(ptr > ptr2) seterr(2);
		else uprint(seg,ptr,ptr2);
	} else seterr(3);
}
/* verbal */
static void v()
{
	t_nubitcc i;
	char str[MAXLINE];
	utilsPrint(":");
	FGETS(str,MAXLINE,stdin);
	str[strlen(str)-1] = '\0';
	for(i = 0;i < strlen(str);++i) {
		utilsPrint("%02X",str[i]);
		if(!((i+1)%0x10)) utilsPrint("\n"); 
		else if(!((i+1)%0x08)&&(str[i+1]!='\0')) utilsPrint("-");
		else utilsPrint(" ");
	}
	if(i%0x10) utilsPrint("\n");
}
/* write */
static void w()
{
	t_nubit16 i = 0;
	t_nubit32 len = (_bx<<16)+_cx;
	FILE *write;
	if(!strlen(strFileName)) {utilsPrint("(W)rite error, no destination defined\n");return;}
	else write= FOPEN(strFileName,"wb");
	if(!write) utilsPrint("File not found\n");
	else {
		utilsPrint("Writing ");
		utilsPrint("%04X",_bx);
		utilsPrint("%04X",_cx);
		utilsPrint(" bytes\n");
		switch(narg) {
		case 1:
			seg = vcpu.cs.selector;
			ptr = 0x100;
			break;
		case 2:
			addrparse(vcpu.cs.selector,arg[1]);
			break;
		default:	seterr(narg-1);break;}
		if(!nErrPos)
			while(i < len)
				fputc(vramRealByte(seg,ptr+i++),write);
		fclose(write);
	}
}
/* DEBUG CMD END */

/* EXTENDED DEBUG CMD BEGIN */
t_nubit32 xalin;
t_nubit32 xdlin;
t_nubit32 xulin;
/* print */
static t_nubit8 xuprintins(t_nubit32 linear)
{
	t_nubitcc i;
	t_nubit8 len;
	t_nubit8  ucode[15];
	t_string str, stmt, sbin;
	if (vcpuinsReadLinear(linear, (t_vaddrcc)ucode, 15)) {
		len = 0;
		SPRINTF(str, "L%08X <ERROR>", linear);
	} else {
		len = dasm32(stmt, (t_vaddrcc)ucode);
		sbin[0] = 0;
		for (i = 0;i < len;++i) SPRINTF(sbin, "%s%02X", sbin, GetMax8(ucode[i]));
		SPRINTF(str, "L%08X %s ", linear, sbin);
		for (i = strlen(str);i < 24;++i) STRCAT(str, " ");
		STRCAT(str, stmt);
	}
	utilsPrint("%s\n", str);
	return len;
}
static void xrprintreg()
{
	devicePrintCpuReg();
	xulin = vcpu.cs.base + _eip;
	xuprintins(xulin);
}
/* assemble */
static void xaconsole(t_nubit32 linear)
{
	t_nubitcc i;
	t_nubit8 len;
	t_string astmt;
	t_nubit8 acode[15];
	t_nubitcc errAsmPos;
	t_bool flagexitasm = 0;
	while(!flagexitasm) {
		utilsPrint("L%08X ", linear);
		FGETS(astmt, MAXLINE, stdin);
		fflush(stdin);
		astmt[strlen(astmt) - 1] = 0;
		if(!strlen(astmt)) {
			flagexitasm = 1;
			continue;
		}
		errAsmPos = 0;
		len = aasm32(astmt, (t_vaddrcc)acode);
		if(!len) errAsmPos = (t_nubitcc)strlen(astmt) + 9;
		else {
			if (vcpuinsWriteLinear(linear, (t_vaddrcc)acode, len)) {
				utilsPrint("debug: fail to write to L%08X\n", linear);
				return;
			}
			linear += len;
		}
		if(errAsmPos) {
			for(i = 0;i < errAsmPos;++i) utilsPrint(" ");
			utilsPrint("^ Error\n");
		}
	}
	xalin = linear;
}
static void xa()
{
	if(narg == 1) xaconsole(xalin);
	else if(narg == 2) {
		xalin = scannubit32(arg[1]);
		if(nErrPos) return;
		xaconsole(xalin);
	} else seterr(2);
}
/* compare */
static void xc()
{
	t_nubit32 i;
	t_nubit32 count;
	t_nubit32 lin1, lin2;
	t_nubit8 val1, val2;
	if(narg != 4) seterr(narg-1);
	else {
		lin1 = scannubit32(arg[1]);
		if(nErrPos) return;
		lin2 = scannubit32(arg[2]);
		if(nErrPos) return;
		count = scannubit32(arg[3]);
		if(nErrPos) return;
		if (!count) return;
		for (i = 0;i < count;++i) {
			if (vcpuinsReadLinear(lin1 + i, GetRef(val1), 1)) {
				utilsPrint("debug: fail to read from L%08X.\n", lin1 + i);
				return;
			}
			if (vcpuinsReadLinear(lin2 + i, GetRef(val2), 1)) {
				utilsPrint("debug: fail to read from L%08X.\n", lin2 + i);
				return;
			}
			if(val1 != val2)
				utilsPrint("L%08X  %02X  %02X  L%08X\n",
					lin1 + i, val1, val2, lin2 + i);
		}
	}
}
/* dump */
static void xdprint(t_nubit32 linear,t_nubit32 count)
{
	char t,c[0x11];
	t_nubit32 ilinear;
	t_nubit32 start = linear;
	t_nubit32 end = linear + count - 1;
	c[0x10] = '\0';
	if (!count) return;
	if (end < start) end = 0xffffffff;
	for(ilinear = start - (start % 0x10);ilinear <= end + 0x0f - (end % 0x10);++ilinear) {
		if (ilinear % 0x10 == 0) utilsPrint("L%08X  ", ilinear);
		if (ilinear < start || ilinear > end) {
			utilsPrint("  ");
			c[ilinear % 0x10] = ' ';
		} else {
			if (vcpuinsReadLinear(ilinear, GetRef(c[ilinear % 0x10]), 1)) {
				utilsPrint("debug: fail to read from L%08X\n", ilinear);
				return;
			} else {
				utilsPrint("%02X",c[ilinear % 0x10] & 0xff);
				t = c[ilinear % 0x10];
				if((t >=1 && t <= 7) || t == ' ' ||
					(t >=11 && t <= 12) ||
					(t >=14 && t <= 31) ||
					(t >=33)) ;
				else c[ilinear%0x10] = '.';
			}
		}
		utilsPrint(" ");
		if(ilinear % 0x10 == 7 && ilinear >= start && ilinear < end) utilsPrint("\b-");
		if((ilinear + 1) % 0x10 == 0) {
			utilsPrint("  %s\n",c);
		}
		if (ilinear == 0xffffffff) break;
	}
	xdlin = ilinear;
}
static void xd()
{
	t_nubit32 count;
	if(narg == 1) xdprint(xdlin, 0x80);
	else if(narg == 2) {
		xdlin = scannubit32(arg[1]);
		if(nErrPos) return;
		xdprint(xdlin, 0x80);
	} else if(narg == 3) {
		xdlin = scannubit32(arg[1]);
		count = scannubit32(arg[2]);
		if(nErrPos) return;
		xdprint(xdlin, count);
	} else seterr(3);
}
/* enter */
static void xe()
{
	t_nubit8 i, val;
	t_nubit32 linear;
	t_string s;
	if(narg == 1) seterr(0);
	else if(narg == 2) {
		linear = scannubit32(arg[1]);
		if(nErrPos) return;
		if (vcpuinsReadLinear(linear, GetRef(val), 1)) {
			utilsPrint("debug: fail to read from L%08X.\n", linear);
			return;
		}
		utilsPrint("L%08X  %02X.", linear, val);
		FGETS(s,MAXLINE,stdin);
		utilsLowerStr(s);
		val = scannubit8(s);
		if(nErrPos) return;
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			if (vcpuinsWriteLinear(linear, GetRef(val), 1))
				utilsPrint("debug: fail to write to L%08X.\n", linear);
	} else if(narg > 2) {
		linear = scannubit32(arg[1]);
		if(nErrPos) return;
		for(i = 2;i < narg;++i) {
			val = scannubit8(arg[i]);
			if(!nErrPos) {
				if (vcpuinsWriteLinear(linear, GetRef(val), 1)) {
					utilsPrint("debug: fail to write to L%08X.\n", linear);
					return;
				}
			} else break;
			linear++;
		}
	}
}
/* fill */
static void xf()
{
	t_nubit8  val;
	t_nubit32 count;
	t_nubit32 i, j;
	t_nubit32 bcount;
	t_nubit32 linear;
	if(narg < 4) seterr(narg-1);
	else {
		linear = scannubit32(arg[1]);
		if (nErrPos) return;
		count = scannubit32(arg[2]);
		if (nErrPos) return;
		bcount = (t_nubit32) narg - 3;
		for (i = 0, j = 0;i < count;++i, ++j) {
			val = scannubit8(arg[j % bcount + 3]);
			if (nErrPos) return;
			if (vcpuinsWriteLinear(linear + i, GetRef(val), 1)) {
				utilsPrint("debug: fail to write to L%08X.\n", linear + i);
				return;
			}
		}
	}
}
/* go */
static void xg()
{
	t_nubit32 i;
	t_nubit32 count = 0;
	if (device.flagRun) {
		utilsPrint("NXVM is already running.\n");
		return;
	}
	switch(narg) {
	case 1:
		vdebug.flagbreakx = 0;
		count = 1;
		break;
	case 2:
		vdebug.flagbreakx = 1;
		vdebug.breaklinear = scannubit32(arg[1]);
		count = 1;
		break;
	case 3:
		vdebug.flagbreakx = 1;
		vdebug.breaklinear = scannubit32(arg[1]);
		count = scannubit32(arg[2]);
		break;
	default:seterr(narg-1);break;}
	if(nErrPos) return;
	for (i = 0;i < count;++i) {
		vdebug.breakcnt = 0;
		machineResume();
		while (device.flagRun) utilsSleep(1);
		utilsPrint("%d instructions executed before the break point.\n", vdebug.breakcnt);
		xrprintreg();
	}
	vdebug.flagbreakx = 0;
}
/* move */
static void xm()
{
	t_nubit8 val;
	t_nubit32 i, lin1, lin2, count;
	if(narg != 4) seterr(narg-1);
	else {
		lin1 = scannubit32(arg[1]);
		if (nErrPos) return;
		lin2 = scannubit32(arg[2]);
		if (nErrPos) return;
		count = scannubit32(arg[3]);
		if (nErrPos) return;
		for (i = 0;i < count;++i) {
			if (vcpuinsReadLinear(lin1 + i, GetRef(val), 1)) {
				utilsPrint("debug: fail to read from L%08X.\n", lin1 + i);
				return;
			}
			if (vcpuinsWriteLinear(lin2 + i, GetRef(val), 1)) {
				utilsPrint("debug: fail to write to L%08X.\n", lin2 + i);
				return;
			}
		}
	}
}
/* search */
static void xs()
{
	t_nubit32 linear, count, bcount, i;
	t_nubit8 val, mem[256], line[256];
	if(narg < 4) seterr(narg-1);
	else {
		linear = scannubit32(arg[1]);
		if (nErrPos) return;
		count = scannubit32(arg[2]);
		if (nErrPos) return;
		addrparse(vcpu.ds.selector,arg[1]);
		bcount = (t_nubit32) narg - 3;
		for (i = 0;i < bcount;++i) {
			val = scannubit8(arg[i + 3]);
			if (nErrPos) return;
			line[i] = val;
		}
		for (i = 0;i < count;++i) {
			if (vcpuinsReadLinear(linear + i, (t_vaddrcc)mem, bcount)) {
				utilsPrint("debug: fail to read from L%08X.\n", linear + i);
				return;
			}
			if (!memcmp(mem, line, bcount)) {
				utilsPrint("L%08X\n", linear + i);
			}
		}
	}
}
/* trace */
static void xt()
{
	t_nubit32 i;
	t_nubit32 count;
	if (device.flagRun) {
		utilsPrint("NXVM is already running.\n");
		return;
	}
	switch(narg) {
	case 1:
		count = 1;
		break;
	case 2:
		count = scannubit32(arg[1]);
		break;
	default:seterr(narg-1);break;}
	if(nErrPos) return;
	if (count < 0x0100) {
		for(i = 0;i < count;++i) {
			vdebug.tracecnt = 0x01;
			machineResume();
			while (device.flagRun) utilsSleep(10);
			devicePrintCpuMem();
			xrprintreg();
			if (i != count - 1) utilsPrint("\n");
		}
	} else {
		vdebug.tracecnt = count;
		machineResume();
		while (device.flagRun) utilsSleep(10);
		devicePrintCpuMem();
		xrprintreg();
	}
	vdebug.tracecnt = 0x00;
//	gexec(ptr1,ptr2);
}
/* register */
static void xrscanreg()
{
	t_nubit32 value;
	t_string s;
	if(!STRCMP(arg[1], "eax")) {
		utilsPrint("EAX ");
		utilsPrint("%08X", _eax);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_eax = value;
	} else if(!STRCMP(arg[1], "ecx")) {
		utilsPrint("ECX ");
		utilsPrint("%08X", _ecx);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_ecx = value;
	} else if(!STRCMP(arg[1], "edx")) {
		utilsPrint("EDX ");
		utilsPrint("%08X", _edx);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_edx = value;
	} else if(!STRCMP(arg[1], "ebx")) {
		utilsPrint("EBX ");
		utilsPrint("%08X", _ebx);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_ebx = value;
	} else if(!STRCMP(arg[1], "esp")) {
		utilsPrint("ESP ");
		utilsPrint("%08X", vcpu.esp);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			vcpu.esp = value;
	} else if(!STRCMP(arg[1], "ebp")) {
		utilsPrint("EBP ");
		utilsPrint("%08X", _ebp);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_ebp = value;
	} else if(!STRCMP(arg[1], "esi")) {
		utilsPrint("ESI ");
		utilsPrint("%08X", vcpu.esi);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			vcpu.esi = value;
	} else if(!STRCMP(arg[1], "edi")) {
		utilsPrint("EDI ");
		utilsPrint("%08X", _edi);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_edi = value;
	} else if(!STRCMP(arg[1], "eip")) {
		utilsPrint("EIP ");
		utilsPrint("%08X", _eip);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_eip = value;
	} else if(!STRCMP(arg[1], "eflags")) {
		utilsPrint("EFLAGS ");
		utilsPrint("%08X", _eflags);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			_eflags = value;
	} else if(!STRCMP(arg[1], "es")) {
		devicePrintCpuSreg();
		utilsPrint(":");
		FGETS(s, MAXLINE, stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			if (vcpuinsLoadSreg(&vcpu.es, GetMax16(value)))
				utilsPrint("debug: fail to load es from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1], "cs")) {
		devicePrintCpuSreg();
		utilsPrint(":");
		FGETS(s, MAXLINE, stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			if (vcpuinsLoadSreg(&vcpu.cs, GetMax16(value)))
				utilsPrint("debug: fail to load cs from %04X\n", GetMax16(value));
	}  else if(!STRCMP(arg[1], "ss")) {
		devicePrintCpuSreg();
		utilsPrint(":");
		FGETS(s, MAXLINE, stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			if (vcpuinsLoadSreg(&vcpu.ss, GetMax16(value)))
				utilsPrint("debug: fail to load ss from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1], "ds")) {
		devicePrintCpuSreg();
		utilsPrint(":");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos) 
			if (vcpuinsLoadSreg(&vcpu.ds, GetMax16(value)))
				utilsPrint("debug: fail to load ds from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1], "fs")) {
		devicePrintCpuSreg();
		utilsPrint(":");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos) 
			if (vcpuinsLoadSreg(&vcpu.fs, GetMax16(value)))
				utilsPrint("debug: fail to load fs from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1], "gs")) {
		devicePrintCpuSreg();
		utilsPrint(":");
		FGETS(s,MAXLINE,stdin);
		value = scannubit16(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos) 
			if (vcpuinsLoadSreg(&vcpu.gs, GetMax16(value)))
				utilsPrint("debug: fail to load gs from %04X\n", GetMax16(value));
	} else if(!STRCMP(arg[1], "cr0")) {
		utilsPrint("CR0 ");
		utilsPrint("%08X", vcpu.cr1);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			vcpu.cr0 = value;
	} else if(!STRCMP(arg[1], "cr2")) {
		utilsPrint("CR2 ");
		utilsPrint("%08X", vcpu.cr2);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			vcpu.cr2 = value;
	} else if(!STRCMP(arg[1], "cr3")) {
		utilsPrint("CR3 ");
		utilsPrint("%08X", vcpu.cr3);
		utilsPrint("\n:");
		FGETS(s, MAXLINE, stdin);
		value = scannubit32(s);
		if(s[0] != '\0' && s[0] != '\n' && !nErrPos)
			vcpu.cr3 = value;
	} else utilsPrint("br Error\n");
}
static void xr()
{
	if(narg == 1) {
		xrprintreg();
	} else if(narg == 2) {
		xrscanreg();
	} else seterr(2);
}
/* unassemble */
static void xuprint(t_nubit32 linear, t_nubit8 count)
{
	t_nubit32 len = 0;
	t_nubit8 i;
	for (i = 0;i < count;++i) {
		len = xuprintins(linear);
		if (!len) break;
		linear += len;
	}
	xulin = linear;
}
static void xu()
{
	t_nubit32 count;
	if(narg == 1) xuprint(xulin, 10);
	else if(narg == 2) {
		xulin = scannubit32(arg[1]);
		if(nErrPos) return;
		xuprint(xulin, 0x10);
	} else if(narg == 3) {
		xulin = scannubit32(arg[1]);
		count = scannubit32(arg[2]);
		if(nErrPos) return;
		xuprint(xulin, count);
	} else seterr(3);
}
/* watch */
static void xw()
{
	switch(narg) {
	case 1:
		if (vcpuins.flagwr) utilsPrint("Watch-read point: Lin=%08x\n", vcpuins.wrlin);
		if (vcpuins.flagww) utilsPrint("Watch-write point: Lin=%08x\n", vcpuins.wwlin);
		if (vcpuins.flagwe) utilsPrint("Watch-exec point: Lin=%08x\n", vcpuins.welin);
		break;
	case 2:
		switch (arg[1][0]) {
		case 'r':
			vcpuins.flagwr = 0;
			utilsPrint("Watch-read point removed.\n");
			break;
		case 'w':
			vcpuins.flagww = 0;
			utilsPrint("Watch-write point removed.\n");
			break;
		case 'e':
			vcpuins.flagwe = 0;
			utilsPrint("Watch-exec point removed.\n");
			break;
		case 'u':
			vcpuins.flagwr = 0;
			vcpuins.flagww = 0;
			vcpuins.flagwe = 0;
			utilsPrint("All watch points removed.\n");
			break;
		default: seterr(1);break;}
		break;
	case 3:
		switch (arg[1][0]) {
		case 'r':
			vcpuins.flagwr = 1;
			vcpuins.wrlin = scannubit32(arg[2]);
			break;
		case 'w':
			vcpuins.flagww = 1;
			vcpuins.wwlin = scannubit32(arg[2]);
			break;
		case 'e':
			vcpuins.flagwe = 1;
			vcpuins.welin = scannubit32(arg[2]);
			break;
		default:
			seterr(2);
			break;
		}
		break;
	default:seterr(narg-1);break;}
}
static void xhelp()
{
	utilsPrint("assemble        XA [address]\n");
	utilsPrint("compare         XC addr1 addr2 count_byte\n");
	utilsPrint("dump            XD [address [count_byte]]\n");
	utilsPrint("enter           XE address [byte_list]\n");
	utilsPrint("fill            XF address count_byte byte_list\n");
	utilsPrint("go              XG [address [count_instr]]\n");
	utilsPrint("move            XM addr1 addr2 count_byte\n");
	utilsPrint("register        XR [register]\n");
	utilsPrint("  regular         XREG\n");
	utilsPrint("  segment         XSREG\n");
	utilsPrint("  control         XCREG\n");
	utilsPrint("search          XS address count_byte byte_list\n");
	utilsPrint("trace           XT [count_instr]\n");
	utilsPrint("unassemble      XU [address [count_instr]]\n");
	utilsPrint("watch           XW r/w/e address\n");
}
static void x()
{
	t_nubit8 i;
	arg[narg] = arg[0];
	for (i = 1;i < narg;++i) arg[i - 1] = arg[i];
	arg[narg - 1] = arg[narg];
	arg[narg] = NULL;
	narg--;
	if (!STRCMP(arg[0], "\?")) xhelp();
	else if (!STRCMP(arg[0], "a"))    xa();
	else if (!STRCMP(arg[0], "c"))    xc();
	else if (!STRCMP(arg[0], "d"))    xd();
	else if (!STRCMP(arg[0], "e"))    xe();
	else if (!STRCMP(arg[0], "f"))    xf();
	else if (!STRCMP(arg[0], "g"))    xg();
	else if (!STRCMP(arg[0], "m"))    xm();
	else if (!STRCMP(arg[0], "r"))    xr();
	else if (!STRCMP(arg[0], "s"))    xs();
	else if (!STRCMP(arg[0], "t"))    xt();
	else if (!STRCMP(arg[0], "u"))    xu();
	else if (!STRCMP(arg[0], "w"))    xw();
	else if (!STRCMP(arg[0], "reg"))  xrprintreg();
	else if (!STRCMP(arg[0], "sreg")) devicePrintCpuSreg();
	else if (!STRCMP(arg[0], "creg")) devicePrintCpuCreg();
	else {
		arg[0] = arg[narg];
		seterr(0);
	}
}
/* EXTENDED DEBUG CMD END */

/* main routines */
static void help() {
	utilsPrint("assemble        A [address]\n");
	utilsPrint("compare         C range address\n");
	utilsPrint("dump            D [range]\n");
	utilsPrint("enter           E address [list]\n");
	utilsPrint("fill            F range list\n");
	utilsPrint("go              G [[address] breakpoint]\n");
	/* utilsPrint("go              G [=address] [addresses]\n"); */
	utilsPrint("hex             H value1 value2\n");
	utilsPrint("input           I port\n");
	utilsPrint("load            L [address]\n");
	/* utilsPrint("load            L [address] [drive] [firstsector] [number]\n"); */
	utilsPrint("move            M range address\n");
	utilsPrint("name            N pathname\n");
	/* utilsPrint("name            N [pathname] [arglist]\n"); */
	utilsPrint("output          O port byte\n");
	/* !utilsPrint("proceed           P [nx=address] [number]\n"); */
	utilsPrint("quit            Q \n");
	utilsPrint("register        R [register]\n");
	utilsPrint("search          S range list\n");
	utilsPrint("trace           T [[address] value]\n");
	/* utilsPrint("trace           T [=address] [value]\n"); */
	utilsPrint("unassemble      U [range]\n");
	utilsPrint("verbal          V \n");
	utilsPrint("write           W [address]\n");
	utilsPrint("debug32         X?\n");
	/* utilsPrint("write           W [address] [drive] [firstsector] [number]\n"); */
	/* utilsPrint("allocate expanded memory        XA [#pages]\n"); */
	/* utilsPrint("deallocate expanded memory      XD [handle]\n"); */
	/* utilsPrint("map expanded memory pages       XM [Lpage] [Ppage] [handle]\n"); */
	/* utilsPrint("display expanded memory status  XS\n"); */
}

static void parse() {
	STRCPY(strCmdCopy, strCmdBuff);
	narg = 0;
	arg[0] = STRTOK(strCmdCopy, " ,\t\n\r\f");
	if (arg[narg]) {
		utilsLowerStr(arg[narg]);
		narg++;
	} else {
		return;
	}
	if (strlen(arg[narg-1]) != 1) {
		arg[narg] = arg[narg-1]+1;
		narg++;
	}
	while (narg < DEBUG_MAXNARG) {
		arg[narg] = STRTOK(NULL," ,\t\n\r\f");
		if (arg[narg]) {
			utilsLowerStr(arg[narg]);
			narg++;
		} else {
			break;
		}
	}
}

static void exec() {
	nErrPos = 0;
	if (!arg[0]) {
		return;
	}
	switch(arg[0][0]) {
	case '\?': help(); break;
	case 'a': a(); break;
	case 'c': c(); break;
	case 'd': d(); break;
	case 'e': e(); break;
	case 'f': f(); break;
	case 'g': g(); break;
	case 'h': h(); break;
	case 'i': i(); break;
	case 'l': l(); break;
	case 'm': m(); break;
	case 'n': n(); break;
	case 'o': o(); break;
	case 'q': q(); break;
	case 'r': r(); break;
	case 's': s(); break;
	case 't': t(); break;
	case 'u': u(); break;
	case 'v': v(); break;
	case 'w': w(); break;
	case 'x': x(); break;
	default: seterr(0); break;
	}
}

static void xasmTest() {
	static t_nubitcc total = 0;
	t_bool flagtextonly = 0; /* don't stop vmachine if true */
	t_nubit8 i, lend1, lend2, lena;
	t_string dstr1, dstr2;
	t_nubit8 ins1[15], ins2[15];
	total++;
	vcpuinsReadLinear(vcpu.cs.base + vcpu.eip, (t_vaddrcc) ins1, 15);
/*	ins1[0] = 0x67;
	ins1[1] = 0xc6;
	ins1[2] = 0x44;
	ins1[3] = 0xf2;
	ins1[4] = 0x05;
	ins1[5] = 0x8e;
	ins1[6] = 0x00;*/
	switch (d_nubit8(ins1)) {
	case 0x88: case 0x89: case 0x8a: case 0x8b:
	case 0x00: case 0x01: case 0x02: case 0x03:
	case 0x08: case 0x09: case 0x0a: case 0x0b:
	case 0x10: case 0x11: case 0x12: case 0x13:
	case 0x18: case 0x19: case 0x1a: case 0x1b:
	case 0x20: case 0x21: case 0x22: case 0x23:
	case 0x28: case 0x29: case 0x2a: case 0x2b:
	case 0x30: case 0x31: case 0x32: case 0x33:
	case 0x38: case 0x39: case 0x3a: case 0x3b:
		flagtextonly = 1;
		break;
	}
	switch (d_nubit8(ins1+1)) {
	case 0x90:
		flagtextonly = 1;
		break;
	}
	switch (d_nubit16(ins1)) {
	case 0x2e66:
		flagtextonly = 1;
		break;
	}
	switch (GetMax24(d_nubit24(ins1))) {
	case 0xb70f66:
		flagtextonly = 1;
		break;
	}
	switch (GetMax24(d_nubit24(ins1+1))) {
	case 0xb70f66:
		flagtextonly = 1;
		break;
	}
	switch (d_nubit32(ins1)) {
	}
	lend1 = dasm32(dstr1, (t_vaddrcc)ins1);
	lena  = aasm32(dstr1, (t_vaddrcc)ins2);
	lend2 = dasm32(dstr2, (t_vaddrcc)ins2);
	if ((!flagtextonly && (lena != lend1 || lena != lend2 || lend1 != lend2 ||
		memcmp(ins1, ins2, lend1))) || STRCMP(dstr1, dstr2)) {
		utilsPrint("diff at #%d %04X:%08X(L%08X), len(a=%x,d1=%x,d2=%x), CodeSegDefSize=%d\n",
			total, _cs, _eip, vcpu.cs.base + vcpu.eip, lena, lend1, lend2, vcpu.cs.seg.exec.defsize ? 32 : 16);
		for (i = 0;i < lend1;++i) {
			utilsPrint("%02X", ins1[i]);
		}
		utilsPrint("\t%s\n", dstr1);
		for (i = 0;i < lend2;++i) {
			utilsPrint("%02X", ins2[i]);
		}
		utilsPrint("\t%s\n", dstr2);
		machineStop();
	}
}

static void init() {
	memset(&vdebug, 0x00, sizeof(t_debug));
}

static void reset() {}

static void refresh() {
	vdebug.breakcnt++;
	if (vdebug.breakcnt && (
		(vdebug.flagbreak && _cs == vdebug.breakcs && _ip == vdebug.breakip) ||
		(vdebug.flagbreakx && (vcpu.cs.base + vcpu.eip == vdebug.breaklinear)))) {
		machineStop();
	}
	if (vdebug.tracecnt) {
		vdebug.tracecnt--;
		if (!vdebug.tracecnt) {
			machineStop();
		}
	}
//	xasmTest();
}

static void final() {}

void debugRegister() {
	vmachine.deviceTable[VMACHINE_DEVICE_INIT][vmachine.numDevices] = (t_faddrcc) init;
	vmachine.deviceTable[VMACHINE_DEVICE_RESET][vmachine.numDevices] = (t_faddrcc) reset;
	vmachine.deviceTable[VMACHINE_DEVICE_REFRESH][vmachine.numDevices] = (t_faddrcc) refresh;
	vmachine.deviceTable[VMACHINE_DEVICE_FINAL][vmachine.numDevices] = (t_faddrcc) final;
	vmachine.numDevices++;
}

void debugMain() {
	t_nubitcc i;
	strFileName[0] = '\0';
	asmSegRec = uasmSegRec = vcpu.cs.selector;
	asmPtrRec = uasmPtrRec = _ip;
	dumpSegRec = vcpu.ds.selector;
	dumpPtrRec = (t_nubit16)(_ip) / 0x10 * 0x10;
	xalin = 0;
	xdlin = 0;
	xulin = vcpu.cs.base + _eip;
	arg = (char **)malloc(DEBUG_MAXNARG * sizeof(char *));
	flagexit = 0;
	while (!flagexit) {
		fflush(stdin);
		utilsPrint("-");
		FGETS(strCmdBuff,MAXLINE,stdin);
		parse();
		exec();
		if (nErrPos) {
			for (i = 0;i < nErrPos;++i) utilsPrint(" ");
			utilsPrint("^ Error\n");
		}
	}
	free(arg);
}