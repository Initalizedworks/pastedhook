/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <hacks/hacklist.hpp>
#include <settings/Bool.hpp>
#include "HookedMethods.hpp"
#include "MiscTemporary.hpp"
#include "votelogger.hpp"

settings::Boolean die_if_vac{ "misc.die-if-vac", "false" };
static settings::Boolean autoabandon{ "misc.auto-abandon", "false" };
settings::Boolean random_name{ "misc.random-name", "false" };
extern settings::String force_name;
extern std::string name_forced;

namespace hooked_methods
{

static TextFile randomnames_file;

DEFINE_HOOKED_METHOD(Shutdown, void, INetChannel *this_, const char *reason)
{
    g_Settings.bInvalid = true;
    logging::Info("Disconnect: %s", reason);
    if (strstr(reason, "banned") || (strstr(reason, "Generic_Kicked") && tfmm::isMMBanned()))
    {
        if (*die_if_vac)
        {
            logging::Info("VAC/Matchmaking banned");
            *(int *) nullptr = 0;
            exit(1);
        }
    }
#if ENABLE_IPC
    ipc::UpdateServerAddress(true);
#endif
    if (autoabandon && !ignoredc)
        tfmm::disconnectAndAbandon();
    ignoredc = false;
    hacks::autojoin::onShutdown();
    std::string message = reason;
    votelogger::onShutdown(message);
    if (*random_name)
    {
        if (randomnames_file.TryLoad("names.txt"))
        {
            name_forced = randomnames_file.lines.at(rand() % randomnames_file.lines.size());
        }
    }
    else
        name_forced = "";
}

static InitRoutine init(
    []()
    {
        random_name.installChangeCallback(
            [](settings::VariableBase<bool> &, bool after)
            {
                if (after)
                {
                    if (randomnames_file.TryLoad("names.txt"))
                    {
                        name_forced = randomnames_file.lines.at(rand() % randomnames_file.lines.size());
                    }
                }
                else
                    name_forced = "";
            });
    });
} // namespace hooked_methods
