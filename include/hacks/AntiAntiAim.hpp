/*
  Created on 29.07.18.
*/

#include <optional>
#include "mathlib/vector.h"
#include "cdll_int.h"

#pragma once
class IClientEntity;

struct brutedata
{
    bool usingmanual;
    bool pitchdot;
    bool usefakepitch;
    int brutenum{ 0 };
    int missnumber{ 0 };
    int hits_in_a_row{ 0 };
    float fov{ 0.0f };
    Vector original_angle{};
    Vector new_angle{};
};

namespace hacks::anti_anti_aim
{
extern std::unordered_map<unsigned, brutedata> resolver_map;
Vector FakeAngles(CachedEntity *ent);
Vector RealAngles(CachedEntity *ent);
void increaseBruteNum(int idx);
void frameStageNotify(ClientFrameStage_t stage);
// void resolveEnt(int IDX, IClientEntity *entity = nullptr);
} // namespace hacks::anti_anti_aim
