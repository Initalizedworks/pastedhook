/*
 * Created on 29.07.18.
 */

// 02.12.2021
// Credits: (Censored due to privacy, this is not my code.)
// AntiAntiAim.cpp

#include <hacks/ESP.hpp>
#include "common.hpp"
#include "hacks/AntiAntiAim.hpp"
#include "sdk/dt_recv_redef.h"

namespace hacks::anti_anti_aim
{
static settings::Boolean enable_auto_resolver{ "anti-anti-aim.enable", "false" };
#if !ENABLE_TEXTMODE
static settings::Boolean enable_manual_resolver{ "anti-anti-aim.manual.resolver-enable", "false" };
static settings::Button manual_resolver_yaw{ "anti-anti-aim.manual.resolver.yaw-button", "" };
static settings::Int manual_amount_yaw{ "anti-anti-aim.manual.resolver.yaw-amount", "90" };
static settings::Float manual_resolver_fov{ "anti-anti-aim.manual.resolver.fov", "25.0f" };
#endif
static settings::Int miss_to_resolve{ "anti-anti-aim.bruteforce-amount", "1" };

std::unordered_map<unsigned, brutedata> resolver_map;
std::array<CachedEntity *, 32> sniperdot_array;
bool IsKeyPressed, changeangles;

// Update new resolver data every frame
static inline void modifyAngles()
{
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        auto player = ENTITY(i);
        if (CE_BAD(player))
            continue;
        if (!player->m_bAlivePlayer())
            continue;
        if (!player->m_bEnemy())
            continue;
        if (!player->player_info.friendsID) // Rip first bot on itemtest
            continue;

        auto &data  = resolver_map[player->player_info.friendsID];
        auto &angle = CE_VECTOR(player, netvar.m_angEyeAngles);

        if (angle.y != data.new_angle.y && !changeangles)
            data.original_angle.y = angle.y;
        if (angle.x != data.new_angle.x && !changeangles)
            data.original_angle.x = angle.x;

        while (data.original_angle.y > 180)
            data.original_angle.y -= 360;
        while (data.original_angle.y < -180)
            data.original_angle.y += 360;

        if ((data.missnumber > (int) miss_to_resolve) && (enable_auto_resolver))
        {
            while (data.new_angle.y > 180)
                data.new_angle.y -= 360;
            while (data.new_angle.y < -180)
                data.new_angle.y += 360;

            angle.y = data.new_angle.y;

            // If gamer doesn't use fake pitch, there is no point in resolving it
            if (data.usefakepitch)
                angle.x = data.new_angle.x;

            changeangles  = false;
            data.pitchdot = false;
        }
#if !ENABLE_TEXTMODE
        if ((data.usingmanual) && enable_manual_resolver)
        {
            while (data.new_angle.y > 180)
                data.new_angle.y -= 360;
            while (data.new_angle.y < -180)
                data.new_angle.y += 360;

            angle.y = data.new_angle.y;

            if (data.usefakepitch)
                angle.x = data.new_angle.x;

            changeangles  = false;
            data.pitchdot = false;
        }
#endif
    }
}

#if !ENABLE_TEXTMODE
void frameStageNotify(ClientFrameStage_t stage)
{
    if (!g_IEngine->IsInGame())
        return;
    if (stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START)
        modifyAngles();
}
#endif

static std::array<float, 5> yaw_resolves{ -89.0f, 89.0f, -115.0f, 240.0f, 155.0f };

static float resolveAngleYaw(brutedata &data)
{
    changeangles = true;
    float newangle;
    int entry = RandomInt(0, 4);
    newangle  = yaw_resolves[entry];
    data.new_angle.y += newangle;
    return 0;
}

// Store pitch possible angles to resolve(we store 2 ups and 1 down cuz using down as real is bad)
static std::array<float, 3> pitch_resolves{ -89.0f, 89.0f, -89.0f };

static float resolveAnglePitch(brutedata &data, CachedEntity *ent)
{
    if (data.pitchdot)
        return 0;
    int entry        = (int) std::floor(data.brutenum / 1) % pitch_resolves.size();
    data.new_angle.x = pitch_resolves[entry];
    return 0;
}

#if !ENABLE_TEXTMODE
float ManualResolverYaw(brutedata &data, float angle)
{
    changeangles     = true;
    data.usingmanual = true;
    data.missnumber  = 0;
    data.new_angle.y += angle;
}

void ManualResolver()
{
    if (!enable_manual_resolver)
        return;

    CachedEntity *target = NULL;
    float oldfov = (float) manual_resolver_fov;

    for (int i = 1; i < g_IEngine->GetMaxClients(); i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_BAD(ent))
            continue;
        if (!ent->m_bEnemy())
            continue;
        if (!ent->m_bAlivePlayer())
            continue;
        float fov = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, ent->hitboxes.GetHitbox(spine_2)->center);

        if (fov < (float) manual_resolver_fov)
        {
            if (fov < oldfov)
            {
                target = ent;
            }
            oldfov = fov;
        }
    }

    if (target == NULL)
        return;

    auto &data = resolver_map[target->player_info.friendsID];

    if (!IsKeyPressed)
    {
        if (manual_resolver_yaw.isKeyDown())
        {
            IsKeyPressed = true;
            ManualResolverYaw(data, + *manual_amount_yaw);
        }
    if (!manual_resolver_yaw.isKeyDown())
        IsKeyPressed = false;
    }
}
#endif
// Increase brute num to set other angle instead of same one
void increaseBruteNum(int idx)
{
    auto ent = ENTITY(idx);
    if (CE_BAD(ent) || !ent->player_info.friendsID)
        return;
    auto &data = resolver_map[ent->player_info.friendsID];

    data.brutenum++;
    data.missnumber++;
    data.hits_in_a_row = 0;
#if !ENABLE_TEXTMODE
    // Dont resolve, if we use manual resolver;
    if (data.missnumber >= (int) miss_to_resolve)
    {
        data.usingmanual = false;
        resolveAnglePitch(data, ent);
        resolveAngleYaw(data);
    }
#endif
}

void UseFakePitch()
{
#if !ENABLE_TEXTMODE
    if (!enable_manual_resolver && !enable_auto_resolver)
        return;
#else
    if (!enable_auto_resolver)
        return;
#endif
    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        CachedEntity *player = ENTITY(i);
        if (CE_BAD(player))
            continue;
        if (!player->m_bAlivePlayer())
            continue;
        if (!player->m_bEnemy())
            continue;
        if (!player->player_info.friendsID)
            continue;

        auto &data = resolver_map[player->player_info.friendsID];

        // Gamer use fake pitch
        if (data.original_angle.x < -89.5f || data.original_angle.x > 89.5f)
            data.usefakepitch = true;
        else
            data.usefakepitch = false;
    }
}

// Dot pitch resolver that works in real time, wow
void CreateMove()
{
    sniperdot_array.fill(0);

    for (int i = g_IEngine->GetMaxClients(); i <= HIGHEST_ENTITY; i++)
    {
        CachedEntity *sniper_dot = ENTITY(i);
        if (CE_BAD(sniper_dot) || sniper_dot->m_iClassID() != CL_CLASS(CSniperDot))
            continue;
        auto owner_idx = HandleToIDX(CE_INT(sniper_dot, netvar.m_hOwnerEntity));
        if (IDX_BAD(owner_idx) || owner_idx > sniperdot_array.size() || owner_idx <= 0)
            continue;
        sniperdot_array.at(owner_idx) = sniper_dot;
    }

    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        CachedEntity *player = ENTITY(i);
        if (CE_BAD(player))
            continue;
        if (!player->m_bAlivePlayer())
            continue;
        if (!player->m_bEnemy())
            continue;
        if (g_pPlayerResource->GetClass(player) != tf_sniper)
            continue;

        CachedEntity *weapon_id  = ENTITY(HandleToIDX(CE_INT(player, netvar.hActiveWeapon)));
        CachedEntity *sniper_dot = nullptr;
        auto &data               = resolver_map[player->player_info.friendsID];
        data.pitchdot            = false;

        if (weapon_id->m_iClassID() == CL_CLASS(CTFSniperRifle) || weapon_id->m_iClassID() == CL_CLASS(CTFSniperRifleDecap) || weapon_id->m_iClassID() == CL_CLASS(CTFSniperRifleClassic))
        {
            sniper_dot = sniperdot_array.at(player->m_IDX);
        }

        // Sniper dot found, use it.
        if (sniper_dot != nullptr)
        {
            // Get End and start point
            auto dot_origin = sniper_dot->m_vecOrigin();
            auto eye_origin = re::C_BasePlayer::GetEyePosition(RAW_ENT(player));
            // Get Angle from eye to dot
            Vector diff = dot_origin - eye_origin;
            Vector angles;
            VectorAngles(diff, angles);
            // Use the pitch (yaw is not useable because sadly the sniper dot does not represent it with real yaw)
            data.new_angle.x = angles.x;
            data.pitchdot    = true;
        }
    }
}

// Resolved Angles
Vector RealAngles(CachedEntity *ent)
{
    auto &data = resolver_map[ent->player_info.friendsID];
    return data.new_angle;
}

// Fake(Real by view) Angles
Vector FakeAngles(CachedEntity *ent)
{
    auto &data = resolver_map[ent->player_info.friendsID];
    return data.original_angle;
}

static void shutdown()
{
    resolver_map.clear();
}

static InitRoutine init(
    []()
    {
        EC::Register(EC::Shutdown, shutdown, "antiantiaim_shutdown");
        EC::Register(EC::CreateMove, CreateMove, "cm_antiantiaim");
        EC::Register(EC::CreateMove, UseFakePitch, "cm_usefakeptch");
#if !ENABLE_TEXTMODE
        EC::Register(EC::CreateMove, ManualResolver, "cm_manualresolver");
#endif
        EC::Register(EC::CreateMoveWarp, CreateMove, "cm_antiantiaim");
#if ENABLE_TEXTMODE
        EC::Register(EC::CreateMove, modifyAngles, "cm_textmodeantiantiaim");
        EC::Register(EC::CreateMoveWarp, modifyAngles, "cmw_textmodeantiantiaim");
#endif
    });
} // namespace hacks::anti_anti_aim
