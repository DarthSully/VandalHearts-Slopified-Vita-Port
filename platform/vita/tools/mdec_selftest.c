/* Host self-test: the two-phase (serial parse, parallel assemble) MDEC decoder vs the original, on
 * synthetic v2 bitstreams. Build: copy the old/new pc_mdec.c with PC_MdecDecodeBS renamed to
 * dec_old/dec_new, compile each with -Iplatform/pc/src, link with this file and -lm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int dec_old(const unsigned char*,int,int,int,unsigned short*);
int dec_new(const unsigned char*,int,int,int,unsigned short*);
void PC_PoolParallelFor(int n, void (*fn)(int, void *), void *ctx){ for(int i=n-1;i>=0;i--) fn(i,ctx); }
void PC_GpuAsyncFinish(void){}
static unsigned char buf[1<<20]; static int nbits;
static unsigned short hw; static int hwbits; static int pos;
static void putbit(int b){ hw=(unsigned short)((hw<<1)|b); if(++hwbits==16){ buf[pos++]=hw&0xff; buf[pos++]=hw>>8; hwbits=0; hw=0; } }
static void put(int v,int n){ for(int i=n-1;i>=0;i--) putbit((v>>i)&1); }
static unsigned st=99; static unsigned R(void){ st=st*1103515245u+12345u; return st>>8; }
int main(void){
  long diff=0; int frames=40;
  for(int f=0; f<frames; f++){
    int w=320,h=(f%3==0)?176:240; int mbs=((w+15)/16)*((h+15)/16);
    memset(buf,0,sizeof buf); pos=8; hw=0; hwbits=0;
    buf[2]=0x00; buf[3]=0x38; buf[4]=1+R()%20; buf[5]=0; buf[6]=2; buf[7]=0;
    int stopAt = (f%4==0) ? (int)(R()%mbs) : mbs;          /* some frames end early */
    for(int m=0;m<stopAt;m++) for(int b=0;b<6;b++){
      int dc=(int)(R()%600)-300; put(dc&0x3ff,10);
      int nac=R()%12;
      for(int k=0;k<nac;k++){
        if(R()%3==0){ put(3,2); putbit(R()&1); }                 /* 11s: run0 lev1 */
        else { put(1,6); int run=R()%4, lev=(int)(R()%200)-100; put((run<<10)|(lev&0x3ff),16); } /* escape */
      }
      put(2,2);                                                   /* EOB */
    }
    put(0x1FF,10); for(int k=0;k<32;k++) putbit(0);               /* v2 end of frame */
    static unsigned short a[320*240], bb[320*240];
    memset(a,0x5a,sizeof a); memset(bb,0x5a,sizeof bb);
    int ra=dec_old(buf,pos+4,w,h,a), rb=dec_new(buf,pos+4,w,h,bb);
    if(ra!=rb){ printf("frame %d: return %d vs %d\n",f,ra,rb); diff++; }
    { int ch=0; for(int i=0;i<w*h;i++){ diff+=(a[i]!=bb[i]); ch+=(a[i]!=0x5a5a);} if(f<6) printf("frame %d ret=%d decoded pixels=%d\n",f,ra,ch); }
  }
  printf("MDEC: %d synthetic frames, differing pixels/returns old vs new: %ld\n",frames,diff);
  return diff!=0;
}
