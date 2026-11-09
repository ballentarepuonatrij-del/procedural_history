#pragma once
#include "Fantasy/World.h"

// Пример подключаемой подсистемы. Для включения вызовите:
// world.registerModule(std::make_unique<MagicPortentsModule>());
// перед world.generate(settings).
class MagicPortentsModule final : public fantasy::IWorldModule {
public:
    void onYear(fantasy::World& world, int year) override {
        if(year%25!=0 || world.nations().empty()) return;
        int nation=year/25%static_cast<int>(world.nations().size());
        if(world.nations()[static_cast<size_t>(nation)].founded>year) return;
        world.alterNation(nation,+30,+5);
        world.publishEvent(year,nation,L"Небесное знамение вдохновило жителей и укрепило казну.");
    }
};
