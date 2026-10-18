/* Host self-test: banded/threaded and fast-path native raster vs inline, pixel for pixel.
 * gcc -O1 -std=gnu99 -DVH_RASTER_SELFTEST -DVH_REGION_US -Iplatform/pc/src -Iplatform/pc/include
 *     -I<build>/datagen/include_stage platform/pc/src/pc_raster.c platform/vita/tools/raster_selftest.c -lm -lpthread
 * (set g_selftestNoFast=pass instead of defer=pass to compare the fast path). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PsyQ/libgpu.h"
#include "pc_gpu_internal.h"
int g_selftestNoFast = 0; unsigned s_drawFrame = 0;
int HdRegionCount(void){return 0;} const char *HdDumpDir(void){return 0;} int HdReplaceCount(void){return 0;}
int HdAnyActive(void){return 0;} HdRegion *HdFindTriRegion(int a,int b,int c,int d,int e){return 0;}
void HdMaybeDump(int a,int b,int c,int d,int e,int f){} int PC_CpuCount(void){return 1;}
void PC_GpuPresent(unsigned short*v,int a,int b,int c,int d,int e,int f){} void PC_GpuAsyncFinish(void){}
const char *PC_LangBgDir(void){return 0;}
static unsigned rnd(void){ static unsigned s=12345; s=s*1103515245+12345; return s>>8; }
static unsigned short ref[512][1024];
int main(void){
  unsigned short (*v)[1024]=PC_GpuVramRaw();
  for(int y=0;y<512;y++)for(int x=0;x<1024;x++) ref[y][x]=v[y][x]=rnd()&0xFFFF;
  static unsigned short a[512][1024],b[512][1024];
  for(int pass=0;pass<2;pass++){
    g_selftestNoFast=0; int defer=pass; memcpy(v,ref,sizeof ref);
    unsigned st=777; 
    for(int i=0;i<4000;i++){
      #define R() (st=st*1664525u+1013904223u, st>>8)
      RenderCtx rc; memset(&rc,0,sizeof rc);
      rc.clipX=0; rc.clipY=0; rc.clipW=320; rc.clipH=240; rc.ofsX=(R()%3)*4; rc.ofsY=(R()%3)*4;
      rc.dither=R()&1; rc.twMaskX=(R()%4==0)?(R()&31):0; rc.twMaskY=(R()%4==0)?(R()&31):0; rc.twOffX=R()&31; rc.twOffY=R()&31;
      rc.target=0; rc.scale=1;
      RVert q[4]; for(int k=0;k<4;k++){ q[k].x=(double)((int)(R()%360)-20); q[k].y=(double)((int)(R()%280)-20); q[k].u=R()%256; q[k].v=R()%256; }
      int tex=R()&1, tp=(R()%3)<<7, tpage=tp|((5+R()%10)&0xF)|((R()&1)<<4)|((R()&3)<<5);
      int clut=((R()%64))|((256+R()%200)<<6);
      int col = (R()%3==0);
      int r=col?128:R()%256,g=col?128:R()%256,bb=col?128:R()%256;
      if (R()%5==0) { RVert a2=q[0]; q[1]=a2; q[1].x+=R()%60; q[2]=a2; q[2].y+=R()%60; q[3]=q[1]; q[3].y=q[2].y; q[1].u=q[0].u+20;q[2].v=q[0].v+20;q[3].u=q[1].u;q[3].v=q[2].v; }
      (defer?NativeAppendQuad:FillQuad)(&rc,q[0],q[1],q[2],q[3],r,g,bb,tex,tpage,clut,R()&1,(tpage>>5)&3);
      if(R()%7==0) (defer?NativeAppendRect:FillRect)(&rc,R()%300,R()%220,R()%60,R()%60,R()%256,R()%256,R()%256);
    }
    if(defer) NativeRasterizeFlush(0,0,320,240,0);
    memcpy(pass?b:a,v,sizeof a);
  }
  long diff=0; for(int y=0;y<512;y++)for(int x=0;x<1024;x++) diff+=(a[y][x]!=b[y][x]);
  long changed=0; for(int y=0;y<512;y++)for(int x=0;x<1024;x++) changed+=(a[y][x]!=ref[y][x]);
  printf("pixels changed: %ld, differing threaded-banded vs inline: %ld\n",changed,diff);
  return diff!=0;
}
