/*
 * City Life
 * Copyright (C) 2026 iCore
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CityLifeMgr.h"
#include "LifeBotReservation.h"

#include "Chat.h"
#include "CommandScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "WorldScript.h"

using namespace Acore::ChatCommands;

class CityLifeWorldScript : public WorldScript
{
public:
    CityLifeWorldScript()
        : WorldScript("CityLifeWorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_SHUTDOWN })
    {
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sCityLifeMgr.LoadConfig();
    }

    void OnUpdate(uint32 diff) override
    {
        sCityLifeMgr.Update(diff);
    }

    void OnShutdown() override
    {
        PlayerbotsLife::UnregisterActiveModule(PlayerbotsLife::ModuleSystemFlags::MODULE_CITY_LIFE);
    }
};

class CityLifePlayerScript : public PlayerScript
{
public:
    CityLifePlayerScript()
        : PlayerScript("CityLifePlayerScript", {
            PLAYERHOOK_ON_LOGIN,
            PLAYERHOOK_ON_LOGOUT,
            PLAYERHOOK_ON_UPDATE_ZONE,
            PLAYERHOOK_CAN_GROUP_INVITE,
            PLAYERHOOK_CAN_GROUP_ACCEPT,
            PLAYERHOOK_CAN_INIT_TRADE,
            PLAYERHOOK_CAN_SET_TRADE_ITEM,
            PLAYERHOOK_ON_BEFORE_ACHI_COMPLETE,
            PLAYERHOOK_ON_BEFORE_CRITERIA_PROGRESS
        })
    {
    }

    void OnPlayerLogin(Player* player) override
    {
        sCityLifeMgr.OnPlayerLogin(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        sCityLifeMgr.OnPlayerLogout(player);
    }

    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 /*newArea*/) override
    {
        sCityLifeMgr.OnPlayerZoneUpdate(player, newZone);
    }

    bool OnPlayerCanGroupInvite(Player* /*player*/, std::string& membername) override
    {
        if (Player* invited = ObjectAccessor::FindPlayerByName(membername))
        {
            if (invited->GetSession() && invited->GetSession()->IsBot())
            {
                if (sCityLifeMgr.IsResident(invited->GetGUID().GetCounter()))
                    sCityLifeMgr.DisengageResident(invited->GetGUID().GetCounter());
            }
        }
        return true;
    }

    bool OnPlayerCanGroupAccept(Player* player, Group* /*group*/) override
    {
        if (player && player->GetSession() && player->GetSession()->IsBot())
        {
            if (sCityLifeMgr.IsResident(player->GetGUID().GetCounter()))
                sCityLifeMgr.DisengageResident(player->GetGUID().GetCounter());
        }
        return true;
    }

    bool OnPlayerCanInitTrade(Player* player, Player* target) override
    {
        if (!sCityLifeMgr.IsInteractionsEnabled() || !sCityLifeMgr.IsPlayerTradingEnabled())
        {
            bool targetIsResident = target && sCityLifeMgr.IsResident(target->GetGUID().GetCounter());
            bool playerIsResident = player && sCityLifeMgr.IsResident(player->GetGUID().GetCounter());
            if (targetIsResident || playerIsResident)
            {
                if (targetIsResident)
                    target->Whisper("Trading is disabled.", LANG_UNIVERSAL, player);
                return false;
            }
        }
        return true;
    }

    bool OnPlayerCanSetTradeItem(Player* player, Item* /*tradedItem*/, uint8 tradeSlot) override
    {
        if (tradeSlot == TRADE_SLOT_NONTRADED)
            return true;

        if (!sCityLifeMgr.IsInteractionsEnabled() || !sCityLifeMgr.IsPlayerTradingEnabled())
        {
            if (player && sCityLifeMgr.IsResident(player->GetGUID().GetCounter()))
                return false;
        }
        return true;
    }

    bool OnPlayerBeforeAchievementComplete(Player* player, AchievementEntry const* /*achievement*/) override
    {
        if (sCityLifeMgr.IsBlockAchievementsEnabled() && player && sCityLifeMgr.IsResident(player->GetGUID().GetCounter()))
            return false;
        return true;
    }

    bool OnPlayerBeforeCriteriaProgress(Player* player, AchievementCriteriaEntry const* /*criteria*/) override
    {
        if (sCityLifeMgr.IsBlockAchievementsEnabled() && player && sCityLifeMgr.IsResident(player->GetGUID().GetCounter()))
            return false;
        return true;
    }
};

class CityLifeCommandScript : public CommandScript
{
public:
    CityLifeCommandScript() : CommandScript("CityLifeCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "citylife", HandleCityLifeCommand, SEC_GAMEMASTER, Console::Yes }
        };
        return commandTable;
    }

    static bool HandleCityLifeCommand(ChatHandler* handler, char const* args)
    {
        return sCityLifeMgr.HandleCommand(handler, args ? args : "");
    }
};

void AddCityLifeScripts()
{
    PlayerbotsLife::RegisterActiveModule(PlayerbotsLife::ModuleSystemFlags::MODULE_CITY_LIFE);
    new CityLifeWorldScript();
    new CityLifePlayerScript();
    new CityLifeCommandScript();
    LOG_INFO("server.loading", ">> Loaded mod-playerbots-city-life");
}
