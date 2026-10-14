// Original CPU-only silence fixtures, derived from the pinned decoder's grammar.
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int logged_errors;
static void log_callback(void *ptr,int level,const char *fmt,va_list args) {
  (void)ptr;
  if(level <= AV_LOG_ERROR) { ++logged_errors; vfprintf(stderr,fmt,args); }
}
static void put(unsigned char *p,int *pos,unsigned value,int width) {
  for(int i=width-1;i>=0;--i) { p[*pos/8] |= ((value>>i)&1) << (7-*pos%8); ++*pos; }
}
static int fixture(unsigned char *packet,int ch) {
  int bits=8; memset(packet,0,16);
  put(packet,&bits,0,15); // Length patched after constructing the frame.
  put(packet,&bits,1,1); // Fixed channel layout.
  put(packet,&bits,0,1); // One full 512-sample subframe.
  if(ch==2) put(packet,&bits,0,1); // No postprocessing transform.
  put(packet,&bits,0,8); // DRC field.
  put(packet,&bits,0,1); // No start/end sample skipping.
  put(packet,&bits,0,1); // No subframe extended header.
  put(packet,&bits,0,1); // Reserved bit clear.
  if(ch==2) {
    put(packet,&bits,0,1); // Supported channel-transform syntax.
    put(packet,&bits,1,1); put(packet,&bits,0,1); // No channel transform.
  }
  for(int c=0;c<ch;++c) put(packet,&bits,0,1); // No coefficients => silence.
  put(packet,&bits,0,1); // Length-delimited fill bit.
  put(packet,&bits,0,1); // No following frame in this packet.
  int length=bits-8, cursor=8; put(packet,&cursor,length,15);
  int size=(bits+7)/8; packet[0]=(size*8-bits)<<2;
  return size;
}
static AVCodecContext *open_decoder(int ch,int rate) {
  const AVCodec *codec=avcodec_find_decoder(AV_CODEC_ID_XMAFRAMES);
  AVCodecContext *ctx=avcodec_alloc_context3(codec);
  if(!ctx) return NULL;
  ctx->channels=ch; ctx->sample_rate=rate; ctx->flags2|=AV_CODEC_FLAG2_SKIP_MANUAL;
  if(avcodec_open2(ctx,codec,NULL)<0) { avcodec_free_context(&ctx); return NULL; }
  return ctx;
}
int main(void) {
  const int rates[]={24000,32000,44100,48000};
  av_log_set_callback(log_callback);
  for(int ch=1;ch<=2;++ch) for(int r=0;r<4;++r) {
    AVCodecContext *ctx=open_decoder(ch,rates[r]); if(!ctx) return 1;
    AVPacket *pkt=av_packet_alloc(); AVFrame *frame=av_frame_alloc();
    unsigned char data[16]; int size=fixture(data,ch);
    if(!pkt || !frame || av_new_packet(pkt,size)<0) return 2;
    memcpy(pkt->data,data,size);
    for(int repeat=0;repeat<4;++repeat) {
      if(avcodec_send_packet(ctx,pkt)<0 || avcodec_receive_frame(ctx,frame)<0) return 3;
      if(frame->nb_samples!=512 || frame->channels!=ch || frame->sample_rate!=rates[r] || frame->format!=AV_SAMPLE_FMT_FLTP) return 4;
      for(int c=0;c<ch;++c) for(int i=0;i<512;++i) {
        float sample=((float*)frame->extended_data[c])[i];
        if(!isfinite(sample) || sample!=0.0f) return 5;
      }
      av_frame_unref(frame);
      if(avcodec_receive_frame(ctx,frame)!=AVERROR(EAGAIN)) return 6;
    }
    if(logged_errors) return 7;
    if(avcodec_send_packet(ctx,NULL)<0 || avcodec_receive_frame(ctx,frame)!=AVERROR_EOF) return 8;
    printf("PASS valid silence ch=%d rate=%d frames=4 samples_per_channel=512 packet=",ch,rates[r]);
    for(int i=0;i<size;++i) printf("%02x",data[i]); puts("");
    av_frame_free(&frame);av_packet_free(&pkt);avcodec_free_context(&ctx);
  }
  AVCodecContext *ctx=open_decoder(1,48000); AVPacket *pkt=av_packet_alloc();
  unsigned char data[16]; int size=fixture(data,1);
  if(!ctx || !pkt || av_new_packet(pkt,size)<0) return 9;
  memcpy(pkt->data,data,size); pkt->data[2]^=2; // Mutate frame length by one bit.
  int ret=avcodec_send_packet(ctx,pkt);
  if(ret!=AVERROR_INVALIDDATA || logged_errors!=1) return 10;
  puts("PASS malformed length rejected with AVERROR_INVALIDDATA");
  av_packet_free(&pkt);avcodec_free_context(&ctx);
  return 0;
}
