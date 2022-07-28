/*
 * AntiDisguise.cpp
 *
 *  Created on: Nov 16, 2016
 *      Author: nullifiedcat
 */

#include <settings/Bool.hpp>
#include "common.hpp"

namespace hacks::antidisguise
{
#if ENABLE_NULL_GRAPHICS
static settings::Boolean enable{ "remove.disguise", "true" };
static settings::Boolean no_invisibility{ "remove.cloak", "true" };
#else
static settings::Boolean enable{ "remove.disguise", "false" };
static settings::Boolean no_invisibility{ "remove.cloak", "false" };
#endif

void cm()
{
    CachedEntity *ent;
    if (!*enable && !*no_invisibility)
        return;

    for (int i = 0; i <= g_IEngine->GetMaxClients(); i++)
    {
        ent = ENTITY(i);
        if (CE_BAD(ent) || ent == LOCAL_E || ent->m_Type() != ENTITY_PLAYER || CE_INT(ent, netvar.iClass) != tf_class::tf_spy)
        {
            continue;
        }
        if (*enable)
            RemoveCondition<TFCond_Disguised>(ent);
        if (*no_invisibility)
        {
            RemoveCondition<TFCond_Cloaked>(ent);
            RemoveCondition<TFCond_CloakFlicker>(ent);
        }
    }
}
static InitRoutine EC([](){ EC::Register(EC::CreateMove, cm, "antidisguise", EC::average); EC::Register(EC::CreateMoveWarp, cm, "antidisguise_w", EC::average); });
} // namespace hacks::antidisguise

namespace hacks::antitaunts
{

#if ENABLE_NULL_GRAPHICS
static settings::Boolean remove_taunts{ "remove.taunts", "true" };
#else
static settings::Boolean remove_taunts{ "remove.taunts", "false" };
#endif

void CreateMove()
{
    for (int i = 1; i < g_GlobalVars->maxClients; ++i)
    {
        auto ent = ENTITY(i);
        if (CE_BAD(ent) || ent == LOCAL_E || ent->m_Type() != ENTITY_PLAYER)
            continue;
        RemoveCondition<TFCond_Taunting>(ent);
    }
}

static void register_remove_taunts(bool enable)
{
    if (enable)
        EC::Register(EC::CreateMove, CreateMove, "cm_antitaunt");
    else
        EC::Unregister(EC::CreateMove, "cm_antitaunt");
}

static InitRoutine init([]() {
    remove_taunts.installChangeCallback([](settings::VariableBase<bool> &, bool new_val) {
        register_remove_taunts(new_val);
    });
    if (*remove_taunts)
        register_remove_taunts(true);
});

} // namespace hacks::antitaunts
