#include "Names.h"
#include <algorithm>
#include <array>
#include <string_view>
namespace fantasy {
namespace {
template <size_t N> std::wstring pick(const std::array<std::wstring_view, N>& a, std::mt19937& r) {
    return std::wstring(a[std::uniform_int_distribution<size_t>(0, N-1)(r)]);
}
}
std::wstring makePersonName(Species species, std::mt19937& random) {
    static constexpr std::array<std::wstring_view,12> humanA{L"Марк",L"Луций",L"Флавия",L"Аделия",L"Тарен",L"Селена",L"Артак",L"Дарий",L"Варен",L"Элия",L"Тибер",L"Мира"};
    static constexpr std::array<std::wstring_view,10> humanB{L"Аврелий",L"Север",L"Ардакс",L"Морван",L"Фенрис",L"Валерий",L"Нарсис",L"Эвандр",L"Кассиан",L"Талас"};
    static constexpr std::array<std::wstring_view,10> elfA{L"Элиан",L"Тэлира",L"Лаэрин",L"Аэлис",L"Сильвен",L"Эрандиль",L"Лириэль",L"Ваэна",L"Илларион",L"Миривель"};
    static constexpr std::array<std::wstring_view,8> elfB{L"Листопев",L"Лунный Лист",L"Тихий Ручей",L"Туманная Ветвь",L"Свет Рощи",L"Звёздный Прах",L"Зеленолист",L"Песнь Дождя"};
    static constexpr std::array<std::wstring_view,10> dwarfA{L"Бром",L"Торгрим",L"Бальда",L"Дурин",L"Хельга",L"Краг",L"Гуннар",L"Фрейда",L"Оррик",L"Руника"};
    static constexpr std::array<std::wstring_view,8> dwarfB{L"Камнещит",L"Железнобород",L"Меднокров",L"Рудозуб",L"Громорук",L"Скалолом",L"Твердогорн",L"Кузнецвет"};
    static constexpr std::array<std::wstring_view,8> outlawA{L"Кривой Том",L"Старая Рита",L"Кровавый Фин",L"Крыс",L"Шрам",L"Тень",L"Джарек",L"Ворона"};
    static constexpr std::array<std::wstring_view,8> folkA{L"Шелест",L"Моховик",L"Бусинка",L"Капель",L"Флейта",L"Пушинка",L"Лесовичок",L"Искорка"};
    switch(species) {
        case Species::Human: return pick(humanA, random) + L" " + pick(humanB, random);
        case Species::Elf: return pick(elfA, random) + L" " + pick(elfB, random);
        case Species::Dwarf: return pick(dwarfA, random) + L" " + pick(dwarfB, random);
        case Species::Outlaw: return pick(outlawA, random);
        case Species::Folk: return pick(folkA, random);
    }
    return L"Безымянный";
}
std::wstring makeNationName(Species species, int id) {
    static constexpr std::array<std::wstring_view,10> humans{L"Республика Астерия",L"Империя Серен",L"Царство Ардак",L"Полисы Таласса",L"Доминион Морвин",L"Северные кланы",L"Коринфская лига",L"Меридийская держава",L"Гелианская республика",L"Сатрапия Варен"};
    static constexpr std::array<std::wstring_view,6> elves{L"Леса Сильвариона",L"Рощи Эларина",L"Лунный престол",L"Двор Ясных Листьев",L"Вечнозелёный союз",L"Святилище Аэрлис"};
    static constexpr std::array<std::wstring_view,6> dwarves{L"Твердыня Камнещит",L"Глубины Карг-Дума",L"Королевство Железной Кузни",L"Горные чертоги",L"Трон Семи Штолен",L"Оплот Стального Молота"};
    static constexpr std::array<std::wstring_view,6> outlaws{L"Братство Чёрного Паруса",L"Вольные бухты",L"Союз Волчьих Лагерей",L"Береговые воры",L"Кровавые корсары",L"Змеиная шайка"};
    static constexpr std::array<std::wstring_view,6> folk{L"Община Мшистых Троп",L"Тайные деревни",L"Союз Лесных Огней",L"Сокрытые полянки",L"Домики под Корнями",L"Круг Тихих Песен"};
    auto make = [id](auto& items) { return std::wstring(items[static_cast<size_t>(id) % items.size()]) + L" · " + std::to_wstring(id + 1); };
    switch(species) {
        case Species::Human: return make(humans); case Species::Elf: return make(elves); case Species::Dwarf: return make(dwarves);
        case Species::Outlaw: return make(outlaws); case Species::Folk: return make(folk);
    }
    return L"?";
}
std::wstring makeSettlementName(Species species, int id) {
    static constexpr std::array<std::wstring_view,8> human{L"Аврелия",L"Северин",L"Коринт",L"Варград",L"Мирест",L"Талис",L"Аркос",L"Дорум"};
    static constexpr std::array<std::wstring_view,8> elf{L"Лаэлис",L"Туманная Роща",L"Серебролесье",L"Эриваль",L"Нимвейн",L"Зеленодолье",L"Эйлория",L"Тихолесье"};
    static constexpr std::array<std::wstring_view,8> dwarf{L"Барад-Кор",L"Гром-Хольм",L"Карг-Дум",L"Рудная Палата",L"Каменград",L"Торн-Холл",L"Скалоброд",L"Железный Грот"};
    static constexpr std::array<std::wstring_view,6> outlaw{L"Чёрная бухта",L"Проклятый Мыс",L"Воровская Щель",L"Шакалий Холм",L"Косой Берег",L"Мёртвый Пролив"};
    static constexpr std::array<std::wstring_view,6> folk{L"Моховая Нора",L"Колокольчиков Луг",L"Дупляная Роща",L"Тихая Поляна",L"Орешковая Долина",L"Сонный Пень"};
    auto make = [id](auto& items) { return std::wstring(items[static_cast<size_t>(id) % items.size()]) + L"-" + std::to_wstring(id / static_cast<int>(items.size()) + 1); };
    switch(species) { case Species::Human: return make(human); case Species::Elf: return make(elf); case Species::Dwarf: return make(dwarf); case Species::Outlaw: return make(outlaw); case Species::Folk: return make(folk); }
    return L"?";
}
Rgb nationColor(int id, Species species) {
    static constexpr std::array<Rgb,18> colors{{{211,103,83},{70,159,151},{171,128,200},{220,172,73},{98,156,214},{202,116,159},{119,185,101},{218,131,83},{134,127,226},{80,183,176},{207,102,119},{159,171,84},{133,166,206},{205,158,123},{154,112,174},{225,165,84},{97,156,131},{190,112,98}}};
    Rgb c=colors[static_cast<size_t>(id) % colors.size()];
    if(species == Species::Folk) { c.g=static_cast<uint8_t>(std::min(230, int(c.g)+20)); }
    return c;
}
Government startingGovernment(Species species, std::mt19937& random) {
    int r=std::uniform_int_distribution<int>(0,99)(random);
    switch(species) {
        case Species::Human: return r<20?Government::Republic:r<45?Government::Monarchy:r<60?Government::Theocracy:r<80?Government::CityState:r<90?Government::Commune:Government::Clan;
        case Species::Elf: return r<13?Government::Theocracy:Government::Monarchy;
        case Species::Dwarf: return r<14?Government::Theocracy:Government::Monarchy;
        case Species::Outlaw: return Government::PirateCouncil;
        case Species::Folk: return Government::Commune;
    }
    return Government::Monarchy;
}
Profession startingProfession(Species species, std::mt19937& random) {
    int r=std::uniform_int_distribution<int>(0,99)(random);
    switch(species) {
        case Species::Human: return r<34?Profession::Farmer:r<54?Profession::Artisan:r<69?Profession::Merchant:r<82?Profession::Soldier:r<90?Profession::Priest:Profession::Scholar;
        case Species::Elf: return r<27?Profession::Farmer:r<50?Profession::Artisan:r<68?Profession::Healer:r<82?Profession::Scholar:r<92?Profession::Soldier:Profession::Priest;
        case Species::Dwarf: return r<46?Profession::Miner:r<74?Profession::Artisan:r<89?Profession::Soldier:Profession::Merchant;
        case Species::Outlaw: return r<48?Profession::Thief:r<75?Profession::Sailor:r<88?Profession::Soldier:Profession::Merchant;
        case Species::Folk: return r<39?Profession::Farmer:r<62?Profession::Artisan:r<85?Profession::Healer:Profession::Scholar;
    }
    return Profession::Farmer;
}
} // namespace fantasy
