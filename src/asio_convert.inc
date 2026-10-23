void asioReadBlock(const void*buffer,float*out,int frames,int format){
    if(!buffer){std::fill_n(out,frames,0);return;}auto*bytes=(const uint8_t*)buffer;
    if(format==19){memcpy(out,buffer,frames*sizeof(float));for(int i=0;i<frames;i++)out[i]=std::isfinite(out[i])?(float)clamp(out[i],-8,8):0;return;}
    if(format==16){for(int i=0;i<frames;i++){int16_t v;memcpy(&v,bytes+i*2,2);out[i]=v/32768.f;}return;}
    if(format==18){for(int i=0;i<frames;i++){int32_t v;memcpy(&v,bytes+i*4,4);out[i]=v/2147483648.f;}return;}
    for(int i=0;i<frames;i++)out[i]=asioReadSample(buffer,i,format);
}
void asioWriteBlock(void*buffer,const float*input,int channel,int frames,int format){
    if(!buffer)return;auto*bytes=(uint8_t*)buffer;
    if(format==19){for(int i=0;i<frames;i++){float v=input[i*2+channel];v=std::isfinite(v)?(float)clamp(v,-1,1):0;memcpy(bytes+i*4,&v,4);}return;}
    if(format==16){for(int i=0;i<frames;i++){float v=input[i*2+channel];int16_t value=(int16_t)(std::clamp(std::isfinite(v)?double(v):0.,-1.,1.)*32767);memcpy(bytes+i*2,&value,2);}return;}
    if(format==18){for(int i=0;i<frames;i++){float v=input[i*2+channel];int32_t value=(int32_t)(std::clamp(std::isfinite(v)?double(v):0.,-1.,1.)*2147483647.);memcpy(bytes+i*4,&value,4);}return;}
    for(int i=0;i<frames;i++)asioWriteSample(buffer,i,format,input[i*2+channel]);
}
