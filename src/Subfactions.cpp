#include "Fantasy/World.h"
#include "Names.h"
#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace fantasy {
const wchar_t* subfactionKindName(SubfactionKind kind) {
    switch(kind) {
    case SubfactionKind::Pirates: return L"Пиратская фракция";
    case SubfactionKind::Bandits: return L"Бандитская группировка";
    case SubfactionKind::HiddenFolk: return L"Скрытая община сказочного народца";
    }
    return L"Подфракция";
}

int World::subfactionOf(int personId,int year) const {
    // Membership has its own timeline and does not replace citizenship.
    for(auto it=subfactionMemberships_.rbegin();it!=subfactionMemberships_.rend();++it)
        if(it->person==personId && it->year<=year) return it->faction;
    return -1;
}

std::vector<int> World::subfactionsInProvince(const Snapshot& snapshot,int provinceId) const {
    std::vector<int> ids;
    for(const auto& presence:snapshot.subfactionPresences)
        if(presence.province==provinceId && presence.influence>0) ids.push_back(presence.faction);
    return ids;
}

bool World::adjacentProvinces(int first,int second) const {
    if(first<0||second<0||static_cast<size_t>(first)>=provinceNeighbors_.size()) return false;
    const auto& neighbors=provinceNeighbors_[static_cast<size_t>(first)];
    return std::find(neighbors.begin(),neighbors.end(),second)!=neighbors.end();
}

void World::recordSubfactionEvent(int year,int faction,int province,SubfactionAction action,
                                 const std::wstring& message,int person,int otherFaction) {
    int nation=-1;
    if(province>=0 && static_cast<size_t>(province)<provinces_.size())
        nation=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(province)].cells.front())];
    subfactionEvents_.push_back({year,faction,province,person,otherFaction,nation,action,message});
    if(person>=0) addBiography(person,year,message);
    // Local histories record actual interference in the province, independently of borders.
    if(province>=0 && action!=SubfactionAction::Recruitment && action!=SubfactionAction::Founded) {
        for(const auto& town:settlements_) if(town.founded<=year &&
            provinceAt(town.position.x,town.position.y)==province) {
            addSettlementEvent(year,town.id,subfactions_[static_cast<size_t>(faction)].name+L": "+message);
            break;
        }
    }
}

void World::joinSubfaction(int person,int faction,int year) {
    if(person<0||faction<0 || subfactionOf(person,year)>=0) return;
    subfactionMemberships_.push_back({year,person,faction});
    const auto& group=subfactions_[static_cast<size_t>(faction)];
    const auto old=people_[static_cast<size_t>(person)].stages.back();
    if(group.kind!=SubfactionKind::HiddenFolk) {
        changeStage(person,year,old.nation,
                    group.kind==SubfactionKind::Pirates?Profession::Sailor:Profession::Thief,
                    Status::Outlaw,-1);
        applyLifeExperience(person,LifeExperience::Crime);
    } else {
        applyLifeExperience(person,LifeExperience::Friendship);
    }
    addBiography(person,year,L"Вступил/вступила в подфракцию «"+group.name+L"» без смены государственной принадлежности.");
}

void World::seedSubfactions() {
    if(provinces_.empty()||nations_.empty()) return;
    // Build once: adjacency is fixed geography even as national borders change.
    provinceNeighbors_.assign(provinces_.size(),{});
    for(int y=0;y<settings_.height;++y)for(int x=0;x<settings_.width;++x) {
        const int a=provinceAt(x,y);
        if(a<0)continue;
        for(const Point offset:{Point{1,0},Point{0,1}}) {
            int yy=y+offset.y;
            if(yy>=settings_.height)continue;
            int b=provinceAt(wrapX(x+offset.x),yy);
            if(b<0||a==b)continue;
            for(const auto& [f,t]:{std::pair{a,b},std::pair{b,a}}) {
                auto& neighbors=provinceNeighbors_[static_cast<size_t>(f)];
                if(std::find(neighbors.begin(),neighbors.end(),t)==neighbors.end())neighbors.push_back(t);
            }
        }
    }
    constexpr SubfactionKind types[]{SubfactionKind::Pirates,SubfactionKind::Bandits,SubfactionKind::HiddenFolk};
    const int groupsPerKind=std::clamp(settings_.civilizations/5,1,4);
    for(SubfactionKind kind:types) for(int serial=0;serial<groupsPerKind;++serial) {
        std::vector<int> candidates;
        for(const auto& province:provinces_) {
            const int host=owners_[static_cast<size_t>(province.cells.front())];
            if(host<0)continue;
            bool suitable=false;
            if(kind==SubfactionKind::Pirates) {
                for(int cell:province.cells) if(isCoastal(cell%settings_.width,cell/settings_.width)) {suitable=true;break;}
            } else if(kind==SubfactionKind::HiddenFolk)
                suitable=province.terrain==Terrain::Forest||province.terrain==Terrain::Jungle;
            else suitable=province.terrain!=Terrain::Ice;
            if(!suitable)continue;
            // Prefer international borders. Criminal activity can then cross state lines.
            int copies=1;
            for(const auto& neighbor:provinces_) if(adjacentProvinces(province.id,neighbor.id)) {
                int other=owners_[static_cast<size_t>(neighbor.cells.front())];
                if(other>=0 && other!=host) {copies=5;break;}
            }
            while(copies-->0)candidates.push_back(province.id);
        }
        if(candidates.empty()) for(const auto& province:provinces_)
            if(owners_[static_cast<size_t>(province.cells.front())]>=0) candidates.push_back(province.id);
        if(candidates.empty())return;
        int base=candidates[static_cast<size_t>(randomInt(0,static_cast<int>(candidates.size())-1))];
        int id=static_cast<int>(subfactions_.size());
        std::wstring name;
        if(kind==SubfactionKind::Pirates) name=L"Вольные паруса «"+provinces_[static_cast<size_t>(base)].name+L"»";
        else if(kind==SubfactionKind::Bandits) name=L"Теневая братва «"+provinces_[static_cast<size_t>(base)].name+L"»";
        else name=L"Тихий круг «"+provinces_[static_cast<size_t>(base)].name+L"»";
        name+=L" №"+std::to_wstring(id+1);
        subfactions_.push_back({id,0,base,kind,std::move(name)});
        subfactionState_.push_back({-1,randomInt(30,90),randomInt(20,55),randomInt(45,90)});
        subfactionPresences_.push_back({id,base,randomInt(35,75)});
        recordSubfactionEvent(0,id,base,SubfactionAction::Founded,L"Подфракция возникла в провинции «"+
            provinces_[static_cast<size_t>(base)].name+L"». Власть над провинцией не изменилась.");
        const int owner=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(base)].cells.front())];
        for(int i=0;i<3;++i) {
            // Members are individuals with a sovereign country in PersonStage;
            // affiliation is an additional independent relationship.
            int person=addPerson(owner,0,false);
            if(kind==SubfactionKind::HiddenFolk) {
                auto& p=people_[static_cast<size_t>(person)];
                p.species=Species::Folk;
                p.name=makePersonName(Species::Folk,rng_);
                initializeCharacter(person,0);
                if(p.family>=0)
                    families_[static_cast<size_t>(p.family)].name=L"Дом «"+p.name+L"» (#"+std::to_wstring(p.id)+L")";
            }
            joinSubfaction(person,id,0);
            if(i==0) subfactionState_.back().leader=person;
        }
        // Establish a cross-border foothold where one is available at founding.
        for(const auto& neighbor:provinces_) {
            const int other=owners_[static_cast<size_t>(neighbor.cells.front())];
            if(other>=0 && other!=owner && adjacentProvinces(base,neighbor.id) &&
               (kind!=SubfactionKind::HiddenFolk || neighbor.terrain==Terrain::Forest || neighbor.terrain==Terrain::Jungle)) {
                subfactionPresences_.push_back({id,neighbor.id,randomInt(15,32)});
                recordSubfactionEvent(0,id,neighbor.id,SubfactionAction::Expansion,
                    L"Открыта вторая тайная база за границей государства, в провинции «"+neighbor.name+L"».");
                break;
            }
        }
    }
}

void World::simulateSubfactions(int year) {
    for(const auto& group:subfactions_) {
        auto& state=subfactionState_[static_cast<size_t>(group.id)];
        state.wealth=std::clamp(state.wealth+randomInt(-3,4),0,5000);
        state.strength=std::clamp(state.strength+randomInt(-1,2),1,100);
        state.secrecy=std::clamp(state.secrecy+randomInt(-2,2),1,100);
        std::vector<int> members;
        for(const auto& membership:subfactionMemberships_) if(membership.faction==group.id &&
            aliveAt(people_[static_cast<size_t>(membership.person)],year)) members.push_back(membership.person);
        if(state.leader<0 || !aliveAt(people_[static_cast<size_t>(state.leader)],year)) {
            state.leader=members.empty()?-1:members.front();
            if(state.leader>=0) recordSubfactionEvent(year,group.id,group.homeProvince,SubfactionAction::Recruitment,
                L"Новым руководителем стал/стала «"+people_[static_cast<size_t>(state.leader)].name+L"».",state.leader);
        }
        // Recruit an actual inhabitant of a country where the group operates.
        if((members.size()<3 || chance(.075)) && members.size()<28) {
            std::vector<int> hosts;
            for(const auto& presence:subfactionPresences_)if(presence.faction==group.id) {
                int country=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(presence.province)].cells.front())];
                if(country>=0)hosts.push_back(country);
            }
            if(!hosts.empty()) {
                int host=hosts[static_cast<size_t>(randomInt(0,static_cast<int>(hosts.size())-1))];
                std::vector<int> eligible;
                for(int id:alive_) if(people_[static_cast<size_t>(id)].born<=year-18 &&
                    people_[static_cast<size_t>(id)].stages.back().nation==host && subfactionOf(id,year)<0 &&
                    people_[static_cast<size_t>(id)].stages.back().status!=Status::Enslaved &&
                    (group.kind!=SubfactionKind::HiddenFolk ||
                     people_[static_cast<size_t>(id)].species==Species::Folk || members.size()<2))
                    eligible.push_back(id);
                if(!eligible.empty()) {
                    int recruit=eligible[static_cast<size_t>(randomInt(0,static_cast<int>(eligible.size())-1))];
                    joinSubfaction(recruit,group.id,year);
                    recordSubfactionEvent(year,group.id,group.homeProvince,SubfactionAction::Recruitment,
                        L"Принят новый участник: «"+people_[static_cast<size_t>(recruit)].name+L"».");
                    members.push_back(recruit);
                }
            }
        }
        if(state.leader<0 && !members.empty()) state.leader=members.front();
        // A faction's footprint is separate from the sovereign owner of each province.
        if(chance(.13) && subfactionPresences_.size()<500) {
            std::vector<int> neighbors;
            for(const auto& presence:subfactionPresences_) if(presence.faction==group.id) {
                for(const auto& province:provinces_) {
                    if(!adjacentProvinces(presence.province,province.id)) continue;
                    bool present=false;
                    for(const auto& other:subfactionPresences_)if(other.faction==group.id && other.province==province.id)
                        {present=true;break;}
                    if(present || province.terrain==Terrain::Ice)continue;
                    if(group.kind==SubfactionKind::HiddenFolk && province.terrain!=Terrain::Forest &&
                       province.terrain!=Terrain::Jungle)continue;
                    int weight=1;
                    int oldOwner=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(presence.province)].cells.front())];
                    int newOwner=owners_[static_cast<size_t>(province.cells.front())];
                    if(newOwner>=0 && oldOwner!=newOwner)weight+=3;
                    while(weight-->0)neighbors.push_back(province.id);
                }
            }
            if(!neighbors.empty()) {
                int province=neighbors[static_cast<size_t>(randomInt(0,static_cast<int>(neighbors.size())-1))];
                subfactionPresences_.push_back({group.id,province,randomInt(8,22)});
                recordSubfactionEvent(year,group.id,province,SubfactionAction::Expansion,
                    L"Фракция распространила сеть на провинцию «"+provinces_[static_cast<size_t>(province)].name+L"»." );
            }
        }
        for(auto& presence:subfactionPresences_)if(presence.faction==group.id) {
            presence.influence=std::clamp(presence.influence+randomInt(-2,3),4,100);
        }
        if(members.empty())continue;
        const int actor=members[static_cast<size_t>(randomInt(0,static_cast<int>(members.size())-1))];
        if(group.kind==SubfactionKind::HiddenFolk) {
            if(chance(.24)) {
                std::vector<int> outposts;
                for(const auto& p:subfactionPresences_)if(p.faction==group.id)outposts.push_back(p.province);
                int province=outposts[static_cast<size_t>(randomInt(0,static_cast<int>(outposts.size())-1))];
                const int host=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(province)].cells.front())];
                if(host>=0 && chance(.48)) {
                    nationState_[static_cast<size_t>(host)].stability=std::min(100,nationState_[static_cast<size_t>(host)].stability+1);
                    state.wealth+=3;
                    applyLifeExperience(actor,LifeExperience::Trade);
                    recordSubfactionEvent(year,group.id,province,SubfactionAction::Trade,
                        L"Незаметный обмен лекарственными травами и изделиями с жителями «"+
                        nations_[static_cast<size_t>(host)].name+L"».",actor);
                    if(chance(.5)) addEvent(year,host,L"Местные жители обменялись товарами с тайной общиной «"+
                        group.name+L"».",actor);
                } else {
                    state.secrecy=std::min(100,state.secrecy+2);
                    applyLifeExperience(actor,LifeExperience::Journey);
                    recordSubfactionEvent(year,group.id,province,SubfactionAction::Refuge,
                        L"Община укрыла своих жителей и переместила тайные тропы.",actor);
                }
            }
        } else {
            // Genuine cross-border trade uses TWO NEIGHBORING provinces with different owners.
            bool traded=false;
            if(chance(.35)) for(const auto& from:subfactionPresences_) if(from.faction==group.id && !traded)
                for(const auto& to:subfactionPresences_)if(to.faction==group.id && !traded &&
                    adjacentProvinces(from.province,to.province)) {
                    const int first=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(from.province)].cells.front())];
                    const int second=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(to.province)].cells.front())];
                    if(first<0 || second<0 || first==second)continue;
                    traded=true;
                    const int profit=randomInt(5,18);
                    state.wealth=std::min(5000,state.wealth+profit);
                    nationState_[static_cast<size_t>(first)].treasury=std::max(0,nationState_[static_cast<size_t>(first)].treasury-profit/3);
                    nationState_[static_cast<size_t>(second)].treasury=std::max(0,nationState_[static_cast<size_t>(second)].treasury-profit/4);
                    std::wstring event=L"Контрабанда из «"+provinces_[static_cast<size_t>(from.province)].name+
                        L"» ("+nations_[static_cast<size_t>(first)].name+L") в «"+
                        provinces_[static_cast<size_t>(to.province)].name+L"» ("+
                        nations_[static_cast<size_t>(second)].name+L"); прибыль "+std::to_wstring(profit)+L" монет.";
                    applyLifeExperience(actor,LifeExperience::Crime);
                    recordSubfactionEvent(year,group.id,to.province,SubfactionAction::Smuggling,event,actor);
                    addEvent(year,first,L"Через границу действует контрабандная сеть «"+group.name+L"».",actor);
                    addEvent(year,second,L"В провинцию проникает контрабанда группировки «"+group.name+L"».",actor);
                    // Named participant in the destination country; the border affects real biographies.
                    std::vector<int> contacts;
                    for(int person:alive_)if(person!=actor &&
                        people_[static_cast<size_t>(person)].stages.back().nation==second &&
                        people_[static_cast<size_t>(person)].born<=year-18 &&
                        people_[static_cast<size_t>(person)].stages.back().status!=Status::Enslaved)contacts.push_back(person);
                    int buyer=-1;
                    if(!contacts.empty()) {
                        buyer=contacts[static_cast<size_t>(randomInt(0,static_cast<int>(contacts.size())-1))];
                        interactions_.push_back({year,actor,buyer,InteractionKind::Crime});
                        addBiography(buyer,year,L"Участвовал/участвовала в контрабандной сделке с «"+
                            people_[static_cast<size_t>(actor)].name+L"» ("+group.name+L").");
                        applyLifeExperience(buyer,LifeExperience::Trade);
                    }
                    smugglingRoutes_.push_back({year,group.id,from.province,to.province,
                                                first,second,profit,actor,buyer});
                }
            if(!traded && chance(group.kind==SubfactionKind::Pirates?.20:.15)) {
                std::vector<int> outposts;
                for(const auto& p:subfactionPresences_)if(p.faction==group.id)outposts.push_back(p.province);
                int province=outposts[static_cast<size_t>(randomInt(0,static_cast<int>(outposts.size())-1))];
                const int owner=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(province)].cells.front())];
                int loot=randomInt(2,10);
                state.wealth+=loot;
                applyLifeExperience(actor,LifeExperience::Crime);
                recordSubfactionEvent(year,group.id,province,SubfactionAction::Raid,
                    L"Набег на торговцев и караваны; добыча "+std::to_wstring(loot)+L" монет.",actor);
                if(owner>=0) {
                    nationState_[static_cast<size_t>(owner)].stability=std::max(5,nationState_[static_cast<size_t>(owner)].stability-1);
                    addEvent(year,owner,L"Налёт подфракции «"+group.name+L"» на торговцев.",actor);
                }
            }
        }
        // Territorial police action, without any transfer of province ownership.
        if(chance(.09)) {
            std::vector<int> owned;
            for(const auto& p:subfactionPresences_)if(p.faction==group.id)owned.push_back(p.province);
            int province=owned[static_cast<size_t>(randomInt(0,static_cast<int>(owned.size())-1))];
            int host=owners_[static_cast<size_t>(provinces_[static_cast<size_t>(province)].cells.front())];
            if(host>=0) {
                state.secrecy=std::max(1,state.secrecy-4);
                for(auto& p:subfactionPresences_)if(p.faction==group.id && p.province==province)
                    p.influence=std::max(4,p.influence-7);
                recordSubfactionEvent(year,group.id,province,SubfactionAction::Crackdown,
                    L"Стража государства «"+nations_[static_cast<size_t>(host)].name+L"» обнаружила следы фракции.");
                if(chance(.5))addEvent(year,host,L"Стража провела облаву на «"+group.name+L"».");
            }
        }
        // Neighboring subfactions can cooperate or compete even when they are of different types.
        if(chance(.11)) for(const auto& other:subfactions_)if(other.id>group.id) {
            int shared=-1;
            for(const auto& a:subfactionPresences_)if(a.faction==group.id)
                for(const auto& b:subfactionPresences_)if(b.faction==other.id && b.province==a.province) {shared=a.province;break;}
            if(shared<0)continue;
            if(chance(.55)) {
                state.wealth=std::min(5000,state.wealth+3);
                subfactionState_[static_cast<size_t>(other.id)].wealth=std::min(5000,subfactionState_[static_cast<size_t>(other.id)].wealth+2);
                recordSubfactionEvent(year,group.id,shared,SubfactionAction::Trade,
                    L"Сделка или обмен сведениями с подфракцией «"+other.name+L"».",actor,other.id);
                recordSubfactionEvent(year,other.id,shared,SubfactionAction::Trade,
                    L"Обмен сведениями с подфракцией «"+group.name+L"».",-1,group.id);
            } else {
                state.strength=std::max(1,state.strength-1);
                subfactionState_[static_cast<size_t>(other.id)].strength=std::max(1,subfactionState_[static_cast<size_t>(other.id)].strength-1);
                recordSubfactionEvent(year,group.id,shared,SubfactionAction::Clash,
                    L"Конфликт из-за тайных путей с подфракцией «"+other.name+L"».",actor,other.id);
                recordSubfactionEvent(year,other.id,shared,SubfactionAction::Clash,
                    L"Стычка с подфракцией «"+group.name+L"».",-1,group.id);
            }
            break;
        }
    }
}
} // namespace fantasy
