            if(kind==32)d.extra[0]=std::exp(-2*pi*v[0]/rate);
            if(kind==33)for(int n=0;n<6;n++)for(int c=0;c<2;c++)d.filters[n][c].set(6,v[0]*(n+1),v[2],0,rate);
            if(kind==34)for(int c=0;c<2;c++){d.filters[0][c].set(5,v[0],.707,-v[1]*.5,rate);d.filters[1][c].set(4,v[0],.707,v[1]*.5,rate);}
            if(kind==35)for(int c=0;c<2;c++)d.filters[0][c].set(2,v[0],.707,0,rate);
            if(kind==35)d.extra[0]=amp(v[1]);
            if(kind==36){for(int c=0;c<2;c++){d.filters[0][c].set(1,v[0],.707,0,rate);d.filters[1][c].set(1,v[2],.707,0,rate);}d.extra[0]=amp(v[1]);}
            if(kind==37||kind==38)for(int c=0;c<2;c++)d.filters[0][c].set(kind==37?1:2,v[0],.707,0,rate);
            if(kind==40){d.extra[0]=v[0]*.001*rate;d.extra[1]=v[1]*.001*rate;}
            if(kind==41){d.extra[0]=1-std::exp(-2*pi*std::min(v[2],rate*.46)/rate);d.extra[1]=clamp(v[3]>.5?v[4]*60/std::max(1.,tempo):v[0]*.001,.001,4)*rate;}
            if(kind==42)for(int n=0;n<3;n++)for(int c=0;c<2;c++)d.filters[n][c].set(3,v[0]*std::pow(v[2],n),v[1],0,rate);
            if(kind==43)d.extra[0]=2*pi*v[0]/rate;
            if(kind==44)for(int c=0;c<2;c++)d.filters[0][c].set(1,v[1],.707,0,rate);
