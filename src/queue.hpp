#pragma once
#include <array>
#include <atomic>
#include <cstdint>
namespace vamp {
template<typename T,size_t N> class Spsc {
    std::array<T,N> data{};std::atomic<size_t> read{0},write{0};
public:
    bool push(const T&v){size_t w=write.load(std::memory_order_relaxed),n=(w+1)%N;if(n==read.load(std::memory_order_acquire))return false;data[w]=v;write.store(n,std::memory_order_release);return true;}
    bool pop(T&v){size_t r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return false;v=data[r];read.store((r+1)%N,std::memory_order_release);return true;}
    void reset(){read=0;write=0;}
    size_t size()const{size_t w=write.load(std::memory_order_acquire),r=read.load(std::memory_order_acquire);return (w+N-r)%N;}
};
// Bounded multiple-producer / single-consumer queue; the audio thread never waits.
template<typename T,size_t N> class Mpsc {
    struct Cell{std::atomic<size_t>sequence{0};T value{};};
    std::array<Cell,N>data;std::atomic<size_t>write{0};size_t read=0;
public:
    Mpsc(){for(size_t i=0;i<N;i++)data[i].sequence=i;}
    bool push(const T&value){size_t position=write.load(std::memory_order_relaxed);Cell*cell;for(;;){cell=&data[position%N];size_t sequence=cell->sequence.load(std::memory_order_acquire);auto difference=(intptr_t)(sequence-position);if(difference==0){if(write.compare_exchange_weak(position,position+1,std::memory_order_relaxed))break;}else if(difference<0)return false;else position=write.load(std::memory_order_relaxed);}cell->value=value;cell->sequence.store(position+1,std::memory_order_release);return true;}
    bool pop(T&value){auto&cell=data[read%N];if(cell.sequence.load(std::memory_order_acquire)!=read+1)return false;value=cell.value;cell.sequence.store(read+N,std::memory_order_release);read++;return true;}
};
}
