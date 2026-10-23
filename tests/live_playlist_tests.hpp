static void livePlaylistTests(){
    Project p;p.bpm=120;p.bars=8;p.loop=false;p.tracks.resize(1);auto&track=p.tracks[0];track.sound=0;track.notes={{0,24,57,.8f}};track.clips={{0,32}};
    auto reference=compile(p),current=compile(p);auto a=std::make_unique<Renderer>(),b=std::make_unique<Renderer>();std::array<float,1024>x{},y{};
    for(int n=0;n<40;n++){a->render(*reference,x.data(),512,true);b->render(*current,y.data(),512,true);}
    auto expanded=p;expanded.tracks.push_back(Track{});auto sample=std::make_shared<Sample>();sample->rate=48000;sample->left.resize(48000);sample->right.resize(48000);expanded.tracks[1].sample=sample;expanded.tracks[1].clips={{0,2}};
    auto next=compile(expanded,{},current.get());double maximum=0;for(int n=0;n<100;n++){a->render(*reference,x.data(),512,true);b->render(*next,y.data(),512,true);for(int i=0;i<1024;i++)maximum=std::max(maximum,double(std::abs(x[i]-y[i])));}
    check(maximum<1e-7,"adding audio during playback preserves a sustained voice without a restart");
    expanded.tracks[0].clips.push_back({28,2});auto sameTrack=compile(expanded,{},next.get());for(int n=0;n<30;n++){a->render(*reference,x.data(),512,true);b->render(*sameTrack,y.data(),512,true);for(int i=0;i<1024;i++)maximum=std::max(maximum,double(std::abs(x[i]-y[i])));}
    check(maximum<1e-7,"appending a future clip on the playing track does not retrigger its current note");
    Project selection;selection.tracks.resize(256);for(auto&t:selection.tracks)t.clips={{1,1},{12,1}};
    auto forward=clipsInRectangle(selection,.5,2.5,.2,31.8),reverse=clipsInRectangle(selection,2.5,.5,31.8,.2);
    check(forward==reverse&&forward.size()==32&&forward.count({0,0})&&forward.count({31,0})&&!forward.count({0,1}),"playlist box selection keeps offscreen tracks and works in either direction");
    check(playlistTimeRange(.2,16,32)==std::make_pair(0.,16.),"ruler selection covers four complete bars without adding the next bar");
    check(playlistTimeRange(16,.2,32)==playlistTimeRange(.2,16,32),"ruler selection works backwards");
    check(playlistTimeRange(128,128,32)==std::make_pair(124.,128.),"ruler selection stays inside the song at its end");
    check(playlistTimeRange(-20,1000,32)==std::make_pair(0.,128.),"ruler selection clamps to song bounds");
    check(playlistTimeRange(1.13,1.8,32,.25)==std::make_pair(1.,2.),"Alt ruler selection supports the current fine grid");
    Json report={{"passed",true},{"sustained_audio_max_difference",maximum},{"new_track",true},{"same_track_future_clip",true}};std::ofstream(L"work/test-results/live-playlist.json")<<report.dump(2);
}
