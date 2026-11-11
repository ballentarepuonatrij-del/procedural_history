#include "Fantasy/World.h"
namespace fantasy {
const wchar_t* speciesName(Species x) {
    switch(x) { case Species::Human: return L"Люди"; case Species::Elf: return L"Эльфы"; case Species::Dwarf: return L"Дварфы"; case Species::Outlaw: return L"Бандиты и пираты"; case Species::Folk: return L"Сказочный народец"; }
    return L"Неизвестно";
}
const wchar_t* terrainName(Terrain x) {
    switch(x) { case Terrain::Sea: return L"Море"; case Terrain::Plain: return L"Равнина"; case Terrain::Forest: return L"Лес"; case Terrain::Jungle: return L"Джунгли"; case Terrain::Mountain: return L"Горы"; }
    return L"?";
}
const wchar_t* governmentName(Government x) {
    switch(x) { case Government::Republic: return L"Республика"; case Government::Monarchy: return L"Монархия"; case Government::Theocracy: return L"Теократия"; case Government::CityState: return L"Город-государство"; case Government::Commune: return L"Община"; case Government::Clan: return L"Союз кланов"; case Government::PirateCouncil: return L"Пиратский совет"; }
    return L"?";
}
const wchar_t* professionName(Profession x) {
    switch(x) { case Profession::Farmer: return L"Земледелец"; case Profession::Artisan: return L"Ремесленник"; case Profession::Merchant: return L"Купец"; case Profession::Soldier: return L"Воин"; case Profession::Miner: return L"Рудокоп"; case Profession::Sailor: return L"Мореплаватель"; case Profession::Scholar: return L"Учёный"; case Profession::Priest: return L"Жрец"; case Profession::Noble: return L"Знать"; case Profession::Thief: return L"Разбойник"; case Profession::Healer: return L"Целитель"; }
    return L"?";
}
const wchar_t* statusName(Status x) { switch(x) { case Status::Free: return L"Свободен"; case Status::Enslaved: return L"В рабстве"; case Status::Outlaw: return L"Вне закона"; } return L"?"; }
const wchar_t* settlementName(SettlementKind x) {
    switch(x) { case SettlementKind::City: return L"Город"; case SettlementKind::Village: return L"Деревня"; case SettlementKind::Castle: return L"Замок"; case SettlementKind::Grove: return L"Лесное поселение"; case SettlementKind::MountainCity: return L"Горный город"; case SettlementKind::Hideout: return L"Лагерь"; case SettlementKind::FairyVillage: return L"Скрытая община"; }
    return L"?";
}
} // namespace fantasy

namespace fantasy {
const wchar_t* abilityName(Ability v) {
    switch(v) {
    case Ability::Strength: return L"Сила"; case Ability::Agility: return L"Ловкость";
    case Ability::Intellect: return L"Интеллект"; case Ability::Charisma: return L"Харизма";
    case Ability::Endurance: return L"Выносливость"; case Ability::Willpower: return L"Сила воли";
    default: return L"?";
    }
}
const wchar_t* skillName(Skill v) {
    switch(v) {
    case Skill::Combat: return L"Военное дело"; case Skill::Craft: return L"Ремесло";
    case Skill::Trade: return L"Торговля"; case Skill::Lore: return L"Знания";
    case Skill::Diplomacy: return L"Дипломатия"; case Skill::Survival: return L"Выживание";
    case Skill::Stealth: return L"Скрытность"; case Skill::Healing: return L"Целительство";
    default: return L"?";
    }
}
const wchar_t* personalityName(Personality v) {
    switch(v) {
    case Personality::Courage: return L"Смелость"; case Personality::Curiosity: return L"Любознательность";
    case Personality::Ambition: return L"Честолюбие"; case Personality::Empathy: return L"Сочувствие";
    case Personality::Discipline: return L"Дисциплина";
    default: return L"?";
    }
}
} // namespace fantasy
