#include "Fantasy/World.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace fantasy {
namespace {
struct Noise {
    int width{},height{};
    std::vector<double> values;
    Noise(int w,int h,std::mt19937& random):width(w),height(h),values(static_cast<size_t>(w*h)) {
        std::uniform_real_distribution<double> pick(0,1);
        for(auto& v:values) v=pick(random);
    }
    // Longitude is periodic; no artificial cliff or ocean edge at +/-180 degrees.
    double sample(double x,double y) const {
        x=std::fmod(x,double(width));if(x<0)x+=width;
        y=std::clamp(y,0.,double(height-1));
        int ix=int(x),iy=int(y),jx=(ix+1)%width,jy=std::min(height-1,iy+1);
        double fx=x-ix,fy=y-iy;
        fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
        auto at=[&](int xx,int yy){return values[static_cast<size_t>(yy*width+xx)];};
        return (1-fy)*((1-fx)*at(ix,iy)+fx*at(jx,iy)) + fy*((1-fx)*at(ix,jy)+fx*at(jx,jy));
    }
};
}
void World::createTerrain() {
    const int w=settings_.width,h=settings_.height;
    Noise tectonic(8,6,rng_),broad(15,10,rng_),fine(39,25,rng_),moisture(17,11,rng_);
    tiles_.resize(static_cast<size_t>(w*h));
    owners_.assign(static_cast<size_t>(w*h),-1);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const double nx=(x+.5)/w,ny=(y+.5)/h;
        const double latitude=latitudeAt(y+.5);
        const double polar=std::abs(latitude)/90.;
        const double large=tectonic.sample(nx*8,ny*5);
        const double medium=broad.sample(nx*15,ny*9);
        const double detail=fine.sample(nx*39,ny*24);
        // Whole-world pseudo-tectonic elevation, not a continent in a rectangular mask.
        const double elevation=.505+.66*(large-.5)+.21*(medium-.5)+.095*(detail-.5)-.10*std::pow(polar,5);
        const double wet=.67*moisture.sample(nx*17,ny*10)+.33*detail;
        Terrain terrain=Terrain::Sea;
        if(elevation>=.48) {
            if(std::abs(latitude)>75) terrain=Terrain::Ice;
            else if(std::abs(latitude)>59) terrain=Terrain::Tundra;
            else if(elevation>.69 || (elevation>.61 && large>.66)) terrain=Terrain::Mountain;
            else if(std::abs(latitude)<24 && wet>.51) terrain=Terrain::Jungle;
            else if(std::abs(latitude)<43 && wet<.38) terrain=Terrain::Desert;
            else if(wet>.48) terrain=Terrain::Forest;
            else terrain=Terrain::Plain;
        }
        tiles_[static_cast<size_t>(index(x,y))]={terrain,static_cast<uint8_t>(std::clamp(int(elevation*255),0,255))};
    }
}
bool World::isCoastal(int x,int y) const {
    if(!inside(x,y) || tiles_[static_cast<size_t>(index(x,y))].terrain==Terrain::Sea) return false;
    constexpr int dx[]{1,-1,0,0},dy[]{0,0,1,-1};
    for(int d=0;d<4;++d) {
        int xx=wrapX(x+dx[d]),yy=y+dy[d];
        if(yy>=0 && yy<settings_.height && tiles_[static_cast<size_t>(index(xx,yy))].terrain==Terrain::Sea) return true;
    }
    return false;
}
Point World::chooseCapital(Species species) {
    double best=-1e10;Point choice{settings_.width/2,settings_.height/2};
    for(int y=2;y<settings_.height-2;++y) for(int x=0;x<settings_.width;++x) {
        int idx=index(x,y);
        if(tiles_[static_cast<size_t>(idx)].terrain==Terrain::Sea || owners_[static_cast<size_t>(idx)]!=-1) continue;
        auto terrain=tiles_[static_cast<size_t>(idx)].terrain;
        if(terrain==Terrain::Ice || terrain==Terrain::Tundra) continue;
        double score=std::uniform_real_distribution<double>(0,2)(rng_);
        switch(species) {
            case Species::Human:score+=terrain==Terrain::Plain?12:terrain==Terrain::Forest?3:0;if(isCoastal(x,y))score+=5;break;
            case Species::Elf:score+=terrain==Terrain::Forest?15:terrain==Terrain::Jungle?6:0;break;
            case Species::Dwarf:score+=terrain==Terrain::Mountain?18:0;break;
            case Species::Outlaw:score+=isCoastal(x,y)?17:terrain==Terrain::Forest?7:0;break;
            case Species::Folk:score+=(terrain==Terrain::Forest||terrain==Terrain::Jungle)?17:0;break;
        }
        for(const Nation& n:nations_) {
            double dx=std::min(std::abs(x-n.capital.x),settings_.width-std::abs(x-n.capital.x));
            double dist=std::hypot(dx,double(y-n.capital.y));
            score+=std::min(16.,dist*.72);
            if(dist<8)score-=35;
        }
        score+=std::min(y,settings_.height-1-y)*.12;
        if(score>best){best=score;choice={x,y};}
    }
    return choice;
}
} // namespace fantasy
