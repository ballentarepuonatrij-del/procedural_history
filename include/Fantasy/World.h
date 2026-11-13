#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace fantasy {

enum class Species { Human, Elf, Dwarf, Outlaw, Folk };
enum class Terrain { Sea, Plain, Forest, Jungle, Mountain };
enum class Government { Republic, Monarchy, Theocracy, CityState, Commune, Clan, PirateCouncil };
enum class Profession { Farmer, Artisan, Merchant, Soldier, Miner, Sailor, Scholar, Priest, Noble, Thief, Healer };
enum class Status { Free, Enslaved, Outlaw };
enum class SettlementKind { City, Village, Castle, Grove, MountainCity, Hideout, FairyVillage };

struct Rgb { uint8_t r{}, g{}, b{}; };
struct Point { int x{}, y{}; };
struct Tile { Terrain terrain{Terrain::Sea}; uint8_t height{}; };
struct Settings { int civilizations{10}; int years{200}; uint32_t seed{42}; int width{88}; int height{52}; };
struct Nation {
    int id{};
    std::wstring name;
    Species culture{};
    Government startingGovernment{};
    Rgb color{};
    Point capital{};
    int founded{};
    int vanished{-1};
};
struct NationYear {
    Government government{Government::Monarchy};
    int population{}, territory{}, treasury{150}, stability{70}, military{30};
    int settlements{}, ruler{-1};
};
struct SettlementYear {
    int owner{-1};
    int knownResidents{};  // named living people born here and affiliated with the current owner
    int prosperity{50};
};
struct Snapshot {
    int year{};
    std::vector<int16_t> owner; // -1 = ничейная территория / море
    std::vector<NationYear> nationStates;
    std::vector<SettlementYear> settlementStates;
};
struct PersonStage {
    int year{}, nation{-1}, guild{-1};
    Profession profession{Profession::Farmer};
    Status status{Status::Free};
};
// Values are clamped to 0..100. Enums are stable indices in these arrays.
enum class Ability : size_t { Strength, Agility, Intellect, Charisma, Endurance, Willpower, Count };
enum class Skill : size_t { Combat, Craft, Trade, Lore, Diplomacy, Survival, Stealth, Healing, Count };
enum class Personality : size_t { Courage, Curiosity, Ambition, Empathy, Discipline, Count };
struct CharacterStats {
    std::array<uint8_t, static_cast<size_t>(Ability::Count)> abilities{};
    std::array<uint8_t, static_cast<size_t>(Skill::Count)> skills{};
    std::array<uint8_t, static_cast<size_t>(Personality::Count)> traits{};
    bool operator==(const CharacterStats&) const = default;
};
struct CharacterYear { int year{}; CharacterStats stats{}; int wealth{}; };
struct Family {
    int id{}, founder{}, founded{};
    std::wstring name;        // Ancestral house: each person belongs to one birth lineage
};
struct Marriage { int year{}, first{}, second{}; }; // Historical union, may end by death
struct Inheritance { int year{}, from{}, to{}, wealth{}; };

enum class LifeExperience { Guild, Achievement, Journey, Trade, Dispute, Captivity, Freedom,
                            Friendship, Rivalry, Mentorship, Marriage, Crime, Leadership, Rebellion };
struct PersonNote { int year{}; std::wstring text; };
struct Person {
    int id{};
    std::wstring name;
    Species species{Species::Human};
    int born{}, died{std::numeric_limits<int>::max()};
    int homeSettlement{-1};
    int parentA{-1}, parentB{-1}; // Recorded biological parents (IDs, not names)
    int family{-1};              // Birth house is stable even after marriage
    std::vector<int> children;   // IDs in chronological order
    int wealth{};                // Transferable estate, not national treasury
    std::vector<PersonStage> stages;
    CharacterStats innate{};             // Starting potential, retained for comparison
    CharacterStats current{};            // Updated during simulation
    std::vector<CharacterYear> development; // Year-by-year history; unaffected by future years
    std::vector<PersonNote> biography;
};
struct Settlement {
    int id{};
    std::wstring name;
    Point position{};
    SettlementKind kind{SettlementKind::Village};
    int founded{};
    int foundingNation{-1};
};
struct SettlementEvent {
    int year{}, settlement{-1};
    std::wstring text;
};
struct Guild {
    int id{}, nation{}, founded{};
    std::wstring name;
    std::wstring purpose;
};
enum class InteractionKind { Partnership, Friendship, Rivalry, Mentorship, Marriage, Crime };
struct Interaction {
    int year{}, first{}, second{};
    InteractionKind kind{InteractionKind::Friendship};
};
struct Event {
    int year{}, nation{-1}, person{-1};
    std::wstring text;
};

const wchar_t* speciesName(Species value);
const wchar_t* terrainName(Terrain value);
const wchar_t* governmentName(Government value);
const wchar_t* professionName(Profession value);
const wchar_t* statusName(Status value);
const wchar_t* settlementName(SettlementKind value);
const wchar_t* abilityName(Ability value);
const wchar_t* skillName(Skill value);
const wchar_t* personalityName(Personality value);

class World;

// Контракт модуля: можно расширять мир без изменения Win32-оболочки.
class IWorldModule {
public:
    virtual ~IWorldModule() = default;
    virtual void onWorldCreated(World&) {}
    virtual void onYear(World&, int year) = 0;
};

class World {
public:
    void registerModule(std::unique_ptr<IWorldModule> module);
    void publishEvent(int year, int nation, const std::wstring& text);
    bool alterNation(int nation, int treasuryDelta, int stabilityDelta);
    bool claimTile(int nation, int x, int y, int year);
    bool addPersonNote(int personId, int year, const std::wstring& text);
    // Extension hook: modules can let a living person learn from an event.
    bool grantExperience(int personId, int year, LifeExperience experience);
    bool publishSettlementEvent(int year, int settlementId, const std::wstring& text);
    const std::vector<int16_t>& liveOwners() const { return owners_; }
    const std::vector<NationYear>& liveStates() const { return nationState_; }
    void generate(const Settings& settings);
    const Settings& settings() const { return settings_; }
    const std::vector<Tile>& tiles() const { return tiles_; }
    const std::vector<Nation>& nations() const { return nations_; }
    const std::vector<Person>& people() const { return people_; }
    const std::vector<Settlement>& settlements() const { return settlements_; }
    const std::vector<SettlementEvent>& settlementEvents() const { return settlementEvents_; }
    const std::vector<Guild>& guilds() const { return guilds_; }
    const std::vector<Event>& events() const { return events_; }
    const std::vector<Interaction>& interactions() const { return interactions_; }
    const std::vector<Family>& families() const { return families_; }
    const std::vector<Marriage>& marriages() const { return marriages_; }
    const std::vector<Inheritance>& inheritances() const { return inheritances_; }
    int spouseAt(const Person& person, int year) const;
    int wealthAt(const Person& person, int year) const;
    bool isDescendantOf(int person, int ancestor) const;
    const Snapshot& at(int year) const;
    const PersonStage& stageAt(const Person& person, int year) const;
    const CharacterStats& statsAt(const Person& person, int year) const;
    bool aliveAt(const Person& person, int year) const;
    int nationAt(const Person& person, int year) const;
    int index(int x, int y) const { return y * settings_.width + x; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < settings_.width && y < settings_.height; }
    bool isCoastal(int x, int y) const;

private:
    Settings settings_{};
    std::mt19937 rng_{};
    std::vector<Tile> tiles_;
    std::vector<int16_t> owners_;
    std::vector<Nation> nations_;
    std::vector<NationYear> nationState_;
    std::vector<Person> people_;
    std::vector<int> alive_;
    std::vector<Settlement> settlements_;
    std::vector<SettlementYear> settlementState_;
    std::vector<SettlementEvent> settlementEvents_;
    std::vector<Guild> guilds_;
    std::vector<Event> events_;
    std::vector<Interaction> interactions_;
    std::vector<Family> families_;
    std::vector<Marriage> marriages_;
    std::vector<Inheritance> inheritances_;
    std::vector<int> currentSpouses_; // Live-year index; historical lookup uses marriages_
    std::vector<Snapshot> timeline_;
    std::vector<std::unique_ptr<IWorldModule>> modules_;
    int nextSettlementName_{};

    int randomInt(int min, int max);
    bool chance(double probability);
    void createTerrain();
    void initialNations();
    int createNation(Species species, Point capital, int year);
    Point chooseCapital(Species species);
    void startPeople(int nation, int number, int year, bool newborn);
    int addPerson(int nation, int year, bool newborn);
    int addChild(int nation, int year, int parentA, int parentB);
    void formMarriages(int year);
    void seedFoundingFamilies();
    bool marry(int first, int second, int year);
    void settleInheritance(int personId, int year);
    void inheritCharacter(int child, int first, int second);
    void addGuild(int nation, int year);
    void addSettlement(int nation, int year, Point position, bool capital);
    void addEvent(int year, int nation, const std::wstring& text, int person = -1);
    void addSettlementEvent(int year, int settlement, const std::wstring& text);
    void addBiography(int personId, int year, const std::wstring& text);
    void changeStage(int personId, int year, int nation, Profession profession, Status status, int guild);
    void initializeCharacter(int personId, int year);
    void developCharacters(int year);
    void applyLifeExperience(int personId, LifeExperience experience);
    void recordCharacterProgress(int year);
    void simulateYear(int year);
    void simulatePolitics(int year);
    void simulateTerritories(int year);
    void simulatePopulation(int year);
    void simulateSociety(int year);
    void simulateInteractions(int year);
    void simulateRebellion(int year);
    void simulateSettlements(int year);
    void updateCensus();
    void capture(int year);
    int choosePerson(int nation, int minAge, int year);
    int chooseFrontierTile(int nation, bool unclaimedOnly);
    int findSettlement(int nation, int year) const;
    int findRuler(int nation,int year,int predecessor=-1);
};

} // namespace fantasy
