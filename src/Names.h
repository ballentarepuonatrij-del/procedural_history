#pragma once
#include "Fantasy/World.h"
#include <random>
#include <string>
namespace fantasy {
std::wstring makePersonName(Species species, std::mt19937& random);
std::wstring makeNationName(Species species, int unique);
std::wstring makeSettlementName(Species species, int unique);
Rgb nationColor(int unique, Species species);
Government startingGovernment(Species species, std::mt19937& random);
Profession startingProfession(Species species, std::mt19937& random);
} // namespace fantasy
