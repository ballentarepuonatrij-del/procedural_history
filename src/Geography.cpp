#include "Fantasy/World.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace fantasy {
namespace {
struct Noise {
    int width{}, height{};
    std::vector<double> values;
    Noise(int w, int h, std::mt19937& random) : width(w), height(h), values(static_cast<size_t>(w*h)) {
        std::uniform_real_distribution<double> distribution(0,1);
        for (auto& v: values) v=distribution(random);
    }
    double sample(double x, double y) const {
        x=std::clamp(x,0.,double(width-1)); y=std::clamp(y,0.,double(height-1));
        int ix=int(x), iy=int(y), jx=std::min(width-1,ix+1), jy=std::min(height-1,iy+1);
        double fx=x-ix, fy=y-iy;
        fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy);
        auto at=[&](int xx,int yy) { return values[static_cast<size_t>(yy*width+xx)]; };
        return (1-fy)*((1-fx)*at(ix,iy)+fx*at(jx,iy)) + fy*((1-fx)*at(ix,jy)+fx*at(jx,jy));
    }
};
}
void World::createTerrain() {
    const int w=settings_.width, h=settings_.height;
    Noise broad(6,5,rng_), middle(15,10,rng_), fine(32,22,rng_), moisture(11,8,rng_);
    tiles_.resize(static_cast<size_t>(w*h));
    owners_.assign(static_cast<size_t>(w*h),-1);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        double nx=(x+.5)/double(w), ny=(y+.5)/double(h);
        double ellipse=std::sqrt(std::pow((nx-.51)/.78,2)+std::pow((ny-.49)/.79,2));
        double broadV=broad.sample(nx*5,ny*4), midV=middle.sample(nx*14,ny*9), fineV=fine.sample(nx*31,ny*21);
        double elevation=.53 + .34*(broadV-.5)+.27*(midV-.5)+.16*(fineV-.5) - .23*ellipse;
        double wet=.64*moisture.sample(nx*10,ny*7)+.36*fineV;
        Terrain terrain;
        if(elevation<.38) terrain=Terrain::Sea;
        else if(elevation>.63 || (elevation>.56 && broadV>.78)) terrain=Terrain::Mountain;
        else if(wet>.61 && ny>.57) terrain=Terrain::Jungle;
        else if(wet>.47) terrain=Terrain::Forest;
        else terrain=Terrain::Plain;
        tiles_[static_cast<size_t>(index(x,y))]={terrain,static_cast<uint8_t>(std::clamp(int(elevation*255),0,255))};
    }
}
bool World::isCoastal(int x, int y) const {
    if(!inside(x,y) || tiles_[static_cast<size_t>(index(x,y))].terrain==Terrain::Sea) return false;
    constexpr int dx[]{1,-1,0,0},dy[]{0,0,1,-1};
    for(int d=0;d<4;++d) {
        int xx=x+dx[d],yy=y+dy[d];
        if(inside(xx,yy) && tiles_[static_cast<size_t>(index(xx,yy))].terrain==Terrain::Sea) return true;
    }
    return false;
}
Point World::chooseCapital(Species species) {
    double best=-1e10; Point choice{settings_.width/2,settings_.height/2};
    for(int y=2;y<settings_.height-2;++y) for(int x=2;x<settings_.width-2;++x) {
        int idx=index(x,y);
        if(tiles_[static_cast<size_t>(idx)].terrain==Terrain::Sea || owners_[static_cast<size_t>(idx)]!=-1) continue;
        auto terrain=tiles_[static_cast<size_t>(idx)].terrain;
        double score=std::uniform_real_distribution<double>(0,2)(rng_);
        switch(species) {
            case Species::Human: score+=terrain==Terrain::Plain?12:terrain==Terrain::Forest?3:0; if(isCoastal(x,y)) score+=5; break;
            case Species::Elf: score+=terrain==Terrain::Forest?15:terrain==Terrain::Jungle?6:0; break;
            case Species::Dwarf: score+=terrain==Terrain::Mountain?18:0; break;
            case Species::Outlaw: score+=isCoastal(x,y)?17:terrain==Terrain::Forest?7:0; break;
            case Species::Folk: score+=(terrain==Terrain::Forest||terrain==Terrain::Jungle)?17:0; break;
        }
        for(const Nation& n: nations_) {
            double dist=std::hypot(double(x-n.capital.x),double(y-n.capital.y));
            score+=std::min(16.,dist*.72);
            if(dist<8) score-=35;
        }
        score+=std::min({x, settings_.width-1-x, y, settings_.height-1-y})*.20;
        if(score>best) { best=score; choice={x,y}; }
    }
    return choice;
}
} // namespace fantasy
