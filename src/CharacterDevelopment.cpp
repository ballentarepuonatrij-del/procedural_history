#include "Fantasy/World.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

namespace fantasy {
namespace {
using A=Ability;
using S=Skill;
using T=Personality;
template<class Enum> constexpr size_t idx(Enum e) { return static_cast<size_t>(e); }
void adjust(uint8_t& value,int delta) {
    value=static_cast<uint8_t>(std::clamp(static_cast<int>(value)+delta,0,100));
}
Skill vocation(Profession p) {
    switch(p) {
    case Profession::Farmer: return S::Survival;
    case Profession::Artisan: case Profession::Miner: return S::Craft;
    case Profession::Merchant: return S::Trade;
    case Profession::Soldier: return S::Combat;
    case Profession::Sailor: return S::Survival;
    case Profession::Scholar: return S::Lore;
    case Profession::Priest: case Profession::Noble: return S::Diplomacy;
    case Profession::Thief: return S::Stealth;
    case Profession::Healer: return S::Healing;
    }
    return S::Survival;
}
Ability vocationAbility(Profession p) {
    switch(p) {
    case Profession::Farmer: case Profession::Miner: case Profession::Soldier: return A::Endurance;
    case Profession::Artisan: case Profession::Sailor: case Profession::Thief: return A::Agility;
    case Profession::Merchant: case Profession::Priest: case Profession::Noble: return A::Charisma;
    case Profession::Scholar: case Profession::Healer: return A::Intellect;
    }
    return A::Willpower;
}
int oldAge(Species s) {
    switch(s) {
    case Species::Human: return 55;
    case Species::Elf: return 155;
    case Species::Dwarf: return 105;
    case Species::Outlaw: return 55;
    case Species::Folk: return 80;
    }
    return 55;
}
} // namespace

void World::initializeCharacter(int id,int year) {
    Person& p=people_[static_cast<size_t>(id)];
    auto& c=p.innate;
    for(auto& value:c.abilities) value=static_cast<uint8_t>(randomInt(23,64));
    for(auto& value:c.traits) value=static_cast<uint8_t>(randomInt(20,80));
    for(auto& value:c.skills) value=static_cast<uint8_t>(randomInt(0,9));
    switch(p.species) {
    case Species::Human: adjust(c.abilities[idx(A::Charisma)],5); adjust(c.traits[idx(T::Ambition)],6);break;
    case Species::Elf: adjust(c.abilities[idx(A::Agility)],8); adjust(c.traits[idx(T::Curiosity)],10);adjust(c.traits[idx(T::Empathy)],10);break;
    case Species::Dwarf: adjust(c.abilities[idx(A::Endurance)],9); adjust(c.traits[idx(T::Discipline)],8);break;
    case Species::Outlaw: adjust(c.traits[idx(T::Courage)],9);adjust(c.skills[idx(S::Stealth)],6);break;
    case Species::Folk: adjust(c.abilities[idx(A::Agility)],5);adjust(c.traits[idx(T::Empathy)],8);break;
    }
    p.current=p.innate;
    // Already adult at year zero: provide a plausible pre-history without writing
    // fictitious yearly records before the simulation's first year.
    int age=std::max(0,year-p.born);
    if(age>=16) {
        auto profession=p.stages.back().profession;
        auto& skill=p.current.skills[idx(vocation(profession))];
        adjust(skill,std::min(36,(age-13)/2+randomInt(0,7)));
        adjust(p.current.abilities[idx(vocationAbility(profession))],std::min(12,age/6));
        if(age>=30) adjust(p.current.traits[idx(T::Discipline)],std::min(8,(age-22)/8));
        addBiography(id,year,L"К началу летописи уже приобрёл/приобрела жизненный и профессиональный опыт.");
    }
}

void World::applyLifeExperience(int id, LifeExperience event) {
    if(id<0 || static_cast<size_t>(id)>=people_.size()) return;
    auto& c=people_[static_cast<size_t>(id)].current;
    auto ability=[&](A kind,int delta){adjust(c.abilities[idx(kind)],delta);};
    auto skill=[&](S kind,int delta){adjust(c.skills[idx(kind)],delta);};
    auto trait=[&](T kind,int delta){adjust(c.traits[idx(kind)],delta);};
    switch(event) {
    case LifeExperience::Guild: skill(S::Craft,2);trait(T::Discipline,2);break;
    case LifeExperience::Achievement: skill(vocation(people_[static_cast<size_t>(id)].stages.back().profession),4);trait(T::Ambition,1);break;
    case LifeExperience::Journey: skill(S::Survival,3);ability(A::Endurance,1);trait(T::Curiosity,2);break;
    case LifeExperience::Trade: skill(S::Trade,3);ability(A::Charisma,1);break;
    case LifeExperience::Dispute: trait(T::Courage,2);trait(T::Empathy,-1);break;
    case LifeExperience::Captivity: ability(A::Endurance,-3);trait(T::Empathy,-2);trait(T::Discipline,1);break;
    case LifeExperience::Freedom: trait(T::Courage,3);trait(T::Ambition,2);break;
    case LifeExperience::Friendship: trait(T::Empathy,2);skill(S::Diplomacy,1);break;
    case LifeExperience::Rivalry: trait(T::Ambition,2);trait(T::Courage,1);break;
    case LifeExperience::Mentorship: skill(vocation(people_[static_cast<size_t>(id)].stages.back().profession),3);trait(T::Discipline,2);break;
    case LifeExperience::Marriage: trait(T::Empathy,2);trait(T::Discipline,1);break;
    case LifeExperience::Crime: skill(S::Stealth,3);trait(T::Empathy,-2);break;
    case LifeExperience::Leadership: skill(S::Diplomacy,3);ability(A::Charisma,2);trait(T::Ambition,2);break;
    case LifeExperience::Rebellion: trait(T::Courage,2);skill(S::Survival,1);break;
    }
}

void World::developCharacters(int year) {
    // These changes are applied after political, professional and social events.
    // An annual snapshot is saved in World::capture, not inside this method.
    for(int id:alive_) {
        Person& p=people_[static_cast<size_t>(id)];
        const int age=year-p.born;
        auto& c=p.current;
        if(age==7) {
            adjust(c.traits[idx(T::Curiosity)],2);
            addBiography(id,year,L"Начал/начала помогать старшим и узнавать устройство окружающего мира.");
        }
        if(age==16) {
            adjust(c.traits[idx(T::Discipline)],3);
            addBiography(id,year,L"Вступил/вступила во взрослую жизнь и начал/начала самостоятельный путь.");
        }
        if(age==30) {
            adjust(c.abilities[idx(A::Willpower)],2);
            addBiography(id,year,L"Достиг/достигла зрелости; опыт стал влиять на принятые решения.");
        }
        if(age==oldAge(p.species)) {
            addBiography(id,year,L"Вступил/вступила в поздний период жизни; накопленные знания и возраст меняют способности.");
        }
        // Parents transmit skills by upbringing, not only inherited genetic potential.
        // References are stable IDs; parental death stops future personal teaching.
        if(age>=7 && age<=17 && p.parentA>=0) {
            int guardian=(aliveAt(people_[static_cast<size_t>(p.parentA)],year))?p.parentA:
                (p.parentB>=0 && aliveAt(people_[static_cast<size_t>(p.parentB)],year)?p.parentB:-1);
            if(guardian>=0 && chance(.52)) {
                const auto& parent=people_[static_cast<size_t>(guardian)];
                adjust(c.skills[idx(vocation(stageAt(parent,year).profession))],1);
                if(chance(.16)) adjust(c.traits[idx(T::Discipline)],1);
                if(age==7) addBiography(id,year,L"Получил/получила первые уроки ремесла в семье.");
            }
        }
        if(age>=7) {
            const auto& stage=p.stages.back();
            S primary=vocation(stage.profession);
            int discipline=static_cast<int>(c.traits[idx(T::Discipline)]);
            int curiosity=static_cast<int>(c.traits[idx(T::Curiosity)]);
            double chanceToLearn=(age<16?.38:.50)+discipline/300.0+curiosity/600.0;
            if(stage.status==Status::Enslaved) chanceToLearn*=.67;
            if(age>=oldAge(p.species)) chanceToLearn*=.55;
            if(chance(chanceToLearn)) adjust(c.skills[idx(primary)],randomInt(1,2));
            if(chance(.14)) adjust(c.abilities[idx(vocationAbility(stage.profession))],1);
            if(chance(.045)) adjust(c.traits[idx(T::Discipline)],1);
            // Changing careers is uncommon; prior experience and aptitudes matter.
            if(age>=20 && age<oldAge(p.species) && stage.status==Status::Free && chance(.0025)) {
                const Profession jobs[]{Profession::Artisan,Profession::Merchant,Profession::Soldier,
                    Profession::Scholar,Profession::Healer,Profession::Sailor,Profession::Miner};
                const Profession next=jobs[static_cast<size_t>(randomInt(0,6))];
                const int newSkill=c.skills[idx(vocation(next))];
                const int oldSkill=c.skills[idx(primary)];
                if(next!=stage.profession && newSkill+12>=oldSkill) {
                    changeStage(id,year,stage.nation,next,stage.status,stage.guild);
                    addBiography(id,year,L"Сменил/сменила жизненное призвание: "+std::wstring(professionName(next))+L".");
                }
            }
        }
        if(age>=16 && chance(.016)) {
            switch(randomInt(0,4)) {
            case 0: applyLifeExperience(id,LifeExperience::Journey);
                addBiography(id,year,L"Побывал/побывала в дальних землях и закалил/закалила характер.");break;
            case 1: applyLifeExperience(id,LifeExperience::Achievement);
                addBiography(id,year,L"Создал/создала выдающееся произведение или добился/добилась успеха в своём деле.");break;
            case 2: applyLifeExperience(id,LifeExperience::Dispute);
                addBiography(id,year,L"Пережил/пережила столкновение интересов, став решительнее.");break;
            case 3: adjust(c.abilities[idx(A::Endurance)],-randomInt(1,5));
                adjust(c.traits[idx(T::Courage)],1);
                addBiography(id,year,L"Перенёс/перенесла тяжёлую болезнь или ранение, ослабившие здоровье.");break;
            default: applyLifeExperience(id,LifeExperience::Mentorship);
                addBiography(id,year,L"Учился/училась у опытного наставника и повысил/повысила своё мастерство.");break;
            }
        }
        if(age>=16) {
            const auto profession=p.stages.back().profession;
            int earned=profession==Profession::Noble?7:profession==Profession::Merchant?5:
                       profession==Profession::Artisan?4:profession==Profession::Thief?3:2;
            if(p.stages.back().status==Status::Enslaved) earned=0;
            p.wealth=std::clamp(p.wealth+earned-randomInt(0,3),0,100000);
        }
        if(age>oldAge(p.species) && year%3==0) {
            // Gradual physical decline; knowledge and personality are retained.
            adjust(c.abilities[idx(A::Strength)],-1);
            adjust(c.abilities[idx(A::Agility)],-1);
            adjust(c.abilities[idx(A::Endurance)],-1);
        }
    }
}

void World::recordCharacterProgress(int year) {
    for(int id:alive_) {
        auto& p=people_[static_cast<size_t>(id)];
        if(p.development.empty() || p.development.back().year<year)
            p.development.push_back({year,p.current,p.wealth});
    }
}
} // namespace fantasy
