#include "Fantasy/World.h"
#include "Fantasy/MapCamera.h"
#include "../examples/MagicPortentsModule.h"
#include <cassert>
#include <iostream>
#include <string>
int main() {
    fantasy::World w;
    fantasy::Settings settings{10,80,123456,72,44};
    w.generate(settings);
    assert(w.at(0).owner.size()==72u*44u);
    assert(w.at(80).year==80);
    assert(w.nations().size()>=10);
    assert(!w.people().empty());
    assert(!w.guilds().empty());
    assert(!w.interactions().empty());
    assert(!w.settlements().empty());
    assert(!w.settlementEvents().empty());
    for(int year=0;year<=settings.years;++year) {
        const auto& snap=w.at(year);
        size_t founded=0;
        for(const auto& town:w.settlements()) {
            if(town.founded>year) continue;
            ++founded;
            assert(static_cast<size_t>(town.id)<snap.settlementStates.size());
            const auto& state=snap.settlementStates[static_cast<size_t>(town.id)];
            assert(state.owner==snap.owner[static_cast<size_t>(w.index(town.position.x,town.position.y))]);
            assert(state.prosperity>=0 && state.prosperity<=100);
            assert(state.knownResidents>=0);
            assert(town.foundingNation>=0);
            assert(w.nations()[static_cast<size_t>(town.foundingNation)].founded<=town.founded);
        }
        assert(snap.settlementStates.size()==founded);
    }
    for(const auto& note:w.settlementEvents()) {
        assert(note.settlement>=0 && static_cast<size_t>(note.settlement)<w.settlements().size());
        assert(note.year>=w.settlements()[static_cast<size_t>(note.settlement)].founded);
        assert(note.year<=settings.years);
        assert(!note.text.empty());
    }
    fantasy::MapCamera camera;
    camera.reset(88,52);
    camera.setViewport({24,88,1024,688});
    assert(std::abs(camera.zoom()-1.)<1.e-10);
    auto center=camera.worldToScreen(44,26);
    auto restored=camera.screenToWorld(center.x,center.y);
    assert(std::abs(restored.x-44)<1.e-8 && std::abs(restored.y-26)<1.e-8);
    auto pivot=camera.screenToWorld(610,360);
    camera.zoomAt(610,360,2.);
    auto samePivot=camera.screenToWorld(610,360);
    assert(std::abs(pivot.x-samePivot.x)<1.e-8);
    assert(std::abs(pivot.y-samePivot.y)<1.e-8);
    auto beforePan=camera.screenToWorld(610,360);
    camera.panPixels(40,-20);
    auto afterPan=camera.screenToWorld(610,360);
    assert(afterPan.x<beforePan.x && afterPan.y>beforePan.y);
    camera.panPixels(-100000,-100000); // bounded by world edges
    auto rect=camera.drawnRect();
    assert(rect.right>=1024 && rect.bottom>=688);
    camera.zoomAt(610,360,.00001);
    assert(camera.zoom()==1.);
    auto fitted=camera.drawnRect();
    assert(fitted.width()<=1000 && fitted.height()<=600);
    bool hadPirates=false;
    for(const auto& n:w.nations()) {
        if(n.founded==0 && n.culture==fantasy::Species::Outlaw) {
            hadPirates=true;
            assert(w.at(0).nationStates[static_cast<size_t>(n.id)].ruler>=0);
        }
    }
    assert(hadPirates);
    int changed=0;
    for(size_t i=0;i<w.at(0).owner.size();++i) if(w.at(0).owner[i]!=w.at(80).owner[i]) ++changed;
    assert(changed>10);
    for(int year=0;year<=settings.years;++year) {
        const auto& snap=w.at(year);
        for(auto owner:snap.owner) assert(owner==-1 || (owner>=0 && static_cast<size_t>(owner)<w.nations().size() && w.nations()[static_cast<size_t>(owner)].founded<=year));
        for(const auto& p:w.people()) if(w.aliveAt(p,year)) assert(w.nationAt(p,year)>=0);
    }
    size_t peopleWhoDeveloped=0;
    for(const auto& p:w.people()) {
        assert(!p.development.empty());
        assert(p.development.front().year==std::max(0,p.born));
        assert(p.development.back().year==std::min(settings.years,p.died-1));
        assert(w.statsAt(p,p.development.front().year)==p.development.front().stats);
        assert(w.statsAt(p,settings.years)==p.development.back().stats);
        for(size_t j=1;j<p.development.size();++j) {
            assert(p.development[j].year==p.development[j-1].year+1);
            // Binary search at an earlier year must never leak later values.
            assert(w.statsAt(p,p.development[j-1].year)==p.development[j-1].stats);
        }
        for(const auto& entry:p.development) {
            for(auto x:entry.stats.abilities) assert(x<=100);
            for(auto x:entry.stats.skills) assert(x<=100);
            for(auto x:entry.stats.traits) assert(x<=100);
        }
        if(p.development.front().stats!=p.development.back().stats) ++peopleWhoDeveloped;
        assert(!p.biography.empty());
        assert(!p.stages.empty());
        assert(p.stages.front().year>=p.born);
        for(const auto& note:p.biography) assert(note.year>=p.born);
        for(size_t k=1;k<p.stages.size();++k) assert(p.stages[k].year>=p.stages[k-1].year);
    }
    assert(peopleWhoDeveloped>100);
    for(const auto& relation:w.interactions()) {
        assert(relation.first!=relation.second);
        assert(relation.year>=0 && relation.year<=settings.years);
        assert(static_cast<size_t>(relation.first)<w.people().size());
        assert(static_cast<size_t>(relation.second)<w.people().size());
    }
    fantasy::World again; again.generate(settings);
    assert(w.at(80).owner==again.at(80).owner);
    assert(w.people().size()==again.people().size());
    for(size_t i=0;i<w.people().size();++i) {
        const auto& a=w.people()[i],&b=again.people()[i];
        assert(a.innate==b.innate);
        assert(a.development.size()==b.development.size());
        for(size_t yearIndex=0;yearIndex<a.development.size();++yearIndex)
            assert(a.development[yearIndex].stats==b.development[yearIndex].stats);
    }
    assert(w.settlementEvents().size()==again.settlementEvents().size());
    assert(w.at(80).settlementStates.size()==again.at(80).settlementStates.size());
    for(size_t i=0;i<w.at(80).settlementStates.size();++i) {
        assert(w.at(80).settlementStates[i].owner==again.at(80).settlementStates[i].owner);
        assert(w.at(80).settlementStates[i].prosperity==again.at(80).settlementStates[i].prosperity);
    }
    fantasy::World withModule;
    withModule.registerModule(std::make_unique<MagicPortentsModule>());
    withModule.generate(settings);
    bool granted=withModule.grantExperience(0,settings.years,fantasy::LifeExperience::Mentorship);
    if(withModule.aliveAt(withModule.people()[0],settings.years)) assert(granted);
    else assert(!granted);
    assert(!withModule.grantExperience(-1,settings.years,fantasy::LifeExperience::Journey));
    bool foundMagic=false;
    for(const auto& event:withModule.events()) if(event.text.find(L"Небесное знамение")!=std::wstring::npos) foundMagic=true;
    assert(foundMagic);
    std::cout << "OK: " << w.nations().size() << " nations; " << w.people().size()
              << " named persons; " << w.events().size() << " historical events; "
              << changed << " changed cells; "
              << w.settlements().size() << " settlements; "
              << w.settlementEvents().size() << " local events.\n";
}
