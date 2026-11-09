#include "Fantasy/World.h"
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
    for(const auto& p:w.people()) {
        assert(!p.biography.empty());
        assert(!p.stages.empty());
        assert(p.stages.front().year>=p.born);
        for(const auto& note:p.biography) assert(note.year>=p.born);
        for(size_t k=1;k<p.stages.size();++k) assert(p.stages[k].year>=p.stages[k-1].year);
    }
    for(const auto& relation:w.interactions()) {
        assert(relation.first!=relation.second);
        assert(relation.year>=0 && relation.year<=settings.years);
        assert(static_cast<size_t>(relation.first)<w.people().size());
        assert(static_cast<size_t>(relation.second)<w.people().size());
    }
    fantasy::World again; again.generate(settings);
    assert(w.at(80).owner==again.at(80).owner);
    assert(w.people().size()==again.people().size());
    fantasy::World withModule;
    withModule.registerModule(std::make_unique<MagicPortentsModule>());
    withModule.generate(settings);
    bool foundMagic=false;
    for(const auto& event:withModule.events()) if(event.text.find(L"Небесное знамение")!=std::wstring::npos) foundMagic=true;
    assert(foundMagic);
    std::cout << "OK: " << w.nations().size() << " nations; " << w.people().size()
              << " named persons; " << w.events().size() << " historical events; "
              << changed << " changed cells.\n";
}
