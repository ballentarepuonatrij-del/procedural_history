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
