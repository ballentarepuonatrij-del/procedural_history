#include "Fantasy/World.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <vector>
#include <limits>

namespace fantasy {
namespace {
const wchar_t* wordsA[]={L"Ар",L"Те",L"Кал",L"Лир",L"Мор",L"Сан",L"Вир",L"Эль",L"Дар",L"Ор",L"Рун",L"Бел",L"Нар",L"Тал",L"Кир",L"Зар"};
const wchar_t* wordsB[]={L"ланд",L"ория",L"мир",L"валь",L"гард",L"тейна",L"рид",L"ания",L"елия",L"дор",L"нель",L"сара"};
std::wstring geoName(const wchar_t* prefix,int id,int salt) {
    return std::wstring(prefix)+wordsA[(id*7+salt)%16]+wordsB[(id*5+salt/3)%12]+L"-"+std::to_wstring(id+1);
}
}
// The fixed geography consists of continents -> regions -> provinces -> tiles.
// Each province is a connected component of land cells (including the dateline),
// rather than a moving country boundary. All sovereignty changes happen by province.
void World::createRegions() {
    const int w=settings_.width,h=settings_.height,n=w*h;
    tileProvinces_.assign(static_cast<size_t>(n),-1);
    std::vector<int> landmass(static_cast<size_t>(n),-1);
    constexpr int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
    std::vector<std::vector<int>> masses;
    for(int start=0;start<n;++start) {
        if(tiles_[static_cast<size_t>(start)].terrain==Terrain::Sea || landmass[static_cast<size_t>(start)]>=0)continue;
        int id=static_cast<int>(masses.size());
        masses.emplace_back();
        std::queue<int> queue;queue.push(start);landmass[static_cast<size_t>(start)]=id;
        while(!queue.empty()) {
            int idx=queue.front();queue.pop();masses.back().push_back(idx);
            int x=idx%w,y=idx/w;
            for(int d=0;d<4;++d) {
                int ny=y+dy[d];if(ny<0||ny>=h)continue;
                int ni=index(wrapX(x+dx[d]),ny);
                if(tiles_[static_cast<size_t>(ni)].terrain==Terrain::Sea||landmass[static_cast<size_t>(ni)]>=0)continue;
                landmass[static_cast<size_t>(ni)]=id;queue.push(ni);
            }
        }
    }
    continents_.reserve(masses.size());
    for(size_t m=0;m<masses.size();++m) {
        Continent c;c.id=static_cast<int>(m);c.landTiles=static_cast<int>(masses[m].size());c.name=geoName(c.landTiles>=110?L"Материк ":L"Остров ",c.id,5);
        c.center={masses[m][masses[m].size()/2]%w,masses[m][masses[m].size()/2]/w};
        continents_.push_back(c);
        const auto& cells=masses[m];
        // One province per ~26 tiles. Every island receives at least one.
        const int count=std::clamp(static_cast<int>((cells.size()+13)/26),1,120);
        std::vector<int> seeds;
        seeds.reserve(static_cast<size_t>(count));
        seeds.push_back(cells[static_cast<size_t>(randomInt(0,static_cast<int>(cells.size())-1))]);
        auto distance=[&](int a,int b) {
            const int ax=a%w,bx=b%w;
            int dd=std::abs(ax-bx);dd=std::min(dd,w-dd);
            const double lat=std::abs(latitudeAt((a/w+b/w+1)*.5));
            const double longitudeScale=std::max(.24,std::cos(lat*3.141592653589793/180.));
            const double xx=dd*longitudeScale,yy=double(a/w-b/w);
            return xx*xx+yy*yy;
        };
        for(int seed=1;seed<count;++seed) {
            double farthest=-1.;int chosen=-1;
            for(int idx:cells) {
                double near=std::numeric_limits<double>::max();
                for(int s:seeds)near=std::min(near,distance(idx,s));
                if(near>farthest) {farthest=near;chosen=idx;}
            }
            if(chosen<0||farthest<=0)break;
            seeds.push_back(chosen);
        }
        std::queue<int> queue;
        for(int seed:seeds) {
            Province p;p.id=static_cast<int>(provinces_.size());p.continent=static_cast<int>(m);
            p.name=geoName(L"Провинция ",p.id,23);p.center={seed%w,seed/w};
            tileProvinces_[static_cast<size_t>(seed)]=static_cast<int16_t>(p.id);
            provinces_.push_back(std::move(p));queue.push(seed);
        }
        // Simultaneous flood fill guarantees a connected province with no hidden gaps.
        while(!queue.empty()) {
            int idx=queue.front();queue.pop();
            int pid=tileProvinces_[static_cast<size_t>(idx)];
            Province& p=provinces_[static_cast<size_t>(pid)];p.cells.push_back(idx);
            int x=idx%w,y=idx/w;
            for(int d=0;d<4;++d) {
                int ny=y+dy[d];if(ny<0||ny>=h)continue;
                int ni=index(wrapX(x+dx[d]),ny);
                if(landmass[static_cast<size_t>(ni)]!=static_cast<int>(m)||tileProvinces_[static_cast<size_t>(ni)]>=0)continue;
                tileProvinces_[static_cast<size_t>(ni)]=static_cast<int16_t>(pid);queue.push(ni);
            }
        }
    }
    // Compute stable province centroids (actual member tiles) and dominant biomes.
    for(Province& p:provinces_) {
        p.landTiles=static_cast<int>(p.cells.size());
        std::array<int,8> counts{};
        const int first=p.cells.front();
        double best=-std::numeric_limits<double>::infinity();
        for(int cell:p.cells) counts[static_cast<size_t>(tiles_[static_cast<size_t>(cell)].terrain)]++;
        p.terrain=static_cast<Terrain>(std::max_element(counts.begin(),counts.end())-counts.begin());
        // Pick a tile most central to the other cells (small provinces -> affordable).
        for(int candidate:p.cells) {
            double score=0.;
            for(int cell:p.cells) {
                int xx=std::abs(candidate%w-cell%w);xx=std::min(xx,w-xx);
                int yy=candidate/w-cell/w;
                score-=xx*xx+yy*yy;
            }
            if(candidate==first||score>best) {best=score;p.center={candidate%w,candidate/w};}
        }
    }
    std::vector<std::vector<int>> neighbors(provinces_.size());
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        int a=tileProvinces_[static_cast<size_t>(index(x,y))];if(a<0)continue;
        for(int d: {0,2}) { // right + down; undirected province adjacency
            int ny=y+dy[d];if(ny>=h)continue;
            int b=tileProvinces_[static_cast<size_t>(index(wrapX(x+dx[d]),ny))];
            if(b>=0&&a!=b) {neighbors[static_cast<size_t>(a)].push_back(b);neighbors[static_cast<size_t>(b)].push_back(a);}
        }
    }
    for(auto& adj:neighbors){std::sort(adj.begin(),adj.end());adj.erase(std::unique(adj.begin(),adj.end()),adj.end());}
    // Contiguous areas/regions formed from adjacent provinces on the same continent.
    for(Continent& c:continents_) {
        std::vector<int> remaining;
        for(const Province& p:provinces_)if(p.continent==c.id)remaining.push_back(p.id);
        int perRegion=std::max(1,static_cast<int>(remaining.size()+4)/5);
        // Pick widely separated anchors across the province graph, then flood-fill.
        std::vector<int> seeds;
        seeds.push_back(remaining.front());
        for(int k=1;k<perRegion;++k) {
            double best=-1.;int chosen=-1;
            for(int id:remaining) {
                const auto& pos=provinces_[static_cast<size_t>(id)].center;
                double dist=std::numeric_limits<double>::max();
                for(int s:seeds) {
                    const auto& other=provinces_[static_cast<size_t>(s)].center;
                    int xx=std::abs(pos.x-other.x);xx=std::min(xx,w-xx);
                    int yy=pos.y-other.y;
                    dist=std::min(dist,double(xx*xx+yy*yy));
                }
                if(dist>best){best=dist;chosen=id;}
            }
            if(chosen<0||best<=0)break;
            seeds.push_back(chosen);
        }
        std::queue<int> queue;
        for(int seed:seeds) {
            Region r;r.id=static_cast<int>(regions_.size());r.continent=c.id;
            r.name=geoName(L"Регион ",r.id,37);r.center=provinces_[static_cast<size_t>(seed)].center;
            regions_.push_back(r);provinces_[static_cast<size_t>(seed)].region=r.id;queue.push(seed);
            ++c.regionCount;
        }
        while(!queue.empty()) {
            int id=queue.front();queue.pop();
            int rid=provinces_[static_cast<size_t>(id)].region;
            Region& r=regions_[static_cast<size_t>(rid)];
            r.provinces.push_back(id);r.landTiles+=provinces_[static_cast<size_t>(id)].landTiles;
            for(int other:neighbors[static_cast<size_t>(id)]) {
                Province& next=provinces_[static_cast<size_t>(other)];
                if(next.region!=-1)continue;
                next.region=rid;queue.push(other);
            }
        }
    }
    provinceState_.assign(provinces_.size(),{});
}
void World::setProvinceOwner(int province,int nation) {
    if(province<0||static_cast<size_t>(province)>=provinces_.size())return;
    for(int cell:provinces_[static_cast<size_t>(province)].cells)
        owners_[static_cast<size_t>(cell)]=static_cast<int16_t>(nation);
}
int World::provinceOwnerAt(const Snapshot& snapshot,int provinceId) const {
    if(provinceId<0||static_cast<size_t>(provinceId)>=snapshot.provinceStates.size())return -1;
    return snapshot.provinceStates[static_cast<size_t>(provinceId)].owner;
}
} // namespace fantasy
