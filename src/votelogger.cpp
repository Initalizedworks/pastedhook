/*
 * votelogger.cpp
 *
 *  Created on: Dec 31, 2017
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include <boost/algorithm/string.hpp>
#include <settings/Bool.hpp>
#include "votelogger.hpp"
#include "Votekicks.hpp"
#include "PlayerTools.hpp"

static settings::Int vote_wait_min{ "votelogger.autovote.wait.min", "10" };
static settings::Int vote_wait_max{ "votelogger.autovote.wait.max", "40" };
static settings::Boolean vote_wait{ "votelogger.autovote.wait", "false" };
static settings::Boolean vote_kicky{ "votelogger.autovote.yes", "false" };
static settings::Boolean vote_kickn{ "votelogger.autovote.no", "false" };
static settings::Boolean vote_rage_vote{ "votelogger.autovote.no.rage", "false" };
static settings::Boolean chat{ "votelogger.chat", "true" };
static settings::Boolean chat_partysay{ "votelogger.chat.partysay", "false" };
static settings::Boolean chat_casts{ "votelogger.chat.casts", "false" };
static settings::Boolean requeue_on_kick{ "votelogger.requeue-on-kick", "false" };

namespace votelogger
{

static bool was_local_player { false };
static bool was_local_player_caller { false };
static Timer local_kick_timer{};
static int kicked_player;

void Reset()
{
    was_local_player        = false;
    was_local_player_caller = false;
    
}

static void vote_rage_back()
{
    static Timer attempt_vote_time;
    char cmd[40];
    player_info_s info;
    std::vector<int> targets;

    if (!g_IEngine->IsInGame() || !attempt_vote_time.test_and_set(1000))
        return;

    for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
    {
        auto ent = ENTITY(i);
        // TO DO: m_bEnemy check only when you can't vote off players from the opposite team
        if (CE_BAD(ent) || ent == LOCAL_E || ent->m_Type() != ENTITY_PLAYER || ent->m_bEnemy())
            continue;

        if (!GetPlayerInfo(ent->m_IDX, &info))
            continue;

        auto &pl = playerlist::AccessData(info.friendsID);
        if (pl.state == playerlist::k_EState::RAGE)
            targets.emplace_back(info.userID);
    }
    if (targets.empty())
        return;

    std::snprintf(cmd, sizeof(cmd), "callvote kick \"%d scamming\"", targets[UniformRandomInt(0, targets.size() - 1)]);
    g_IEngine->ClientCmd_Unrestricted(cmd);
}

void dispatchUserMessage(bf_read &buffer, int type)
{
    static player_info_s kicked_info;
    player_info_s caller_info{};
    int caller, eid, vote_id;
    char reason[64], formated_string[256];

    switch (type)
    {
    case 45:
        // Call vote failed
        break;
    case 46:
    {
        Reset();
        /* Team where vote occured */
        buffer.ReadByte();
         /* Voteid because global votes exist now */
        vote_id = buffer.ReadLong();
        /* Caller player index */
        caller = buffer.ReadByte();
        /* Vote Reason */
        buffer.ReadString(reason, 64, false, nullptr);
        /* Name of kicked player */
        buffer.ReadString(formated_string, 64, false, nullptr);
        /* Kicked player index */
        eid = (buffer.ReadByte() & 0xFF) >> 1;
        /* Restore buffer positions */
        buffer.Seek(0);

        if (!g_IEngine->GetPlayerInfo(eid, &kicked_info) || !g_IEngine->GetPlayerInfo(caller, &caller_info))
            break;

        logging::Info("Vote called to kick %s [U:1:%u] for %s by %s [U:1:%u]", kicked_info.name, kicked_info.friendsID, reason, caller_info.name, caller_info.friendsID);

        std::string reason_short;
        std::string reason_s = std::string(reason);
        if (reason_s.find("#TF_vote_kick_player_cheating") != std::string::npos)
            reason_short = "Cheating";
        else if (reason_s.find("#TF_vote_kick_player_scamming") != std::string::npos)
            reason_short = "Scamming";
        else if (reason_s.find("#TF_vote_kick_player_idle") != std::string::npos)
            reason_short = "Idle";
        else if (reason_s.find("#TF_vote_kick_player_other") != std::string::npos)
            reason_short = "Other";
        else
            reason_short = reason_s;

        if (kicked_info.friendsID == g_ISteamUser->GetSteamID().GetAccountID())
            was_local_player = true;
        if (CE_GOOD(LOCAL_E) && caller == LOCAL_E->m_IDX)
            was_local_player_caller = true;
            logging::Info("We are currently calling a votekick");
            logging::Info("We are currently being kicked ): ):");
            local_kick_timer.update();
        if (*vote_kickn || *vote_kicky)
        {
            using namespace playerlist;

            auto &pl             = AccessData(kicked_info.friendsID);
            auto &pl_caller      = AccessData(caller_info.friendsID);
            bool friendly_kicked = !player_tools::shouldTargetSteamId(kicked_info.friendsID);
            bool friendly_caller = !player_tools::shouldTargetSteamId(caller_info.friendsID);

            std::string state = "DEFAULT";

           /* Determine string */
            switch (pl.state)
            {
            case k_EState::PRIVATE
            case k_EState::IPC:
              state = "PRIVATE";
              
            case k_EState::FRIEND:
            case k_EState::PARTY
                state= "FRIEND"
                break;
            case k_EState::RAGE:
                state = "RAGE";
                break;
            }

            if (*vote_kickn && friendly_kicked)
            {
                if (*vote_wait)
                    std::snprintf(formated_string, sizeof(formated_string), format_cstr("wait %d;vote %d option2", UniformRandomInt(*vote_wait_min, *vote_wait_max), vote_id).get());
                else
                    std::snprintf(formated_string, sizeof(formated_string), format_cstr("vote %d option2", vote_id).get());
                g_IEngine->ClientCmd_Unrestricted(formated_string);

                if (*vote_rage_vote && !friendly_caller)
                {
                    pl_caller.state = k_EState::RAGE;
                logging::Info("Voting F2 because votekick target is %s playerlist state. A counter kick will automatically be called when we can vote.", state);
                }
            }
            else if (*vote_kicky && !friendly_kicked)
            {
                if (*vote_wait)
                    std::snprintf(formated_string, sizeof(formated_string), format_cstr("wait %d;vote %d option1", UniformRandomInt(*vote_wait_min, *vote_wait_max), vote_id).get());
                else
                    std::snprintf(formated_string, sizeof(formated_string), format_cstr("vote %d option1", vote_id).get());

                g_IEngine->ClientCmd_Unrestricted(formated_string);
                
            }
        }
        if (*chat_partysay)
        {
            std::snprintf(formated_string, sizeof(formated_string), "Kick called: %s [U:1:%u] -> %s [U:1:%u] (%s)", caller_info.name, caller_info.friendsID, kicked_info.name, kicked_info.friendsID, reason_short.c_str());
            re::CTFPartyClient::GTFPartyClient()->SendPartyChat(formated_string);
        }
        break;
    }
    case 47:
        logging::Info("Vote passed on %s [U:1:%u] with %i F1s and %i F2s.", kicked_info.name, kicked_info.friendsID, F1count + 1, F2count + 1);
        if (*chat_partysay_result)
        {
            std::snprintf(formated_string, sizeof(formated_string), "Vote passed on %s [U:1:%u] with %i F1s and %i F2s.", kicked_info.name, kicked_info.friendsID, F1count + 1, F2count + 1);
            re::CTFPartyClient::GTFPartyClient()->SendPartyChat(formated_string);
        }
        if (was_local_player_caller)
        {
            if (kicked_info.friendsID)
                hacks::votekicks::previously_kicked.emplace(kicked_info.friendsID);
            if (leave_after_local_vote)
                tfmm::abandon();
        }
        Reset();
        break;
    case 48:
     logging::Info("Vote Failed on %s [U:1:%u]", kicked_info.name, kicked_info.friendsID);
       if (was_local_player && requeue_on_kick)
            tfmm::leaveQueue();
      Reset();
        break;
    default:
        break;
    }
}

static void setup_vote_rage()
{
    EC::Register(EC::CreateMove, vote_rage_back, "vote_rage_back");
}

static void reset_vote_rage()
{
    EC::Unregister(EC::CreateMove, "vote_rage_back");
}

class VoteEventListener : public IGameEventListener
{
public:
    void FireGameEvent(KeyValues *event) override
    {
        if (!*chat_casts || (!*chat_partysay && !chat))
            return;
        const char *name = event->GetName();
        if (!strcmp(name, "vote_cast"))
        {
            bool vote_option = event->GetInt("vote_option");
            if (vote_option)
                return;
            int eid = event->GetInt("entityid");
            player_info_s info{};
           if (!GetPlayerInfo(eid, &info))
                return;
            if (chat_partysay)
            {
                char formated_string[256];
               std::snprintf(formated_string, sizeof(formated_string), "[CAT] %s [U:1:%u] %s", info.name, info.friendsID, vote_option ? "F2" : "F1");
            }
                
        }
    }
};

static VoteEventListener listener{};
static InitRoutine init(
    []()
    {
        if (*vote_rage_vote)
            setup_vote_rage();
           setup_paint_abandon();
            [](settings::VariableBase<bool> &var, bool new_val)
            {
                if (new_val)
                    setup_vote_rage();
                else
                    reset_vote_rage();
            });
        g_IGameEventManager->AddListener(&listener, false);
        EC::Register(
            EC::Shutdown, []() { g_IGameEventManager->RemoveListener(&listener); }, "event_shutdown_vote");
    });
} // namespace votelogger
