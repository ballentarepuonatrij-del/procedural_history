#include "Fantasy/World.h"
#include "Names.h"
#include <algorithm>
#include <functional>
#include <utility>

namespace fantasy {

int World::spouseAt(const Person& person, int year) const {
    if(!aliveAt(person,year)) return -1;
    // A new marriage supersedes earlier ones. A death terminates a union;
    // only a later marriage permits remarriage.
    for(auto it=marriages_.rbegin();it!=marriages_.rend();++it) {
        if(it->year>year || (it->first!=person.id && it->second!=person.id)) continue;
        int partner=it->first==person.id?it->second:it->first;
        return aliveAt(people_[static_cast<size_t>(partner)],year)?partner:-1;
    }
    return -1;
}

int World::wealthAt(const Person& person, int year) const {
    if(person.development.empty()) return person.wealth;
    auto it=std::upper_bound(person.development.begin(),person.development.end(),year,
        [](int y,const CharacterYear& history){return y<history.year;});
    return it==person.development.begin()?it->wealth:std::prev(it)->wealth;
}

bool World::isDescendantOf(int person, int ancestor) const {
    if(person<0 || ancestor<0 || person==ancestor ||
       static_cast<size_t>(person)>=people_.size() || static_cast<size_t>(ancestor)>=people_.size()) return false;
    // IDs point only to earlier-born people; the depth bound also protects
    // the program if a future extension constructs a malformed cycle.
    std::function<bool(int,int)> rec=[&](int id,int depth)->bool {
        if(id<0 || depth>12) return false;
        const Person& p=people_[static_cast<size_t>(id)];
        if(p.parentA==ancestor || p.parentB==ancestor) return true;
        return (p.parentA>=0 && rec(p.parentA,depth+1)) ||
               (p.parentB>=0 && rec(p.parentB,depth+1));
    };
    return rec(person,0);
}

bool World::marry(int first,int second,int year) {
    if(first<0 || second<0 || first==second ||
       static_cast<size_t>(first)>=people_.size() || static_cast<size_t>(second)>=people_.size()) return false;
    const auto& a=people_[static_cast<size_t>(first)];
    const auto& b=people_[static_cast<size_t>(second)];
    if(!aliveAt(a,year) || !aliveAt(b,year) || year-a.born<18 || year-b.born<18 ||
       currentSpouses_[static_cast<size_t>(first)]!=-1 || currentSpouses_[static_cast<size_t>(second)]!=-1 ||
       stageAt(a,year).nation!=stageAt(b,year).nation ||
       stageAt(a,year).status==Status::Enslaved || stageAt(b,year).status==Status::Enslaved ||
       a.family==b.family || isDescendantOf(first,second) || isDescendantOf(second,first) ||
       (a.parentA>=0 && (a.parentA==b.parentA || a.parentA==b.parentB)) ||
       (a.parentB>=0 && (a.parentB==b.parentA || a.parentB==b.parentB))) return false;
    const std::wstring firstName=a.name,secondName=b.name;
    marriages_.push_back({year,first,second});
    interactions_.push_back({year,first,second,InteractionKind::Marriage});
    currentSpouses_[static_cast<size_t>(first)]=second;
    currentSpouses_[static_cast<size_t>(second)]=first;
    applyLifeExperience(first,LifeExperience::Marriage);
    applyLifeExperience(second,LifeExperience::Marriage);
    addBiography(first,year,L"Заключил/заключила брак с «"+secondName+L"».");
    addBiography(second,year,L"Заключил/заключила брак с «"+firstName+L"».");
    if(chance(.14)) addEvent(year,stageAt(a,year).nation,L"Заключён семейный союз «"+firstName+L"» и «"+secondName+L"».");
    return true;
}

void World::formMarriages(int year) {
    for(size_t nation=0;nation<nations_.size();++nation) {
        if(nations_[nation].founded>year || nationState_[nation].territory<=0) continue;
        const int tries=(year==0?75:14);
        int weddings=0;
        for(int i=0;i<tries && weddings<(year==0?15:3);++i) {
            if(!chance(year==0?.68:.37)) continue;
            const int a=choosePerson(static_cast<int>(nation),18,year);
            const int b=choosePerson(static_cast<int>(nation),18,year);
            if(marry(a,b,year)) ++weddings;
        }
    }
}

void World::seedFoundingFamilies() {
    // All original adults are independent founders with their own named
    // houses. This is not a fabrication of unknown generations before year 0.
    formMarriages(0);
}

void World::inheritCharacter(int child,int first,int second) {
    auto& person=people_[static_cast<size_t>(child)];
    const auto& a=people_[static_cast<size_t>(first)].innate;
    const auto& b=people_[static_cast<size_t>(second)].innate;
    auto blend=[&](uint8_t left,uint8_t right,int variation)->uint8_t {
        int value=(static_cast<int>(left)+static_cast<int>(right)+randomInt(-variation,variation))/2;
        // A little variance from the broader population prevents cloned dynasties.
        return static_cast<uint8_t>(std::clamp(value,0,100));
    };
    for(size_t i=0;i<person.innate.abilities.size();++i)
        person.innate.abilities[i]=blend(a.abilities[i],b.abilities[i],26);
    for(size_t i=0;i<person.innate.traits.size();++i)
        person.innate.traits[i]=blend(a.traits[i],b.traits[i],34);
    // Skills are learned, not genetically copied. Family teaching is handled
    // by developCharacters during the child's formative years.
    person.current=person.innate;
}

int World::addChild(int nation,int year,int first,int second) {
    if(first<0 || second<0 || first==second ||
       static_cast<size_t>(first)>=people_.size() || static_cast<size_t>(second)>=people_.size()) return -1;
    if(currentSpouses_[static_cast<size_t>(first)]!=second ||
       !aliveAt(people_[static_cast<size_t>(first)],year) || !aliveAt(people_[static_cast<size_t>(second)],year)) return -1;
    const int id=static_cast<int>(people_.size());
    const Person& a=people_[static_cast<size_t>(first)];
    const Person& b=people_[static_cast<size_t>(second)];
    const int home=(a.homeSettlement>=0 && settlements_[static_cast<size_t>(a.homeSettlement)].founded<=year)?a.homeSettlement:
        (b.homeSettlement>=0?b.homeSettlement:findSettlement(nation,year));
    Person child{};
    child.id=id;
    child.born=year;
    child.species=chance(.5)?a.species:b.species;
    child.name=makePersonName(child.species,rng_);
    child.parentA=first;
    child.parentB=second;
    child.family=chance(.65)?a.family:b.family;
    child.homeSettlement=home;
    child.wealth=2;
    child.stages.push_back({year,nation,-1,Profession::Farmer,Status::Free});
    child.biography.push_back({year,L"Родился/родилась в семье «"+a.name+L"» и «"+b.name+L"»."});
    people_.push_back(std::move(child));
    currentSpouses_.push_back(-1);
    // initializeCharacter uses a random initial potential, then the hereditary
    // component replaces abilities and personality with parental blends.
    initializeCharacter(id,year);
    inheritCharacter(id,first,second);
    people_[static_cast<size_t>(first)].children.push_back(id);
    people_[static_cast<size_t>(second)].children.push_back(id);
    addBiography(first,year,L"Родился ребёнок «"+people_[static_cast<size_t>(id)].name+L"».");
    addBiography(second,year,L"Родился ребёнок «"+people_[static_cast<size_t>(id)].name+L"».");
    if(home>=0 && chance(.10))
        addSettlementEvent(year,home,L"В семье родился ребёнок: «"+people_[static_cast<size_t>(id)].name+L"».");
    alive_.push_back(id);
    return id;
}

void World::settleInheritance(int personId,int year) {
    Person& person=people_[static_cast<size_t>(personId)];
    if(person.wealth<=0) return;
    std::vector<int> heirs;
    for(int child:person.children)
        if(aliveAt(people_[static_cast<size_t>(child)],year)) heirs.push_back(child);
    if(heirs.empty()) {
        int spouse=spouseAt(person,year-1);
        if(spouse>=0 && aliveAt(people_[static_cast<size_t>(spouse)],year)) heirs.push_back(spouse);
    }
    if(heirs.empty()) return; // Estate outside traced lineage, unclaimed in the prototype
    const int estate=person.wealth;
    person.wealth=0;
    const int equalPart=estate/static_cast<int>(heirs.size());
    const int remainder=estate%static_cast<int>(heirs.size());
    for(size_t k=0;k<heirs.size();++k) {
        int heir=heirs[k],share=equalPart+(k==0?remainder:0);
        if(share<=0) continue;
        people_[static_cast<size_t>(heir)].wealth+=share;
        inheritances_.push_back({year,personId,heir,share});
        addBiography(heir,year,L"Получил/получила наследство от «"+person.name+L"»: "+std::to_wstring(share)+L" монет.");
    }
    addBiography(personId,year,L"Наследство распределено между наследниками: "+std::to_wstring(estate)+L" монет.");
}

} // namespace fantasy
