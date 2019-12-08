/* 
   vbidecode.cc - videotext, intercast, VPS  and videocrypt VBI data decoder

   Copyright (C) 1997,1998 by Ralph Metzler (rjkm@thp.uni-koeln.de)
   All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    
*/

/*
TODO:

 - Videodat (if somebody can get me some documentation)
 - support NTSC Teletext, CC and intercast
 - RS error correction for intercast
   (did some experimenting, but nothing included here)
 - handling of data with bad CRC or hamming code
 - make everything byte order independent for Linux on big endian machines
   (pointers in intercast data are little endian)
 - support other data formats besides Bt848 raw data (e.g. YUV)
 - handling of interleaved intercast data 
 - make sense of strange directory structure transmission in DSF
 - lots more ... 

*/

#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strstream.h>
#include <errno.h>
#include <fstream.h>
#include <iostream.h>
#include <iomanip.h>
#include <dirent.h>
#include <sched.h>
#include <sys/stat.h>

#include "tables.h"

int verbosity=0;

#define LOG(v,l) {if (verbosity&(v)) {l;}}

#define HEX(N) hex << setw(N) << setfill(int('0')) 
#define DEC(N) dec << setw(N) << setfill(int('0')) 

void hdump(unsigned char *data, int len, unsigned char xor=0)
{
  int i;
  for (i=0; i<len; i++)
    cout << HEX(2) << int(xor^data[i]) << " ";
}

void hdump10(unsigned char *data, int len, int width=16)
{
  int i,j;
  for (i=0; i<len; i+=width) {
    for (j=0; j<width; j++)
      if (i+j < len) 
	cout << HEX(2) << int(data[i+j]) << " ";
      else
	cout << "   ";
    for (j = 0; j < width && i + j < len; j++)
      cout << char((data[i+j]>31 && data[i+j]<127) ? data[i+j] : '.');
    cout << "\n";
  }
}

void adump(unsigned char *data, int len, unsigned char xor=0)
{
  int i;
  unsigned char c;

  for (i=0; i<len; i++) {
    c=0x7f&(xor^data[i]);
    if (c<0x20)
      c='.';
    cout << char(c);
  }
}

void padump(unsigned char *data, int len)
{
  int i;
  unsigned char c;

  for (i=0; i<len; i++) {
    c=data[i];
    if (!(unhamtab[c]&0x80))
      c='@';
    c&=0x7f;
    if (c<0x20)
      c='.';
    cout << char(c);
  }
}

// unham 2 bytes into 1, report 2 bit errors but ignore them
inline unsigned char unham(unsigned char *d)
{
  unsigned char c1,c2;
  
  c1=unhamtab[d[0]];
  c2=unhamtab[d[1]];
  if ((c1|c2)&0x40)
    LOG (16, cout << "bad ham!";)
  return (c2<<4)|(0x0f&c1);
}


struct vpsinfo {
  char chname[9];
  int namep;
  unsigned char *info;
  
  vpsinfo() {
    namep=0;
  }
  void decode(unsigned char *data) {
    info=data;
    cout << "VPS: ";
    hdump(info+2,13);
    adump(info+2,13);
    
    if ((info[3]&0x80)) {
      chname[namep]=0;
      if (namep==8)
	cout << chname;
      namep=0;
    }
    chname[namep++]=info[3]&0x7f;
    if (namep==9) 
      namep=0;

    cout << "\n";
  }

  /* Who wants to add other VPS stuff (start/stop of VCR, etc. )??? */
  
};

struct vpsinfo vpsi;


/*******************************************************************************/

struct chan_name {
  chan_name *next;
  char *name;
};
static chan_name *channel_names=0;

void getcnames(void)
{
  DIR *dp;
  struct dirent *dirp;
  struct stat statbuf;

  dp=opendir(".");
  LOG(1, cout << "Data directories: ";)
  while ((dirp=readdir(dp)) !=NULL) {
    if (strcmp(dirp->d_name, ".") == 0 ||
	strcmp(dirp->d_name, "..") == 0)
      continue;
    lstat(dirp->d_name, &statbuf);
    if (S_ISDIR(statbuf.st_mode)) {
      chan_name *cn=new chan_name;
      cn->name=new char[strlen(dirp->d_name)+1];
      strcpy(cn->name, dirp->d_name);
      cn->next=channel_names;
      channel_names=cn;
      LOG(1,cout << cn->name << " ";)
    }
  }
  LOG(1,cout << endl;)
}


int nmatch(char *str, char *sub, int len) {
  int i, n=0; 
  for (i=0; i<len; i++) 
    if (str[i]=='\1' || str[i]==sub[i])
      n++;
  return n;
}


void filter(unsigned char *name,int len) {
  int i;
  unsigned char c;
  
  for (i=0; i< len; i++){
    c=name[i];
    if (!(unhamtab[c]&0x80)) 
      c='\1';
    c&=0x7f;
    if (c < ' ') 
      c=' ';
    switch (c) {
    case '/':
    case 0x23:
    case 0x24:
    case 0x40:
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f:
    case 0x60:
    case 0x7b:
    case 0x7c:
    case 0x7d:
    case 0x7e:
      c = '\1';
      break;
    }
    name[i]=c;
  }
};
  
#define VT_PAGESIZE 25*40
#define VTPAGE_DIRTY 1

struct VTpage {
  VTpage *next;
  unsigned int number; 
  unsigned char page[VT_PAGESIZE];
  ushort flags;

  /* try to match a sub-directory name to part of the header line */
  char *match(char *cname) {
    unsigned char c;
    int i, len,n;
    chan_name *names=channel_names;
    
    while (names) {
      len=strlen(names->name);
      for (i=0; i<24-len; i++) {
	n=nmatch(cname+i,names->name,len);
	if (n>len-1) {
	  //if (n!=len)
	  //cerr << "Partial match on " << names->name << "\n";
	  return names->name;
	}
      }
      names=names->next;
    }
    return (char *) 0;
  };

  void write(void) {
    char name[80];
    unsigned char cname[25];
    ushort pn;
    ostrstream oname(name,80);
    char *cmatch;

    memcpy(cname,page+8,24);
    filter(cname,24);
    cname[24]=0;
    cmatch=match((char *)cname);

    if (!cmatch) {
      LOG(1, cout << "No channel match for: ";
	  adump(page+8,24);
	  cout << "\n";)
      return;
    }
    pn=number&0xfff;
    if (!(pn&0xf00))
      pn+=0x900;
    oname //<< "/var/spool/vtx/" 
      << cmatch << "/" //"VTX/" 
	<< HEX(3) << pn << "_" << DEC(2) << int(number>>16) 
	  << ".vtx" << char(0);
    ofstream of(name);
    of << "VTXV4" << (unsigned char)(number&0xff) << (unsigned char)((pn>>8)&0xf)
      << (unsigned char)(number>>24) << (unsigned char)(number>>16);
    of << (unsigned char)(flags>>5)
      << (unsigned char)(0);
    of << (unsigned char)(0);
    of.write(page,40*24);
    of.close(); 
  };

  void setline(unsigned char *data, int line) {
    if (!line) {
      unsigned char c=unham(data+4);
      flags=unham(data+2)&0x80;
      flags|=(c&0x40)|((c>>2)&0x20);
      c=unham(data+6);
      /* Hmm, this is probably not completely right, or is it? */
      flags|=((c<<4)&0x10)|((c<<2)&0x08)|(c&0x04)|((c>>1)&0x02)|((c>>4)&0x01);
    }
    if (line<25)
      memcpy(page+40*line, data, 40);
    //cout << HEX(8) << number << " " << HEX(2) << line << ": ";
    //padump(data,40); cout << " VTPA\n";
    //hdump(data,40); cout << " VTH\n";

    if (line==23) 
      write();
  
/*
      cout << " PAGE " << number << "\n";
      for (int i=0; i<24; i++) {
	cout << "DP "; adump(page+40*i,40);
	cout << "\n";
      }
*/

  };

  VTpage(unsigned int pnum) : number(pnum) {
    next=0;
    flags=0;
  };
};

struct VTmagazine {
  VTpage *pages;
  VTpage *current;

  VTmagazine(void) {
    pages=current=NULL;
  };
  ~VTmagazine(void) {
    VTpage *p=pages, *pn;
    while (p) {
      pn=p->next;
      delete p;
      p=pn;
    }
  }
  void selectpage(unsigned int pagenum, unsigned char *header) {
    VTpage *p=pages;

    if ((pagenum&0xff)==0xff)
      return;
    while (p) {
      if (p->number==pagenum) {
	current=p;
	p->setline(header,0);
	return;
      }
      p=p->next;
    } 

    /* Comment this out, if you want to keep all pages in memory */
    /* actually, if you do NOT want all pages in memory (default) you can
       delete a lot of other stuff */
    if ((p=pages)) {
      pages=p->next;
      delete p;
    }

    p=new VTpage(pagenum);
    p->next=pages;
    pages=current=p;
    setline(header,0);
    //cout << "\nCreated page " << HEX(4) << pagenum << "\n";
  }
  void setline(unsigned char *data, int line) {
    if (current)
      current->setline(data,line);
  };
};

struct VTchannel {
  char name[20];
  VTmagazine mag[8];
  void selectpage(unsigned int pagenum, unsigned char *header) {
    mag[(pagenum>>8)&7].selectpage(pagenum,header);
  };
  void setline(unsigned char *data, int line, int magnum) {
    mag[magnum].setline(data,line);
  };
};

VTchannel vtch;



/*******************************************************************************/

/* Intercast decoder */
/* only handles the strange (and nowhere documented) format used on
   the German channels ZDF and DSF for now 
*/

struct ICdeco {
  unsigned char blocks[28*16];
  unsigned char data[0x100];
  unsigned char *fbuf;
  unsigned int packnum, packtot, packlen, total;
  int datap, ok, esc, ciok;
  unsigned char ci,lastci;  /* ci = continuity index */
  ushort dat;
  unsigned int done, length, flength;
  int pnum;
  char *name;

  /* This is where the actual data arrives from the lower level 
     bit reading routines
     */
  void setblock(int nr, unsigned char *block) {
    memcpy(blocks+28*nr,block,28);
    /* XXX: also check if all 15 blocks actually arrived */
    if (nr==0x0f)
      procblocks();
  }

  /* These "inner loops" can probably be optimized a lot */
  /* They handle the SLIP-like escape codes (according to the internet draft it
     should be EXACTLY like SLIP!?!?) which mark the separate packages
     */
  void adata(unsigned char c) {
    if (ok) {
      if (datap>=0x100)
	ok=0;
      else
	data[datap++]=c;
    }
  }
  void procbyte(unsigned char c) {
    if (!esc) {
      // 0x10 escapes 0x02, 0x03 and itself
      if (c==0x10) {
	esc=1;
	return;
      }
      // unescaped 0x02 starts a package
      if (c==0x02)
	newpack();
      adata(c);
      // unescaped 0x03 ends a package
      if (c==0x03)
	procpack();
    } else {
      esc=0;
      adata(c);
    }
  }

  /* put working error correction code here .. */
  void fecblocks() {
    unsigned char rsblock[256];
    memset(rsblock,0,256);
    memcpy(rsblock,blocks,26);
    //encode_rs(rsblock,rsblock+253);
    hdump10(blocks,28);
    hdump10(rsblock+253,2);
  }

  void procblocks(void) {
    //fecblocks();
    int i,j;
    unsigned char *block;
    for (i=0;i<14;i++) {
      block=blocks+28*i;
      for (j=0;j<26;j++)
	procbyte(block[j]);
    }
    memset(blocks,0,16*28);
  }
  ICdeco(void) {
    reset();
  }
  void reset(void) {
    fbuf=0;
    esc=0;
    done=0;
    pnum=0;
    ok=0;
  }
  void newpack() {
    datap=0;
    ok=1;
  }

  /* this handles a single data package 
     no time to explain this
     if you understand this and find something new plesae tell me! :-)
     */
  void procpack() {
    if (!ok)
      return;
    ok=0;
    length=datap-10;
    if (length!=data[6]) {
      //ciok=0;
      LOG(1,cout << "packet length mismatch!" << "\n";);
      return;
    }
    //    hdump10 (data,7);
    //   hdump10 (data+7,datap-10); cout << endl;
    ci=data[4];
    if (data[2]) {
      unsigned char *pack=data+7;
      if (!ci) {
	/* XXX: this works on little endian only!!! */
	packnum=*((unsigned int *)(pack+0x0c));
	packtot=*((unsigned int *)(pack+0x10));
	packlen=*((unsigned int *)(pack+0x14));
	total = *((unsigned int *)(pack+0x18));

	LOG(4,
	    cout << "\nICP: packet " << HEX(2) << packnum << " of "
	    << HEX(2) << packtot << ", ";
	    cout << "length: " << HEX(4) << packlen << ", ";
	    cout << "total: " << HEX(8) << total << "\n";
	    );
	if (packnum==1) {
	  lastci=0x0f;
	  done=0;
	  /* I keep the whole file in memory for now */
          /* go ahead and change this but note that you will need to buffer
             the last 2 packages since name and other information might
             be on a package boundary! 
	     */
	  fbuf=(unsigned char *)realloc((void *)fbuf,total);
	  if (!fbuf)
	    cerr << "Realloc failed!\n";
	}
	if (lastci==0x0f)
	  ciok=1;
	lastci=0;
	if (ciok && (length>0x38)) {
	  memcpy(fbuf+done,pack+0x38,length-0x38);
	  done+=length-0x38;
	}    
      } else {
	if (lastci+1!=ci)
	  ciok=0;
	lastci=ci;
	if (ciok) {
	  LOG(4,cout << ".";cout.flush());
	  memcpy(fbuf+done,pack,length);
	  done+=length;
	} else
 	  LOG(4,cout << "x";cout.flush(););
      }
      if (total && (done==total)) {
	unsigned int npos, ipos;
	int nlen;

	/* At fbuf+total-6 is a pointer to an info structure */
	ipos=*((unsigned int *)(fbuf+total-6));
	flength=*((unsigned int *)(fbuf+ipos+2));

	nlen=(int) fbuf[ipos+0x34];
	char name[nlen];
	if (fbuf[ipos+0x32]&0x80) 
	  npos=ipos+0x35;
	else
	  npos=*((unsigned int *)(fbuf+ipos+0x38));
	/* Only save the file if pointers pass some sanity checks */
	if (npos+nlen<total && ipos<total && flength<=total) {
	  strcpy(name,"IC/");
	  strncpy(name+3, (char *)fbuf+npos, nlen);
	  name[nlen+2]=0; // just to be sure ...
	  ofstream icfile(name);
	  icfile.write(fbuf,flength);
	  icfile.close();
	  cout << "\nFile: "  << name 
	    << ", length: " << HEX(8) << flength 
	      << "\n";
	} 

	ciok=0;
      }

    }
  }
};


/* XXX: Make this and possibly other (interleaved) intercast transmissions
   be allocated dynamically */
ICdeco icd;


/*******************************************************************************/

/* decode data in videotext-like packages, like videotext itself, intercast, ...
*/ 

void decode_vt(unsigned char *dat)
{
  int i,FL,NR;
  unsigned char mag, pack, mpag, flags, ftal, ft, al, page;
  unsigned int addr;
  static unsigned int pnum=0;
  ushort sub;
  char c;
  unsigned char udat[4];

  /* dat: 55 55 27 %MPAG% */
  mpag=unham(dat+3);
  mag=mpag&7;
  pack=(mpag>>3)&0x1f;
  
  switch (pack) {
  case 0:
    for (i=0; i<4; i++) 
      udat[i]=unham(dat+5+i*2);
    //hdump(udat,4); cout << " HD\n";
  
    /* dat: 55 55 27 %MPAG% %PAGE% %SUB% 
            00 01 02  03 04  05 06 07-0a 
     */
    page=unham(dat+5);
    if (page==0xff)
      break;
    sub=(unham(dat+9)<<8)|unham(dat+7);
    pnum = (mag<<8)|((sub&0x3f7f)<<16)|page;
/*    
    if (sub&0x80)
      cout << " Erase";
    if (sub&0x4000)
      cout << " Newsflash";
    if (sub&0x8000)
      cout << " Subtitle";
    flags=unham(dat+11);
    if (flags&0x01)
      cout << " SuppHeader";
    if (flags&0x02)
      cout << " Update";
    if (flags&0x04)
      cout << " ISequ";
    if (flags&0x08)
      cout << " IDisp";
    if (flags&0x10)
      cout << " MagSerial";
    cout << " Charset: " << int(flags>>5) << "\n";
*/
//    cout << HEX(4) << int(pnum);
//    cout << HEX(2) << int(unham(dat+9));
//    cout << HEX(2) << int(unham(dat+11));

    vtch.selectpage(pnum, dat+5);
    break;
  case 1 ... 24:
    vtch.setline(dat+5, pack, mag);
    break;
  case 25:
    /*    cout << "page " << HEX(4) << int(pnum)  << " ";
    cout << "AltHeader:"; adump(dat+5,40); cout << "\n";
    */
    break;
  case 26: // PDC 
  case 27:
  case 28:
  case 29:
    break;
  case 30:
    LOG(16,cout << "pack 30\n";)
    break;
  case 31:
    //cout << "mag" << int(mag) << "\n";
    ftal=unham(dat+5);
    al=ftal>>4;  /* address length */
    ft=ftal&0x0f;
    for (addr=0,i=0; i<al; i++) 
      addr=(addr<<4)|(0xf&unhamtab[dat[7+i]]);
    LOG(2,
	cout << "FT:" << HEX(1) << (int)ft;
	cout << " AL:" << HEX(1) << (int)al;
	cout << " ADDR:";
	cout << HEX(8) << addr;
	
	if (ft&4)
	  cout << " CI:" << HEX(2) << (int)dat[7+al];
	if (ft&8)
	  cout << " RI:" << HEX(2) << (int)dat[8+al];
	cout << "\n";
	)
    switch (addr) {
    case 0x07:
      //cout << " ??:" << HEX(2) << int(0xf&unhamtab[dat[9+al]]);
      break;
      
    case 0x0500:
      /* here we usually have the following structure: (03-0e are hammed)
       dat: 00 01 02  03 04  05 06  07-0c    0d  0e  0f-2a 2b-2c 
	      SYNC    MPAG    FTAL   ADDR    NR  FL   DATA   CRC  
	    55 55 27   31    00 06  000500   1-f 08     
      */
      FL=int(0xf&unhamtab[dat[8+al]]);  /* flags? */
      NR=int(0xf&unhamtab[dat[7+al]]);  /* line number */
      LOG(2,cout << " NR:" << HEX(1) << NR;)
      LOG(2,cout << " FL:" << HEX(1) << FL << "\n";)
      icd.setblock(NR,dat+9+al);        /* write data to the intercast decoder */
      if (!(FL&4)) {
	//cout << "\nICH: ";  hdump(dat+9+al,26); 
	//cout << "\nICA:"; adump(dat+9+al,26); 
	//cout << endl;
      }
      break;
    case 0x0f00: /* also used by ZDF and DSF, data format unknown */
      break;
    default:
      break;
    };
    LOG(2,
	cout << "ICH:\n";  
//	hdump10(dat+9+al,40-al-4); 
	hdump10(dat,45); 
	cout << "\n";
	);
    break;
  default:
    cout << "Packet " << dec << int(pack) << "\n";
    break;
  }
}



/***********************************************************************/

/* Low level decoder of raw VBI data 
   It calls the higher level decoders as needed 
*/

struct VBIdecoder {
  unsigned char *vbibuf;
  unsigned char *lbuf;
  int flags;
#define VBI_VT  1
#define VBI_VPS 2
#define VBI_VC  4
  int lpf;   // lines per field
  int field;  
  int line;
  int bpl;   // bytes per line
  int bpf;   // bytes per frame 
  unsigned char vcbuf[20];
  unsigned char vc2buf[20];
  unsigned char off, thresh;
  uint spos;
  vpsinfo vpsi;
  ICdeco icd;
  VTchannel vtch;
  double freq;
  unsigned int vtstep, vcstep, vpsstep;
/* use fixpoint arithmetic for scanning steps */
#define FPSHIFT 16
#define FPFAC (1<<FPSHIFT)
  int norm;

  void decode_line(unsigned char *data, int line);

  void decode(unsigned char *data) {
    vbibuf=data;
    for (line=0; line<lpf*2; line++) {
      lbuf=line*bpl+vbibuf;
      decode_line(lbuf, line);
    }
  }

  void setfreq(double f, int n=0) {
    double vtfreq=norm ? 5.72725 : 6.9375;
    double vpsfreq=5.0; // does NTSC have VPS???
    double vcfreq=0.77;
    
    norm=n;
    /* if no frequency given, use standard ones for Bt848 and PAL/NTSC */
    if (f==0.0) 
      freq=norm ? 28.636363 : 35.468950;
    else
      freq=f;

    vtstep=int((freq/vtfreq)*FPFAC+0.5);
    /* VPS is shift encoded, so just sample first "state" */
    vpsstep=2*int((freq/vpsfreq)*FPFAC+0.5); 
    vcstep=int((freq/vcfreq)*FPFAC+0.5);
  }

  void init(int lines, int bytes, int flag, int n=0, double f=0.0) {
    flags=flag;
    lpf=lines;
    bpl=bytes;
    freq=f;
    bpf=lpf*bpl*2;
    setfreq(f,n);
  }

  VBIdecoder(int lines, int bytes, int flag=VBI_VT, int n=0, double f=0.0) {
    init(lines,bytes,flag,n,f);
  }; 

  ~VBIdecoder() {
    delete [] vbibuf;
  }

  /* primitive automatic gain control to determine the right slicing offset
     XXX: it should suffice to do this once per channel change
     XXX: handle channel changes :-)
   */
  void AGC(int start, int stop, int step) {
    int i, min=255, max=0;
    
    for (i=start; i<stop; i+=step) {
      if (lbuf[i]<min) 
	min=lbuf[i];
      if (lbuf[i]>max) 
	max=lbuf[i];
    }
    thresh=(max+min)/2;
    off=128-thresh;
  }

  inline unsigned char scan(unsigned int step) { 
    int j;
    unsigned char dat;
    
    for (j=7, dat=0; j>=0; j--, spos+=step)
      dat|=((lbuf[spos>>FPSHIFT]+off)&0x80)>>j;
    return dat;
  }

  inline unsigned char vtscan(void) {
    return scan(vtstep);
  }

  inline unsigned char vpsscan(void) {
    return scan(vpsstep);
  }

  unsigned char vcscan(void) {
    int j;
    unsigned char dat;

    /* note the different bit order! */
    for (dat=0,j=7; j>=0; j--, spos+=vcstep) 
      dat|=((lbuf[spos>>FPSHIFT]+off)&0x80)>>j;
    return dat;
  }
};


void VBIdecoder::decode_line(unsigned char *d, int line) 
{
  unsigned char data[45];
  int i,j,p;
  unsigned char c;
  int aline=line % lpf;
  lbuf=d;
  
  AGC(120,450,1);
  
  /* all kinds of data with videotext data format: videotext, intercast, ... */
  if (flags&VBI_VT) {
    // search for first 1 bit (VT always starts with 55 55 27 !!!)
    p=50;
    while ((lbuf[p]<thresh)&&(p<350))
      p++;
    spos=(p<<FPSHIFT)+vtstep/2;
    
    /* ignore first bit for now */
    data[0]=vtscan();
    //cout << HEX(2) << (int)data[0] << endl;
    if ((data[0]&0xfe)==0x54) {
      data[1]=vtscan();
      switch (data[1]) {
      case 0xd5: /* oops, missed first 1-bit: backup 2 bits */
	spos-=2*vtstep; 
	data[1]=0x55;
      case 0x55:
	data[2]=vtscan();
	switch(data[2]) {
	case 0xd8: /* this shows up on some channels?!?!? */
	  for (i=3; i<45; i++) 
	    data[i]=vtscan();
	  return;
	case 0x27:
	  for (i=3; i<45; i++) 
	    data[i]=vtscan();
	  decode_vt(data);
	  return;
	default:
	  break;
	}
      default:
	break;
      }
    }
  }
  
  /* VPS information with channel name, time, VCR programming info, etc. */
  if (flags&VBI_VPS && line==9) {
    uint p;
    int i;
    unsigned char off=128-thresh;
    p=150;
    while ((lbuf[p]<thresh)&&(p<260))
      p++;
    p+=2;
    spos=p<<FPSHIFT;
    if ((data[0]=vpsscan())!=0xff)
      return;
    if ((data[1]=vpsscan())!=0x5d)
      return;
    for (i=2; i<16; i++) 
      data[i]=vpsscan();
    vpsi.decode(data);
  }
  
  /* Videocrypt 1/2 data which includes data like on screen messages (usually
     the channel name), the field number (so that the decoder decrypts the
     right fields) and the datagram which is sent to the smartcard 
     */
  if (flags&VBI_VC) {
    /* Videocrypt stuff removed due to uncertain legal situation */
  }

}


#define VBI_LINENUM 16
#define VBI_BPF VBI_LINENUM*2*2048

main(int argc,char **argv)
{

  char *c;
  int i, norm=0, flags=VBI_VT;
  unsigned char data[VBI_LINENUM*2*2048];
  
  char *devname=0;

  while (--argc) {
    c=(*++argv);
    switch (c[0]) {
    case '-' :
      switch (c[1]) {
      case '?':
	//usage();
	exit(0);
      case 'v':
	switch(c[2]) {
	case 'c':
	  flags|=VBI_VC;
	  break;
	case 'p':
	  flags|=VBI_VPS;
	  break;
	case '\0':
	  verbosity++;
	  break;
	case '1' ... '9':
	  if (c[2])
	    verbosity=atoi(c+2);
	  break;
	default:
	  cerr << "-v" << c[2] << " ???" << "\n"; 
	  break;
	}
	break;
      case 'n':
	norm=1;
	break;
      default:
	cerr << "vbidecode : Unknown switch: " << c << "\n"; 
	break;
      }
      break;
    default:
      devname=c; break;
    }
  }

  if (!devname)
    devname="/dev/vbi0";
  ifstream fin(devname);
  
  if (!fin) {
    cerr << "Could not open VBI device\n";
    exit(1);
  }

  cout << "vbidecode 1.0.0 (C) 1997,98 Ralph Metzler (rjkm@thp.uni-koeln.de)\n";
  
  getcnames();
  VBIdecoder vbid(VBI_LINENUM,2048,flags,norm);

  /* give vbidecode higher priority,
     this seems to decrease the number of decoding errors when other programs
     are running at the same time
     (it might cause trouble on very slow computers!)
     */
  sched_param p;
  p.sched_priority=1;
  sched_setscheduler(0,SCHED_FIFO,&p);

  while (fin) {
    fin.read(data, VBI_BPF);
    vbid.decode(data);
  }
}
