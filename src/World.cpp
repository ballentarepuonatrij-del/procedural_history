#include "Fantasy/World.h"
#include "Names.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fantasy {
void World::registerModule(std::unique_ptr<IWorldModule> module) {
    if(module) modules_.push_back(std::move(module));
}
void World::publishEvent(int year,int nation,const std::wstring& text) {
    if(year<0 || year>settings_.years || nation<-1 || (nation>=0 && static_cast<size_t>(nation)>=nations_.size())) return;
    addEvent(year,nation,text);
}
bool World::alterNation(int nation,int treasuryDelta,int stabilityDelta) {
    if(nation<0 || static_cast<size_t>(nation)>=nationState_.size()) return false;
    auto& s=nationState_[static_cast<size_t>(nation)];
    s.treasury=std::clamp(s.treasury+treasuryDelta,0,100000);
    s.stability=std::clamp(s.stability+stabilityDelta,0,100);
    return true;
}
bool World::claimTile(int nation,int x,int y,int year) {
    if(!inside(x,y) || nation<0 || static_cast<size_t>(nation)>=nations_.size() || nations_[static_cast<size_t>(nation)].founded>year) return false;
    int idx=index(x,y);
    if(tiles_[static_cast<size_t>(idx)].terrain==Terrain::Sea) return false;
    owners_[static_cast<size_t>(idx)]=static_cast<int16_t>(nation);
    return true;
}
bool World::addPersonNote(int person,int year,const std::wstring& text) {
    if(person<0 || static_cast<size_t>(person)>=people_.size()) return false;
    const auto& p=people_[static_cast<size_t>(person)];
    if(year<p.born || year>settings_.years) return false;
    addBiography(person,year,text);
    return true;
}
bool World::grantExperience(int id,int year,LifeExperience experience) {
    if(id<0 || static_cast<size_t>(id)>=people_.size() || year<0 || year>settings_.years) return false;
    const auto& p=people_[static_cast<size_t>(id)];
    if(!aliveAt(p,year)) return false;
    applyLifeExperience(id,experience);
    return true;
}
bool World::publishSettlementEvent(int year,int settlementId,const std::wstring& text) {
    if(year<0 || year>settings_.years || settlementId<0 || static_cast<size_t>(settlementId)>=settlements_.size() ||
       year<settlements_[static_cast<size_t>(settlementId)].founded) return false;
    addSettlementEvent(year,settlementId,text);
    return true;
}
int World::randomInt(int min, int max) { return std::uniform_int_distribution<int>(min,max)(rng_); }
bool World::chance(double probability) { return std::bernoulli_distribution(std::clamp(probability,0.,1.))(rng_); }
const Snapshot& World::at(int year) const {
    if(timeline_.empty()) throw std::logic_error("World is not generated");
    return timeline_[static_cast<size_t>(std::clamp(year,0,settings_.years))];
}
const PersonStage& World::stageAt(const Person& p, int year) const {
    auto it=std::upper_bound(p.stages.begin(),p.stages.end(),year,[](int y,const PersonStage& s){return y<s.year;});
    if(it==p.stages.begin()) return p.stages.front();
    return *std::prev(it);
}
const CharacterStats& World::statsAt(const Person& p, int year) const {
    if(p.development.empty()) return p.innate;
    auto it=std::upper_bound(p.development.begin(),p.development.end(),year,
        [](int y,const CharacterYear& item){ return y<item.year; });
    if(it==p.development.begin()) return it->stats;
    return std::prev(it)->stats;
}
bool World::aliveAt(const Person& p, int year) const { return p.born<=year && year<p.died; }
int World::nationAt(const Person& p, int year) const { return stageAt(p,year).nation; }
void World::addEvent(int year,int nation,const std::wstring& msg,int person) { events_.push_back({year,nation,person,msg}); }
void World::addSettlementEvent(int year,int id,const std::wstring& msg) {
    settlementEvents_.push_back({year,id,msg});
}
void World::addBiography(int id,int year,const std::wstring& msg) { people_[static_cast<size_t>(id)].biography.push_back({year,msg}); }
void World::changeStage(int id,int year,int nation,Profession profession,Status status,int guild) {
    Person& p=people_[static_cast<size_t>(id)];
    if(!p.stages.empty()) {
        const auto previous=p.stages.back();
        if(previous.guild!=guild && guild>=0) applyLifeExperience(id,LifeExperience::Guild);
        if(previous.profession!=profession) {
            // Initial retraining: effort and discipline, followed by profession-specific learning.
            p.current.traits[static_cast<size_t>(Personality::Discipline)]=static_cast<uint8_t>(
                std::min(100,static_cast<int>(p.current.traits[static_cast<size_t>(Personality::Discipline)])+1));
        }
        if(previous.status!=status) {
            if(status==Status::Enslaved) applyLifeExperience(id,LifeExperience::Captivity);
            else if(previous.status==Status::Enslaved) applyLifeExperience(id,LifeExperience::Freedom);
        }
        if(previous.nation!=nation && nation>=0) applyLifeExperience(id,LifeExperience::Rebellion);
    }
    p.stages.push_back({year,nation,guild,profession,status});
}
int World::findSettlement(int nation, int year) const {
    // Предпочитаем ближайшее к столице поселение на территории государства.
    for(const auto& s:settlements_) if(s.founded<=year && owners_[static_cast<size_t>(index(s.position.x,s.position.y))]==nation) return s.id;
    return -1;
}
void World::addSettlement(int nation,int year,Point pos,bool capital) {
    if(!inside(pos.x,pos.y)) return;
    for(const auto& s:settlements_) if(s.position.x==pos.x && s.position.y==pos.y) return;
    Species kind=nations_[static_cast<size_t>(nation)].culture;
    SettlementKind sk=SettlementKind::Village;
    switch(kind) {
        case Species::Human: sk=capital?SettlementKind::City:(chance(.22)?SettlementKind::Castle:SettlementKind::Village); break;
        case Species::Elf: sk=SettlementKind::Grove; break;
        case Species::Dwarf: sk=SettlementKind::MountainCity; break;
        case Species::Outlaw: sk=SettlementKind::Hideout; break;
        case Species::Folk: sk=SettlementKind::FairyVillage; break;
    }
    int id=static_cast<int>(settlements_.size());
    settlements_.push_back({id,makeSettlementName(kind,nextSettlementName_++),pos,sk,year,nation});
    settlementState_.push_back({nation,0,randomInt(38,78)});
    addSettlementEvent(year,id,L"Основано поселение «"+settlements_.back().name+L"» государством «"+
                            nations_[static_cast<size_t>(nation)].name+L"».");
    if(year>0) addEvent(year,nation,L"Основано поселение «"+settlements_.back().name+L"».");
}
int World::createNation(Species species,Point capital,int year) {
    int id=static_cast<int>(nations_.size());
    Nation n{id,makeNationName(species,id),species,startingGovernment(species,rng_),nationColor(id,species),capital,year,-1};
    nations_.push_back(n);
    NationYear state{};
    state.government=n.startingGovernment;
    state.stability=randomInt(48,88);
    state.treasury=randomInt(90,220);
    state.military=randomInt(17,52);
    nationState_.push_back(state);
    owners_[static_cast<size_t>(index(capital.x,capital.y))]=static_cast<int16_t>(id);
    addSettlement(id,year,capital,true);
    addEvent(year,id,L"Основано объединение «"+n.name+L"» ("+speciesName(species)+L").");
    return id;
}
int World::addPerson(int nation,int year,bool newborn) {
    Species main=nations_[static_cast<size_t>(nation)].culture;
    Species species=main;
    if(main==Species::Human && chance(.12)) species=chance(.5)?Species::Elf:Species::Dwarf;
    if(main==Species::Outlaw) {
        int roll=randomInt(0,99);
        species=roll<61?Species::Human:roll<82?Species::Dwarf:roll<96?Species::Elf:Species::Folk;
    }
    if(main==Species::Elf && chance(.02)) species=Species::Human;
    if(main==Species::Dwarf && chance(.03)) species=Species::Human;
    int born=newborn?year:(year-randomInt(0,49));
    int id=static_cast<int>(people_.size());
    Person p{};
    p.id=id;
    p.name=(main==Species::Outlaw&&chance(.55))?makePersonName(Species::Outlaw,rng_):makePersonName(species,rng_);
    p.species=species;
    p.born=born;
    // Distribute births and initial inhabitants among the actual settlements,
    // rather than attributing everybody to the first town of the country.
    std::vector<int> possibleHomes;
    for(const auto& town:settlements_) {
        if(town.founded<=year && owners_[static_cast<size_t>(index(town.position.x,town.position.y))]==nation)
            possibleHomes.push_back(town.id);
    }
    p.homeSettlement=possibleHomes.empty()?-1:
        possibleHomes[static_cast<size_t>(randomInt(0,static_cast<int>(possibleHomes.size())-1))];
    Profession profession=newborn?Profession::Farmer:startingProfession(main,rng_);
    const bool cityOutlaw=main==Species::Human && !newborn && chance(.08);
    Status status=(main==Species::Outlaw||cityOutlaw)?Status::Outlaw:Status::Free;
    if(cityOutlaw) profession=Profession::Thief;
    p.stages.push_back({born,nation,-1,profession,status});
    p.wealth=newborn?0:randomInt(0,45);
    if(profession==Profession::Noble) p.wealth+=90;
    else if(profession==Profession::Merchant) p.wealth+=45;
    p.biography.push_back({born,L"Родился/родилась в землях «"+nations_[static_cast<size_t>(nation)].name+L"»."});
    if(cityOutlaw) p.biography.push_back({0,L"Вступил/вступила в преступную группировку, действующую в городах государства."});
    people_.push_back(std::move(p));
    currentSpouses_.push_back(-1);
    if(!newborn) {
        const int familyId=static_cast<int>(families_.size());
        families_.push_back({familyId,id,year,L"Дом «"+people_.back().name+L"» (#"+std::to_wstring(id)+L")"});
        people_.back().family=familyId;
    }
    initializeCharacter(id,year);
    if(newborn && people_.back().homeSettlement>=0 && chance(.055))
        addSettlementEvent(year,people_.back().homeSettlement,L"Родился новый житель: «"+people_.back().name+L"».");
    alive_.push_back(id);
    return id;
}
void World::startPeople(int nation,int number,int year,bool newborn) {
    for(int i=0;i<number;++i) addPerson(nation,year,newborn);
}
void World::addGuild(int nation,int year) {
    Species type=nations_[static_cast<size_t>(nation)].culture;
    std::wstring name,purpose;
    switch(type) {
        case Species::Human:
            if(chance(.15)) {name=L"Теневой картель";purpose=L"Контрабанда, тайные договоры и теневые рынки";}
            else {name=chance(.5)?L"Союз караванщиков":L"Коллегия мастеров";purpose=L"Торговля, ремёсла и защита монополий";}
            break;
        case Species::Elf: name=L"Круг хранителей рощ";purpose=L"Исцеление, охрана лесов и древние знания";break;
        case Species::Dwarf: name=L"Цех шахтёров и кузнецов";purpose=L"Добыча руды, металлургия и горные промыслы";break;
        case Species::Outlaw: name=L"Контрабандный картель";purpose=L"Нелегальная торговля, пиратство и подкуп";break;
        case Species::Folk: name=L"Содружество тихих троп";purpose=L"Скрытные ремёсла, травничество и взаимопомощь";break;
    }
    int id=static_cast<int>(guilds_.size());
    guilds_.push_back({id,nation,year,name+L" №"+std::to_wstring(id+1),purpose});
    const int town=findSettlement(nation,year);
    if(town>=0) addSettlementEvent(year,town,L"Начало действовать объединение «"+guilds_.back().name+L"»: "+purpose+L".");
    if(year>0) addEvent(year,nation,L"Основано объединение: "+guilds_.back().name+L".");
    int recruited=0;
    for(int pid:alive_) {
        auto& p=people_[static_cast<size_t>(pid)];
        const auto& stage=p.stages.back();
        if(stage.nation!=nation || stage.guild!=-1 || p.born>year-15) continue;
        if(chance(.10)) {
            changeStage(pid,year,nation,stage.profession,stage.status,id);
            addBiography(pid,year,L"Вступил/вступила в «"+guilds_.back().name+L"».");
            if(++recruited>=8) break;
        }
    }
}
void World::initialNations() {
    const Species sequence[]{Species::Human,Species::Elf,Species::Dwarf,Species::Outlaw,Species::Folk,
                             Species::Human,Species::Human,Species::Elf,Species::Dwarf,Species::Human};
    for(int i=0;i<settings_.civilizations;++i) {
        Species species=sequence[static_cast<size_t>(i)%10];
        Point capital=chooseCapital(species);
        int id=createNation(species,capital,0);
        // Небольшое первоначальное владение; дальнейшие границы изменяются по годам.
        const int radius=species==Species::Folk?2:3;
        for(int dy=-radius;dy<=radius;++dy) for(int dx=-radius;dx<=radius;++dx) {
            int x=capital.x+dx,y=capital.y+dy;
            if(!inside(x,y) || dx*dx+dy*dy>radius*radius) continue;
            int ii=index(x,y);
            if(tiles_[static_cast<size_t>(ii)].terrain!=Terrain::Sea && owners_[static_cast<size_t>(ii)]==-1)
                owners_[static_cast<size_t>(ii)]=static_cast<int16_t>(id);
        }
        int pop=species==Species::Folk?35:species==Species::Outlaw?45:randomInt(55,78);
        startPeople(id,pop,0,false);
        addGuild(id,0);
        if(species==Species::Human || species==Species::Dwarf) addGuild(id,0);
        nationState_[static_cast<size_t>(id)].ruler=findRuler(id,0);
    }
    updateCensus();
    seedFoundingFamilies();
}
int World::choosePerson(int nation,int minAge,int year) {
    if(alive_.empty()) return -1;
    auto canLead=[&](const Person& p) {
        const auto& stage=stageAt(p,year);
        return stage.status==Status::Free ||
               (nations_[static_cast<size_t>(nation)].culture==Species::Outlaw && stage.status==Status::Outlaw);
    };
    for(int tries=0;tries<100;++tries) {
        int id=alive_[static_cast<size_t>(randomInt(0,static_cast<int>(alive_.size())-1))];
        const Person& p=people_[static_cast<size_t>(id)];
        if(p.born<=year-minAge && nationAt(p,year)==nation && canLead(p)) return id;
    }
    for(int id:alive_) {
        const Person& p=people_[static_cast<size_t>(id)];
        if(p.born<=year-minAge && nationAt(p,year)==nation && canLead(p)) return id;
    }
    return -1;
}
int World::findRuler(int nation,int year,int predecessor) {
    // Rulers are selected based on developed leadership skills, not purely at random.
    int winner=-1;
    int best=std::numeric_limits<int>::min();
    for(int id:alive_) {
        const Person& p=people_[static_cast<size_t>(id)];
        if(p.born>year-20 || nationAt(p,year)!=nation) continue;
        const auto& st=stageAt(p,year);
        if(st.status!=Status::Free && !(nations_[static_cast<size_t>(nation)].culture==Species::Outlaw && st.status==Status::Outlaw)) continue;
        const auto& c=p.current;
        int score=static_cast<int>(c.abilities[static_cast<size_t>(Ability::Charisma)])+
                  static_cast<int>(c.abilities[static_cast<size_t>(Ability::Willpower)])+
                  static_cast<int>(c.skills[static_cast<size_t>(Skill::Diplomacy)])+
                  static_cast<int>(c.traits[static_cast<size_t>(Personality::Ambition)]);
        if(predecessor>=0 && nationState_[static_cast<size_t>(nation)].government==Government::Monarchy &&
           isDescendantOf(id,predecessor)) score+=450; // Prefer eligible heirs to outsiders
        if(nations_[static_cast<size_t>(nation)].culture==Species::Outlaw)
            score+=static_cast<int>(c.skills[static_cast<size_t>(Skill::Combat)])+
                   static_cast<int>(c.skills[static_cast<size_t>(Skill::Stealth)]);
        if(score>best) { best=score;winner=id; }
    }
    if(winner!=-1) {
        applyLifeExperience(winner,LifeExperience::Leadership);
        addBiography(winner,year,L"Возглавил/возглавила государство «"+nations_[static_cast<size_t>(nation)].name+L"».");
    }
    return winner;
}
void World::updateCensus() {
    for(auto& s:nationState_) { s.population=0; s.territory=0; s.settlements=0; }
    for(int16_t id:owners_) if(id>=0 && static_cast<size_t>(id)<nationState_.size()) ++nationState_[static_cast<size_t>(id)].territory;
    for(int id:alive_) {
        const int nation=people_[static_cast<size_t>(id)].stages.back().nation;
        if(nation>=0 && static_cast<size_t>(nation)<nationState_.size()) ++nationState_[static_cast<size_t>(nation)].population;
    }
    for(const auto& settlement:settlements_) {
        int owner=owners_[static_cast<size_t>(index(settlement.position.x,settlement.position.y))];
        if(owner>=0 && static_cast<size_t>(owner)<nationState_.size()) ++nationState_[static_cast<size_t>(owner)].settlements;
    }
}
void World::capture(int year) {
    for(const Settlement& town:settlements_) {
        auto& state=settlementState_[static_cast<size_t>(town.id)];
        if(town.founded>year) continue;
        const int owner=owners_[static_cast<size_t>(index(town.position.x,town.position.y))];
        if(state.owner!=owner) {
            std::wstring event;
            if(owner<0) event=L"Поселение оказалось вне власти государств.";
            else if(state.owner<0) event=L"Поселение вошло в состав «"+nations_[static_cast<size_t>(owner)].name+L"».";
            else event=L"Поселение перешло от «"+nations_[static_cast<size_t>(state.owner)].name+
                            L"» к «"+nations_[static_cast<size_t>(owner)].name+L"».";
            addSettlementEvent(year,town.id,event);
            state.owner=owner;
            state.prosperity=std::clamp(state.prosperity-9,0,100);
        }
        state.knownResidents=0;
    }
    // Residency is not independently simulated yet.  The counter reflects named
    // natives still associated with their birthplace and its current polity.
    for(int id:alive_) {
        const Person& person=people_[static_cast<size_t>(id)];
        const int home=person.homeSettlement;
        if(home<0 || static_cast<size_t>(home)>=settlements_.size()) continue;
        auto& state=settlementState_[static_cast<size_t>(home)];
        if(state.owner>=0 && person.stages.back().nation==state.owner) ++state.knownResidents;
    }
    recordCharacterProgress(year);
    timeline_.push_back({year,owners_,nationState_,settlementState_});
}
void World::generate(const Settings& requested) {
    settings_=requested;
    settings_.civilizations=std::clamp(settings_.civilizations,1,28);
    settings_.years=std::clamp(settings_.years,1,400);
    settings_.width=std::clamp(settings_.width,40,160);
    settings_.height=std::clamp(settings_.height,30,100);
    rng_.seed(settings_.seed);
    tiles_.clear(); owners_.clear(); nations_.clear(); nationState_.clear(); people_.clear(); alive_.clear(); settlements_.clear(); settlementState_.clear(); settlementEvents_.clear(); guilds_.clear(); events_.clear(); interactions_.clear(); families_.clear(); marriages_.clear(); inheritances_.clear(); currentSpouses_.clear(); timeline_.clear(); nextSettlementName_=0;
    createTerrain(); initialNations();
    for(const auto& module:modules_) module->onWorldCreated(*this);
    updateCensus(); capture(0);
    for(int year=1;year<=settings_.years;++year) {
        simulateYear(year);
        for(const auto& module:modules_) module->onYear(*this,year);
        updateCensus();capture(year);
    }
}
} // namespace fantasy
