#include "Fantasy/World.h"
#include "Names.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace fantasy {
int World::chooseFrontierTile(int nation, bool unclaimedOnly) {
    std::vector<int> candidates;
    candidates.reserve(512);
    constexpr int dx[]{1,-1,0,0},dy[]{0,0,1,-1};
    const auto culture=nations_[static_cast<size_t>(nation)].culture;
    for(int y=1;y<settings_.height-1;++y) for(int x=1;x<settings_.width-1;++x) {
        int idx=index(x,y);
        if(owners_[static_cast<size_t>(idx)]==nation || tiles_[static_cast<size_t>(idx)].terrain==Terrain::Sea) continue;
        if(unclaimedOnly && owners_[static_cast<size_t>(idx)]!=-1) continue;
        bool neighboring=false;
        for(int dir=0;dir<4;++dir) if(owners_[static_cast<size_t>(index(x+dx[dir],y+dy[dir]))]==nation) {neighboring=true;break;}
        if(!neighboring) continue;
        candidates.push_back(idx);
        const auto t=tiles_[static_cast<size_t>(idx)].terrain;
        const bool favored=(culture==Species::Elf&&(t==Terrain::Forest||t==Terrain::Jungle)) ||
             (culture==Species::Folk&&(t==Terrain::Forest||t==Terrain::Jungle)) ||
             (culture==Species::Dwarf&&t==Terrain::Mountain) ||
             (culture==Species::Human&&t==Terrain::Plain) ||
             (culture==Species::Outlaw&&isCoastal(x,y));
        if(favored) { candidates.push_back(idx);candidates.push_back(idx); }
        if(owners_[static_cast<size_t>(idx)]==-1) candidates.push_back(idx);
    }
    return candidates.empty()?-1:candidates[static_cast<size_t>(randomInt(0,static_cast<int>(candidates.size())-1))];
}
void World::simulateTerritories(int year) {
    constexpr int dx[]{1,-1,0,0},dy[]{0,0,1,-1};
    const int count=static_cast<int>(nations_.size());
    for(int id=0;id<count;++id) {
        if(nationState_[static_cast<size_t>(id)].territory==0) continue;
        Species type=nations_[static_cast<size_t>(id)].culture;
        double expansion=type==Species::Human?.78:type==Species::Elf?.20:type==Species::Dwarf?.25:type==Species::Outlaw?.40:.13;
        int moves=type==Species::Human?2:1;
        for(int move=0;move<moves;++move) {
            if(!chance(expansion)) continue;
            int target=chooseFrontierTile(id,chance(.78));
            if(target<0) continue;
            int previous=owners_[static_cast<size_t>(target)];
            if(previous>=0) {
                int att=nationState_[static_cast<size_t>(id)].military,def=nationState_[static_cast<size_t>(previous)].military;
                double attackChance=std::clamp(.42+(att-def)/180.,.10,.78);
                if(type==Species::Elf || type==Species::Folk) attackChance*=.18;
                if(nations_[static_cast<size_t>(previous)].culture==Species::Elf) attackChance*=.52;
                if(!chance(attackChance)) continue;
                nationState_[static_cast<size_t>(previous)].stability=std::max(5,nationState_[static_cast<size_t>(previous)].stability-2);
                if(chance(.27)) addEvent(year,id,L"На границе с «"+nations_[static_cast<size_t>(previous)].name+L"» захвачены земли.");
                if(chance(.30)) addEvent(year,previous,L"Потеряны пограничные земли в столкновении с «"+nations_[static_cast<size_t>(id)].name+L"».");
            } else if(chance(.08)) {
                addEvent(year,id,L"Переселенцы присоединили новые земли.");
            }
            owners_[static_cast<size_t>(target)]=static_cast<int16_t>(id);
            int x=target%settings_.width,y=target/settings_.width;
            for(int dir=0;dir<4;++dir) {
                int xx=x+dx[dir],yy=y+dy[dir];
                if(inside(xx,yy) && owners_[static_cast<size_t>(index(xx,yy))]==-1 && chance(.045) && tiles_[static_cast<size_t>(index(xx,yy))].terrain!=Terrain::Sea)
                    owners_[static_cast<size_t>(index(xx,yy))]=static_cast<int16_t>(id);
            }
        }
    }
}
void World::simulatePopulation(int year) {
    std::vector<int> survivors;
    survivors.reserve(alive_.size()+100);
    for(int id:alive_) {
        Person& p=people_[static_cast<size_t>(id)];
        const int age=year-p.born;
        int maxAge=100;
        switch(p.species) {
            case Species::Human: maxAge=91;break; case Species::Elf: maxAge=230;break;
            case Species::Dwarf: maxAge=160;break; case Species::Outlaw: maxAge=83;break; case Species::Folk: maxAge=115;break;
        }
        double mortality=(age>maxAge)?1.0:(age>maxAge*.70)?.12:(age>maxAge*.5)?.042:.012;
        if(stageAt(p,year).status==Status::Enslaved) mortality+=.027;
        if(chance(mortality)) {
            p.died=year;
            p.biography.push_back({year,L"Умер/умерла в возрасте "+std::to_wstring(age)+L" лет."});
            int home=p.stages.back().nation;
            if(home>=0 && chance(.023)) addEvent(year,home,L"Скончался/скончалась «"+p.name+L"».",id);
            if(p.homeSettlement>=0 && chance(.045))
                addSettlementEvent(year,p.homeSettlement,L"Скончался уроженец поселения: «"+p.name+L"».");
        } else {
            survivors.push_back(id);
            if(age==16) {
                PersonStage before=p.stages.back();
                if(before.status==Status::Free) {
                    Profession next=startingProfession(nations_[static_cast<size_t>(before.nation)].culture,rng_);
                    changeStage(id,year,before.nation,next,before.status,before.guild);
                    addBiography(id,year,L"Освоил/освоила ремесло: "+std::wstring(professionName(next))+L".");
                }
            }
        }
    }
    alive_=std::move(survivors);
    std::vector<int> counts(nations_.size(),0);
    for(int id:alive_) {
        int nation=people_[static_cast<size_t>(id)].stages.back().nation;
        if(nation>=0) ++counts[static_cast<size_t>(nation)];
    }
    for(size_t n=0;n<nations_.size();++n) {
        if(nationState_[n].territory==0 || nations_[n].founded>year) continue;
        const auto kind=nations_[n].culture;
        int maxPop=kind==Species::Human?220:kind==Species::Elf?160:kind==Species::Dwarf?190:kind==Species::Outlaw?125:95;
        // Рождение каждого жителя — отдельная сущность; лимит защищает ресурсы ПК.
        int capacity=std::max(0,maxPop-counts[n]);
        int births=0;
        if(capacity>0) {
            double rate=kind==Species::Elf?.018:kind==Species::Dwarf?.024:.035;
            for(int attempt=0;attempt<std::min(7,capacity);++attempt) if(chance(rate*std::min(10.,capacity/12.))) ++births;
        }
        for(int i=0;i<births;++i) addPerson(static_cast<int>(n),year,true);
        if(births>=4 && chance(.10)) addEvent(year,static_cast<int>(n),L"В городах и селениях наблюдается прирост населения.");
    }
}
void World::simulatePolitics(int year) {
    for(size_t i=0;i<nations_.size();++i) {
        auto& state=nationState_[i];
        if(state.territory<=0) continue;
        state.treasury=std::clamp(state.treasury+randomInt(-6,10),0,3000);
        state.stability=std::clamp(state.stability+randomInt(-2,2),5,100);
        state.military=std::clamp(state.military+randomInt(-2,2),8,100);
        if(state.ruler<0 || !aliveAt(people_[static_cast<size_t>(state.ruler)],year) ||
           nationAt(people_[static_cast<size_t>(state.ruler)],year)!=static_cast<int>(i) ||
           (stageAt(people_[static_cast<size_t>(state.ruler)],year).status!=Status::Free &&
            !(nations_[i].culture==Species::Outlaw && stageAt(people_[static_cast<size_t>(state.ruler)],year).status==Status::Outlaw))) {
            state.ruler=findRuler(static_cast<int>(i),year);
            if(state.ruler>=0) addEvent(year,static_cast<int>(i),L"К власти пришёл/пришла «"+people_[static_cast<size_t>(state.ruler)].name+L"».",state.ruler);
        }
        if(chance(.008) && nations_[i].culture==Species::Human) {
            const auto next=static_cast<Government>(randomInt(0,5));
            state.government=next;
            addEvent(year,static_cast<int>(i),L"Государственный переворот и изменение формы правления: "+std::wstring(governmentName(next))+L".");
            state.stability=std::max(12,state.stability-18);
        }
        if(chance(.012)) {
            state.stability=std::max(6,state.stability-randomInt(5,14));
            state.treasury=std::max(0,state.treasury-randomInt(8,37));
            addEvent(year,static_cast<int>(i),L"Неурожай, эпидемия или междоусобица ослабили государство.");
        }
        if(chance(.01)) {
            state.stability=std::min(100,state.stability+9);
            addEvent(year,static_cast<int>(i),L"Мирный договор укрепил внутреннее единство.");
        }
    }
}
void World::simulateSociety(int year) {
    const int currentNations=static_cast<int>(nations_.size());
    for(int n=0;n<currentNations;++n) {
        if(nationState_[static_cast<size_t>(n)].territory<=0) continue;
        auto species=nations_[static_cast<size_t>(n)].culture;
        if(chance(.06) && nationState_[static_cast<size_t>(n)].territory>8) {
            for(int trial=0;trial<40;++trial) {
                int idx=randomInt(0,static_cast<int>(owners_.size())-1);
                if(owners_[static_cast<size_t>(idx)]!=n) continue;
                int x=idx%settings_.width,y=idx/settings_.width;
                auto ground=tiles_[static_cast<size_t>(idx)].terrain;
                bool suitable=(species==Species::Human) ||
                    (species==Species::Dwarf && ground==Terrain::Mountain) ||
                    (species==Species::Elf && (ground==Terrain::Forest||ground==Terrain::Jungle)) ||
                    (species==Species::Outlaw && (isCoastal(x,y)||ground==Terrain::Forest)) ||
                    (species==Species::Folk && (ground==Terrain::Forest||ground==Terrain::Jungle));
                if(suitable) { addSettlement(n,year,{x,y},false);break; }
            }
        }
        if(chance(.012) && guilds_.size()<200) addGuild(n,year);
        if(chance(.072)) {
            int id=choosePerson(n,17,year);
            if(id>=0) {
                auto stage=people_[static_cast<size_t>(id)].stages.back();
                int guild=-1;
                for(const auto& g:guilds_) if(g.nation==n && g.founded<=year) { guild=g.id; if(chance(.55))break; }
                if(guild>=0 && stage.guild!=guild) {
                    changeStage(id,year,n,stage.profession,stage.status,guild);
                    addBiography(id,year,L"Присоединился/присоединилась к объединению «"+guilds_[static_cast<size_t>(guild)].name+L"».");
                } else {
                    addBiography(id,year,L"Прославился/прославилась профессиональным мастерством.");
                }
            }
        }
        if(chance(.06)) {
            int id=choosePerson(n,19,year);
            if(id>=0) {
                auto& p=people_[static_cast<size_t>(id)];
                std::wstring incident;
                switch(randomInt(0,3)) {
                    case 0: incident=L"Отправился/отправилась в дальнюю экспедицию.";break;
                    case 1: incident=L"Получил/получила признание ремесленной гильдии.";break;
                    case 2: incident=L"Вступил/вступила в спор с местными властями.";break;
                    default: incident=L"Участвовал/участвовала в большой ярмарке.";break;
                }
                p.biography.push_back({year,incident});
                if(chance(.15)) addEvent(year,n,p.name+L": "+incident,id);
            }
        }
        if(species==Species::Human && chance(.13)) {
            int other=randomInt(0,currentNations-1);
            if(other!=n && nationState_[static_cast<size_t>(other)].territory>0) {
                nationState_[static_cast<size_t>(n)].treasury+=randomInt(2,9);
                nationState_[static_cast<size_t>(other)].treasury+=randomInt(1,5);
                if(chance(.15)) addEvent(year,n,L"Караваны установили торговые связи с «"+nations_[static_cast<size_t>(other)].name+L"».");
            }
        }
        if((species==Species::Dwarf && chance(.09)) || (species==Species::Outlaw && chance(.14))) {
            // Набеги могут переместить реального персонажа, меняя и его биографию.
            int victim=-1,from=-1;
            if(!alive_.empty()) for(int attempt=0;attempt<120;++attempt) {
                int id=alive_[static_cast<size_t>(randomInt(0,static_cast<int>(alive_.size())-1))];
                const Person& p=people_[static_cast<size_t>(id)];
                int origin=p.stages.back().nation;
                if(origin!=n && origin>=0 && p.species!=Species::Dwarf && p.born<=year-16 && p.stages.back().status==Status::Free) {
                    victim=id;from=origin;break;
                }
            }
            if(victim>=0) {
                changeStage(victim,year,n,species==Species::Dwarf?Profession::Miner:Profession::Sailor,Status::Enslaved,-1);
                addBiography(victim,year,L"Пленён/пленена налётчиками из «"+nations_[static_cast<size_t>(n)].name+L"» и обращён/обращена в рабство.");
                addEvent(year,n,L"Отряд совершил набег и захватил пленника: "+people_[static_cast<size_t>(victim)].name+L".",victim);
                if(chance(.65)) addEvent(year,from,L"Набег выходцев из «"+nations_[static_cast<size_t>(n)].name+L"»: похищен житель «"+people_[static_cast<size_t>(victim)].name+L"».",victim);
            }
        }
        if(species==Species::Elf && chance(.046)) {
            addEvent(year,n,L"Совет рощ провёл обряд защиты лесных владений.");
            nationState_[static_cast<size_t>(n)].stability=std::min(100,nationState_[static_cast<size_t>(n)].stability+2);
        }
        if(species==Species::Folk && chance(.047)) addEvent(year,n,L"Скрытные общины сменили тайные лесные тропы, укрывшись от чужаков.");
        if(species==Species::Human && chance(.04)) {
            // Городские рынки рабов: статус уже поименованных людей сохраняется в биографии.
            int target=choosePerson(n,18,year);
            if(target>=0 && chance(.32)) {
                changeStage(target,year,n,Profession::Farmer,Status::Enslaved,-1);
                addBiography(target,year,L"Лишён/лишена свободы в результате судебного приговора или долгов.");
            }
        }
    }
    if(chance(.11)) {
        for(int attempts=0;attempts<50 && !alive_.empty();++attempts) {
            int id=alive_[static_cast<size_t>(randomInt(0,static_cast<int>(alive_.size())-1))];
            auto st=people_[static_cast<size_t>(id)].stages.back();
            if(st.status==Status::Enslaved) {
                changeStage(id,year,st.nation,Profession::Artisan,Status::Free,-1);
                addBiography(id,year,L"Получил/получила свободу.");
                if(chance(.30)) addEvent(year,st.nation,L"Освобождён бывший раб «"+people_[static_cast<size_t>(id)].name+L"».",id);
                break;
            }
        }
    }
}
void World::simulateInteractions(int year) {
    // Оба участника существуют в мире; запись попадает в обе биографии.
    for(size_t nation=0;nation<nations_.size();++nation) {
        if(nationState_[nation].territory<=0 || !chance(.18)) continue;
        int a=choosePerson(static_cast<int>(nation),16,year);
        int b=choosePerson(static_cast<int>(nation),16,year);
        if(a<0 || b<0 || a==b) continue;
        auto kind=static_cast<InteractionKind>(randomInt(0,5));
        const std::wstring& an=people_[static_cast<size_t>(a)].name;
        const std::wstring& bn=people_[static_cast<size_t>(b)].name;
        std::wstring action;
        switch(kind) {
            case InteractionKind::Partnership: action=L"Заключили торговое соглашение";break;
            case InteractionKind::Friendship: action=L"Стали близкими друзьями";break;
            case InteractionKind::Rivalry: action=L"Начали длительное соперничество";break;
            case InteractionKind::Mentorship: action=L"Установили отношения учителя и ученика";break;
            case InteractionKind::Marriage: action=L"Заключили брак";break;
            case InteractionKind::Crime: action=L"Оказались участниками криминальной сделки";break;
        }
        interactions_.push_back({year,a,b,kind});
        addBiography(a,year,action+L" с «"+bn+L"»." );
        addBiography(b,year,action+L" с «"+an+L"»." );
        if(chance(.26)) addEvent(year,static_cast<int>(nation),an+L" и "+bn+L": "+action+L".");
    }
}
void World::simulateRebellion(int year) {
    if(!chance(.028) || nations_.size()>=static_cast<size_t>(std::min(38,settings_.civilizations+9))) return;
    std::vector<int> parents;
    for(size_t n=0;n<nationState_.size();++n) if(nationState_[n].territory>=45 && nations_[n].culture!=Species::Folk) parents.push_back(static_cast<int>(n));
    if(parents.empty()) return;
    int parent=parents[static_cast<size_t>(randomInt(0,static_cast<int>(parents.size())-1))];
    const auto oldCapital=nations_[static_cast<size_t>(parent)].capital;
    std::vector<int> candidates;
    for(int y=2;y<settings_.height-2;++y) for(int x=2;x<settings_.width-2;++x) {
        int idx=index(x,y);
        if(owners_[static_cast<size_t>(idx)]!=parent) continue;
        if(std::abs(x-oldCapital.x)+std::abs(y-oldCapital.y)<7) continue;
        bool settlementPresent=false;
        for(const auto& s:settlements_) if(s.position.x==x&&s.position.y==y) {settlementPresent=true;break;}
        if(!settlementPresent) candidates.push_back(idx);
    }
    if(candidates.empty()) return;
    int idx=candidates[static_cast<size_t>(randomInt(0,static_cast<int>(candidates.size())-1))];
    Point point{idx%settings_.width,idx/settings_.width};
    auto culture=nations_[static_cast<size_t>(parent)].culture;
    if(culture==Species::Human && chance(.16)) culture=Species::Outlaw;
    int rebel=createNation(culture,point,year);
    for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx) {
        int x=point.x+dx,y=point.y+dy;
        if(inside(x,y) && dx*dx+dy*dy<=5 && owners_[static_cast<size_t>(index(x,y))]==parent)
            owners_[static_cast<size_t>(index(x,y))]=static_cast<int16_t>(rebel);
    }
    int defectors=0;
    for(int pid:alive_) {
        auto stage=people_[static_cast<size_t>(pid)].stages.back();
        if(stage.nation==parent && stage.status!=Status::Enslaved && chance(.12)) {
            changeStage(pid,year,rebel,stage.profession,stage.status,-1);
            addBiography(pid,year,L"Примкнул/примкнула к восстанию и перешёл/перешла в «"+nations_[static_cast<size_t>(rebel)].name+L"».");
            if(++defectors>=15) break;
        }
    }
    if(defectors==0) {
        int id=choosePerson(parent,18,year);
        if(id>=0) {
            auto stage=people_[static_cast<size_t>(id)].stages.back();
            changeStage(id,year,rebel,stage.profession,stage.status,-1);
            addBiography(id,year,L"Переселился/переселилась на земли нового государства.");
        }
    }
    addGuild(rebel,year);
    nationState_[static_cast<size_t>(rebel)].ruler=findRuler(rebel,year);
    nationState_[static_cast<size_t>(parent)].stability=std::max(5,nationState_[static_cast<size_t>(parent)].stability-22);
    addEvent(year,parent,L"Восстание: от государства отделились земли «"+nations_[static_cast<size_t>(rebel)].name+L"».");
    addEvent(year,rebel,L"Восставшие добились независимости от «"+nations_[static_cast<size_t>(parent)].name+L"».");
}
void World::simulateSettlements(int year) {
    // City-specific history survives changes of borders and even loss of statehood.
    for(const Settlement& town:settlements_) {
        if(town.founded>year) continue;
        auto& state=settlementState_[static_cast<size_t>(town.id)];
        int owner=owners_[static_cast<size_t>(index(town.position.x,town.position.y))];
        if(owner<0) {
            state.prosperity=std::max(0,state.prosperity-1);
            continue;
        }
        state.prosperity=std::clamp(state.prosperity+randomInt(-2,2),0,100);
        if(!chance(.060)) continue;
        switch(randomInt(0,6)) {
        case 0:
            state.prosperity=std::min(100,state.prosperity+6);
            addSettlementEvent(year,town.id,L"Состоялась большая ярмарка, оживившая ремесло и торговлю.");break;
        case 1:
            state.prosperity=std::min(100,state.prosperity+4);
            addSettlementEvent(year,town.id,L"Мастера открыли новые мастерские и приняли учеников.");break;
        case 2:
            state.prosperity=std::max(0,state.prosperity-7);
            addSettlementEvent(year,town.id,L"Неурожай и нехватка припасов осложнили жизнь жителей.");break;
        case 3:
            state.prosperity=std::max(0,state.prosperity-5);
            addSettlementEvent(year,town.id,L"Нападение разбойников нанесло ущерб окрестностям.");break;
        case 4:
            addSettlementEvent(year,town.id,L"Местные жители укрепили стены и сторожевые посты.");break;
        case 5:
            state.prosperity=std::min(100,state.prosperity+3);
            addSettlementEvent(year,town.id,L"В поселение прибыли переселенцы и торговые караваны.");break;
        default:
            addSettlementEvent(year,town.id,L"Собрание общины урегулировало спор между жителями.");break;
        }
    }
}
void World::simulateYear(int year) {
    updateCensus();
    simulatePolitics(year);
    simulateTerritories(year);
    simulatePopulation(year);
    simulateSociety(year);
    simulateInteractions(year);
    simulateRebellion(year);
    simulateSettlements(year);
    updateCensus();
    for(size_t n=0;n<nations_.size();++n) if(nations_[n].vanished<0 && nations_[n].founded<year && nationState_[n].territory==0) {
        nations_[n].vanished=year;
        addEvent(year,static_cast<int>(n),L"Государство утратило последние территории и прекратило существование.");
    }
}
} // namespace fantasy
