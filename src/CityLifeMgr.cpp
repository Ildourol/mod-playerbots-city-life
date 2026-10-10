/*
 * City Life
 * Copyright (C) 2026 iCore
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CityLifeMgr.h"
#include "CityLifeActivities.h"
#include "CityLifeDialogues.h"
#include "CityLifeMovement.h"
#include "LifeBotReservation.h"

#include "AreaDefines.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iterator>
#include <limits>
#include <shared_mutex>
#include <sstream>

namespace
{
    constexpr float TwoPi = 6.28318530718f;
    constexpr uint32 ReservationSeconds = 20 * MINUTE;
    constexpr uint32 ReservationRefreshSeconds = 10 * MINUTE;

    std::string Lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::vector<std::string> Tokenize(std::string const& text)
    {
        std::istringstream stream(text);
        std::vector<std::string> out;
        std::string token;
        while (stream >> token)
            out.push_back(token);
        return out;
    }

    uint32 GameTimeSeconds()
    {
        return static_cast<uint32>(GameTime::GetGameTime().count());
    }

    uint32 GameTimeMS()
    {
        return static_cast<uint32>(GameTime::GetGameTimeMS().count());
    }

    char const* TeamName(CityLife::TeamSide team)
    {
        switch (team)
        {
            case CityLife::TeamSide::Alliance:
                return "Alliance";
            case CityLife::TeamSide::Horde:
                return "Horde";
            default:
                return "Any";
        }
    }

    bool ParseTeam(std::string token, CityLife::TeamSide& out)
    {
        token = Lower(token);
        if (token == "a" || token == "ally" || token == "alliance")
        {
            out = CityLife::TeamSide::Alliance;
            return true;
        }
        if (token == "h" || token == "horde")
        {
            out = CityLife::TeamSide::Horde;
            return true;
        }
        if (token == "any" || token == "both" || token == "neutral")
        {
            out = CityLife::TeamSide::Any;
            return true;
        }
        return false;
    }

    bool ParseUInt(std::string const& text, uint32& out)
    {
        try
        {
            size_t used = 0;
            unsigned long value = std::stoul(text, &used);
            if (used != text.size() || value > std::numeric_limits<uint32>::max())
                return false;
            out = static_cast<uint32>(value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ParseFloat(std::string const& text, float& out)
    {
        try
        {
            size_t used = 0;
            out = std::stof(text, &used);
            return used == text.size();
        }
        catch (...)
        {
            return false;
        }
    }

    CityLife::CityPoiType CategoryToPoiType(std::string const& category)
    {
        std::string cat = Lower(category);
        if (cat == "inn")
            return CityLife::CityPoiType::Inn;
        if (cat == "auction" || cat == "ah")
            return CityLife::CityPoiType::AuctionHouse;
        if (cat == "mailbox")
            return CityLife::CityPoiType::Mailbox;
        if (cat == "tavern")
            return CityLife::CityPoiType::Tavern;
        if (cat == "trainer")
            return CityLife::CityPoiType::Trainer;
        if (cat == "dock" || cat == "harbor" || cat == "fish")
            return CityLife::CityPoiType::Dock;
        if (cat == "wall" || cat == "patrol")
            return CityLife::CityPoiType::WallPatrol;
        if (cat == "vendor")
            return CityLife::CityPoiType::Vendor;
        if (cat == "duel" || cat == "ring")
            return CityLife::CityPoiType::DuelRing;
        if (cat == "bed")
            return CityLife::CityPoiType::InnBed;
        if (cat == "bank")
            return CityLife::CityPoiType::Bank;
        return CityLife::CityPoiType::Plaza;
    }
}

namespace CityLife
{
    Manager& Manager::Instance()
    {
        static Manager instance;
        return instance;
    }

    void Manager::LoadConfig()
    {
        bool wasEnabled = _enable;
        bool enabled = sConfigMgr->GetOption<bool>("CityLife.Enable", true);
        if (wasEnabled && !enabled && !_residents.empty())
            ReleaseAll(true);

        _enable = enabled;
        _debug = sConfigMgr->GetOption<bool>("CityLife.Debug", false);
        _startupDelaySeconds = sConfigMgr->GetOption<uint32>("CityLife.StartupDelaySeconds", 90);
        _tickSeconds = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("CityLife.TickSeconds", 10), 5, 60);

        // Population Budget & Demand
        std::string budgetModeStr = Lower(sConfigMgr->GetOption<std::string>("CityLife.Population.BudgetMode", "percentage"));
        _budgetMode = (budgetModeStr == "count") ? BudgetMode::Count : BudgetMode::Percentage;
        _fixedCount = sConfigMgr->GetOption<uint32>("CityLife.Population.FixedCount", 100);
        _borrowPercent = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("CityLife.Population.BorrowPercent", 10), 1, 100);
        _maxTotalPopulation = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("CityLife.Population.MaxTotal", 120), 1, 2000);

        std::string demandModeStr = Lower(sConfigMgr->GetOption<std::string>("CityLife.Population.DemandMode", "hybrid"));
        if (demandModeStr == "fixed")
            _demandMode = DemandMode::Fixed;
        else if (demandModeStr == "dynamic")
            _demandMode = DemandMode::Dynamic;
        else
            _demandMode = DemandMode::Hybrid;

        std::string distStr = Lower(sConfigMgr->GetOption<std::string>("CityLife.Population.Distribution", "scaled"));
        _distributionMode = (distStr == "legacy") ? DistributionMode::Legacy : DistributionMode::Scaled;
        _baselinePercent = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("CityLife.Population.BaselinePercent", 20), 0, 100);
        _burstGraceSeconds = sConfigMgr->GetOption<uint32>("CityLife.Population.BurstGraceSeconds", 180);
        _maxChangesPerTick = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("CityLife.Population.MaxChangesPerTick", 3), 1, 50);

        // Compatibility & Anti-Stealing Barrier
        _compatPvPLifeHarmony = sConfigMgr->GetOption<bool>("CityLife.Compatibility.PvPLife.EnableHarmony", true);
        _compatPvPLifeRespectReservations = sConfigMgr->GetOption<bool>("CityLife.Compatibility.PvPLife.RespectReservations", true);

        // Candidate Filters & Safety
        _usePlayerbotConfig = sConfigMgr->GetOption<bool>("CityLife.Bots.UsePlayerbotConfig", true);
        _botAccountPrefix = sConfigMgr->GetOption<std::string>("CityLife.Bots.AccountPrefix", "auto");
        _botAccountMin = sConfigMgr->GetOption<uint32>("CityLife.Bots.AccountMin", 0);
        _botAccountMax = sConfigMgr->GetOption<uint32>("CityLife.Bots.AccountMax", 0);
        _botQueryLimit = std::max<uint32>(50, sConfigMgr->GetOption<uint32>("CityLife.Bots.QueryLimit", 500));
        _skipGroupedBots = sConfigMgr->GetOption<bool>("CityLife.Bots.SkipGrouped", true);
        _respectPlayerbotActivity = sConfigMgr->GetOption<bool>("CityLife.Bots.RespectPlayerbotActivity", false);
        _returnBots = sConfigMgr->GetOption<bool>("CityLife.Bots.ReturnWhenReleased", true);

        // Time Scaling
        _timeScaling = sConfigMgr->GetOption<bool>("CityLife.Time.Enable", true);
        _morningMultiplier = std::min<uint32>(200, sConfigMgr->GetOption<uint32>("CityLife.Time.MorningMultiplierPct", 60));
        _dayMultiplier = std::min<uint32>(200, sConfigMgr->GetOption<uint32>("CityLife.Time.DayMultiplierPct", 80));
        _eveningMultiplier = std::min<uint32>(200, sConfigMgr->GetOption<uint32>("CityLife.Time.EveningMultiplierPct", 100));
        _nightMultiplier = std::min<uint32>(200, sConfigMgr->GetOption<uint32>("CityLife.Time.NightMultiplierPct", 25));

        // Relocation & Movement
        _roaming = sConfigMgr->GetOption<bool>("CityLife.Behaviour.Relocate", true);
        _relocateMinSeconds = std::max<uint32>(30, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.RelocateMinSeconds", 180));
        _relocateMaxSeconds = std::max(_relocateMinSeconds, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.RelocateMaxSeconds", 600));
        _leashRadius = std::max<uint32>(10, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.LeashRadius", 35));
        _emotes = sConfigMgr->GetOption<bool>("CityLife.Behaviour.Emotes", true);
        _emoteChance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.EmoteChancePct", 35));
        _emoteMinSeconds = std::max<uint32>(30, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.EmoteMinSeconds", 120));
        _emoteMaxSeconds = std::max(_emoteMinSeconds, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.EmoteMaxSeconds", 360));
        _serviceSpotWeightMultiplier = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("CityLife.Behaviour.ServiceSpotWeightMultiplierPct", 250), 100, 1000);
        _staticServiceChance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.StaticServiceChancePct", 80));

        _staticServiceHubs.clear();
        std::string staticHubs = sConfigMgr->GetOption<std::string>("CityLife.Behaviour.StaticServiceHubs", "Stormwind,Orgrimmar");
        std::replace(staticHubs.begin(), staticHubs.end(), ',', ' ');
        std::replace(staticHubs.begin(), staticHubs.end(), ';', ' ');
        for (std::string const& hub : Tokenize(staticHubs))
            _staticServiceHubs.insert(Lower(hub));

        _staticServiceCategories.clear();
        std::string staticCategories = sConfigMgr->GetOption<std::string>("CityLife.Behaviour.StaticServiceCategories", "auction,bank");
        std::replace(staticCategories.begin(), staticCategories.end(), ',', ' ');
        std::replace(staticCategories.begin(), staticCategories.end(), ';', ' ');
        for (std::string const& category : Tokenize(staticCategories))
            _staticServiceCategories.insert(Lower(category));

        _nonCombatStrategies = sConfigMgr->GetOption<std::string>("CityLife.Strategies.NonCombat",
            "+stay,-follow,-passive,-grind,-rpg,-travel,-duel,-pvp");

        // Activity Switches
        _actLeaveInn = sConfigMgr->GetOption<bool>("CityLife.Activities.LeaveInn.Enable", true);
        _actReturnToInn = sConfigMgr->GetOption<bool>("CityLife.Activities.ReturnToInn.Enable", true);
        _actRoaming = sConfigMgr->GetOption<bool>("CityLife.Activities.Roaming.Enable", true);
        _actWallWalking = sConfigMgr->GetOption<bool>("CityLife.Activities.WallWalking.Enable", true);
        _actTavernSitting = sConfigMgr->GetOption<bool>("CityLife.Activities.TavernSitting.Enable", true);
        _actTavernDrinking = sConfigMgr->GetOption<bool>("CityLife.Activities.TavernDrinking.Enable", true);
        _actRandomEmotes = sConfigMgr->GetOption<bool>("CityLife.Activities.RandomEmotes.Enable", true);
        _actAuctionHouse = sConfigMgr->GetOption<bool>("CityLife.Activities.AuctionHouse.Enable", true);
        _actMailbox = sConfigMgr->GetOption<bool>("CityLife.Activities.Mailbox.Enable", true);
        _actTrainer = sConfigMgr->GetOption<bool>("CityLife.Activities.Trainer.Enable", true);
        _actFoodVendor = sConfigMgr->GetOption<bool>("CityLife.Activities.FoodVendor.Enable", true);
        _actDuelWatching = sConfigMgr->GetOption<bool>("CityLife.Activities.DuelWatching.Enable", true);
        _actFishing = sConfigMgr->GetOption<bool>("CityLife.Activities.Fishing.Enable", true);
        _actDuels = sConfigMgr->GetOption<bool>("CityLife.Activities.Duels.Enable", true);
        _actInnDancing = sConfigMgr->GetOption<bool>("CityLife.Activities.InnDancing.Enable", true);
        _actPlazaSocial = sConfigMgr->GetOption<bool>("CityLife.Activities.PlazaSocial.Enable", true);
        _actCrowdScenes = sConfigMgr->GetOption<bool>("CityLife.Activities.CrowdScenes.Enable", true);
        _actMountedTravel = sConfigMgr->GetOption<bool>("CityLife.Activities.MountedTravel.Enable", true);

        // Living Crowd Scenes
        _crowdEnable = sConfigMgr->GetOption<bool>("CityLife.Crowd.Enable", false);
        _crowdPlayerRadius = sConfigMgr->GetOption<float>("CityLife.Crowd.PlayerRadius", 60.0f);
        _crowdChatCooldownSec = sConfigMgr->GetOption<uint32>("CityLife.Crowd.ChatCooldownSec", 90);
        _crowdAfkMinSec = sConfigMgr->GetOption<uint32>("CityLife.Crowd.AfkMinSec", 60);
        _crowdAfkMaxSec = sConfigMgr->GetOption<uint32>("CityLife.Crowd.AfkMaxSec", 180);

        // Duels Settings
        _duelCooldownSec = sConfigMgr->GetOption<uint32>("CityLife.Duels.CooldownSec", 45);
        _duelAllowPlayerChallenge = sConfigMgr->GetOption<bool>("CityLife.Duels.AllowPlayerChallenge", true);
        _duelMaxDistance = sConfigMgr->GetOption<float>("CityLife.Duels.MaxDuelDistance", 40.0f);

        // Movement & Sink Protection
        _mountBetweenPoiChance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("CityLife.Movement.MountBetweenPoiChance", 90));
        _sinkForensics = sConfigMgr->GetOption<bool>("CityLife.Movement.SinkForensics", true);
        _maxPathRefuseStreak = sConfigMgr->GetOption<uint32>("CityLife.Movement.MaxPathRefuseStreak", 3);

        // Chat & Interactions (Default OFF for silent out-of-the-box operation)
        _chatScriptedConversations = sConfigMgr->GetOption<bool>("CityLife.Chat.ScriptedConversations", false);
        _chatGreetings = sConfigMgr->GetOption<bool>("CityLife.Chat.Greetings", false);
        _chatUnrestricted = sConfigMgr->GetOption<bool>("CityLife.Chat.Unrestricted", false);
        _chatRandomSayChance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("CityLife.Chat.RandomSayChance", 0));
        _chatSayCooldownSec = sConfigMgr->GetOption<uint32>("CityLife.Chat.SayCooldownSec", 30);
        _enableInteractions = sConfigMgr->GetOption<bool>("CityLife.Interactions.Enable", false);
        _enablePlayerTrading = sConfigMgr->GetOption<bool>("CityLife.Interactions.PlayerTrading", false);
        _enableBuyFood = sConfigMgr->GetOption<bool>("CityLife.Interactions.BuyFood", false);
        _blockAchievements = sConfigMgr->GetOption<bool>("CityLife.Achievements.BlockAchievements", true);

        // Advanced Micro-Behaviors (Ported from mod-city-bots)
        _alternatingDistricts = sConfigMgr->GetOption<bool>("CityLife.AlternatingDistricts.Enable", true);
        _duelMeetingPoint = sConfigMgr->GetOption<bool>("CityLife.Duels.MeetingPoint.Enable", true);
        _duelTacticalRange = sConfigMgr->GetOption<bool>("CityLife.Duels.TacticalRange.Enable", true);
        _waterAvoidance = sConfigMgr->GetOption<bool>("CityLife.Movement.WaterAvoidance.Enable", true);

        ImportPlayerbotDefaults();
        CheckCoexistence();

        _botAccountCache.clear();
        _startupElapsedMs = 0;
        _timerMs = 0;

        _databaseReady = VerifyDatabase();
        if (_enable && _databaseReady)
            _databaseReady = LoadData();
        if (_enable && !_databaseReady)
            _enable = false;

        LOG_INFO("module", "[CityLife] Initialized: enable={} DB={} hubs={} residents={} budgetMode={} demandMode={}",
            _enable ? 1 : 0, _databaseReady ? "ready" : "missing", _hubs.size(), _residents.size(),
            budgetModeStr, demandModeStr);
    }

    void Manager::ImportPlayerbotDefaults()
    {
        if (!_usePlayerbotConfig)
            return;

        std::string configuredPrefix = sConfigMgr->GetOption<std::string>(
            "AiPlayerbot.RandomBotAccountPrefix", "rndbot", false);
        uint32 loginDelay = sConfigMgr->GetOption<uint32>("AiPlayerbot.DisabledWithoutRealPlayerLoginDelay", 30, false);
        std::string prefix = Lower(_botAccountPrefix);
        if (prefix.empty() || prefix == "auto" || prefix == "playerbot" || prefix == "playerbots")
            _botAccountPrefix = configuredPrefix;
        _startupDelaySeconds = std::max(_startupDelaySeconds, loginDelay);
    }

    bool Manager::CheckCoexistence()
    {
        bool cbActive = sConfigMgr->GetOption<bool>("CitizenBots.Enable", false, false);
        if (cbActive)
        {
            LOG_WARN("module", "[CityLife] CONFLICT: mod-city-bots appears enabled (CitizenBots.Enable=1). "
                "Disable mod-city-bots to prevent duplicate pathing commands and bot conflicts.");
        }
        return !cbActive;
    }

    bool Manager::VerifyDatabase()
    {
        if (!WorldDatabase.Query("SHOW TABLES LIKE 'city_life_hub'"))
        {
            LOG_ERROR("module", "[CityLife] Missing WORLD table city_life_hub. "
                "Apply data/sql/manual/world_city_life.sql");
            return false;
        }
        if (!WorldDatabase.Query("SHOW TABLES LIKE 'city_life_spot'"))
        {
            LOG_ERROR("module", "[CityLife] Missing WORLD table city_life_spot. "
                "Apply data/sql/manual/world_city_life.sql");
            return false;
        }
        return true;
    }

    bool Manager::LoadData()
    {
        std::vector<Hub> hubs;
        QueryResult hubResult = WorldDatabase.Query(
            "SELECT id,name,enabled,team,min_level,max_level,map_id,default_population,priority "
            "FROM city_life_hub ORDER BY priority DESC,id ASC");
        if (hubResult)
        {
            do
            {
                Field* fields = hubResult->Fetch();
                Hub hub;
                hub.Id = fields[0].Get<uint32>();
                hub.Name = fields[1].Get<std::string>();
                hub.Enabled = fields[2].Get<uint8>() != 0;
                hub.Team = static_cast<TeamSide>(fields[3].Get<uint8>());
                hub.MinLevel = fields[4].Get<uint8>();
                hub.MaxLevel = fields[5].Get<uint8>();
                hub.MapId = fields[6].Get<uint32>();
                hub.DefaultPopulation = fields[7].Get<uint32>();
                hub.Priority = fields[8].Get<uint32>();

                // Known Zone mapping
                std::string lowerName = Lower(hub.Name);
                if (lowerName == "stormwind" || lowerName == "stormwindgate")
                    hub.ZoneId = 1519;
                else if (lowerName == "orgrimmar" || lowerName == "orgrimmargate")
                    hub.ZoneId = 1637;
                else if (lowerName == "ironforge")
                    hub.ZoneId = 1537;
                else if (lowerName == "undercity")
                    hub.ZoneId = 1497;
                else if (lowerName == "darnassus")
                    hub.ZoneId = 1657;
                else if (lowerName == "thunderbluff")
                    hub.ZoneId = 1638;
                else if (lowerName == "exodar")
                    hub.ZoneId = 3557;
                else if (lowerName == "silvermoon")
                    hub.ZoneId = 3487;
                else if (lowerName == "shattrath")
                    hub.ZoneId = 3703;
                else if (lowerName == "dalaran")
                    hub.ZoneId = 4395;
                else if (lowerName.find("goldshire") != std::string::npos)
                    hub.ZoneId = 12;

                hub.DuelHub = (lowerName.find("gate") != std::string::npos || lowerName.find("duel") != std::string::npos);
                hub.SocialHub = (lowerName == "goldshire");

                std::string configPrefix = "CityLife.Hub." + hub.Name;
                hub.ConfiguredPopulation = sConfigMgr->GetOption<uint32>(
                    configPrefix + ".Population", hub.DefaultPopulation, false);
                bool configEnabled = sConfigMgr->GetOption<bool>(
                    configPrefix + ".Enable", true, false);
                hub.Enabled = hub.Enabled && configEnabled;
                hub.UseTimeScaling = sConfigMgr->GetOption<bool>(configPrefix + ".TimeScaling", true, false);
                hub.ResidenceSeconds = sConfigMgr->GetOption<uint32>(
                    configPrefix + ".ResidenceSeconds", 0, false);
                hub.MinCount = sConfigMgr->GetOption<uint32>(
                    configPrefix + ".Min", 0, false);

                hubs.push_back(hub);
            } while (hubResult->NextRow());
        }

        // Check if extended columns exist in city_life_spot
        bool hasExtendedColumns = false;
        if (WorldDatabase.Query("SHOW COLUMNS FROM `city_life_spot` LIKE 'poi_type'"))
            hasExtendedColumns = true;

        std::string spotQuery = hasExtendedColumns
            ? "SELECT id,hub_id,name,category,x,y,z,o,radius,weight,poi_type,district,partner_spot_id FROM city_life_spot WHERE enabled=1 ORDER BY hub_id,id"
            : "SELECT id,hub_id,name,category,x,y,z,o,radius,weight FROM city_life_spot WHERE enabled=1 ORDER BY hub_id,id";

        QueryResult spotResult = WorldDatabase.Query(spotQuery);
        if (spotResult)
        {
            do
            {
                Field* fields = spotResult->Fetch();
                Spot spot;
                spot.Id = fields[0].Get<uint32>();
                spot.HubId = fields[1].Get<uint32>();
                spot.Name = fields[2].Get<std::string>();
                spot.Category = fields[3].Get<std::string>();
                spot.X = fields[4].Get<float>();
                spot.Y = fields[5].Get<float>();
                spot.Z = fields[6].Get<float>();
                spot.O = fields[7].Get<float>();
                spot.Radius = std::max(0.0f, fields[8].Get<float>());
                spot.Weight = std::max<uint32>(1, fields[9].Get<uint32>());

                if (hasExtendedColumns)
                {
                    spot.PoiType = static_cast<CityPoiType>(fields[10].Get<uint8>());
                    spot.District = fields[11].Get<std::string>();
                    spot.PartnerSpotId = fields[12].Get<uint32>();
                }
                else
                {
                    spot.PoiType = CategoryToPoiType(spot.Category);
                    spot.District = spot.Category;
                    spot.PartnerSpotId = 0;
                }

                auto hub = std::find_if(hubs.begin(), hubs.end(),
                    [&](Hub const& candidate) { return candidate.Id == spot.HubId; });
                if (hub != hubs.end())
                    hub->Spots.push_back(spot);
            } while (spotResult->NextRow());
        }

        _hubs.swap(hubs);
        LOG_INFO("module", "[CityLife] Loaded {} hubs. {} active residents.", _hubs.size(), _residents.size());
        return !_hubs.empty();
    }

    TeamSide Manager::TeamForRace(uint8 race) const
    {
        switch (race)
        {
            case RACE_HUMAN:
            case RACE_DWARF:
            case RACE_NIGHTELF:
            case RACE_GNOME:
            case RACE_DRAENEI:
                return TeamSide::Alliance;
            case RACE_ORC:
            case RACE_UNDEAD_PLAYER:
            case RACE_TAUREN:
            case RACE_TROLL:
            case RACE_BLOODELF:
                return TeamSide::Horde;
            default:
                return TeamSide::Any;
        }
    }

    Player* Manager::FindPlayer(uint32 guidLow) const
    {
        ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(guidLow);
        if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
            return player;
        return ObjectAccessor::FindPlayer(guid);
    }

    bool Manager::IsConfiguredBotAccount(uint32 accountId)
    {
        if (!accountId)
            return false;
        auto cached = _botAccountCache.find(accountId);
        if (cached != _botAccountCache.end())
            return cached->second;

        bool rangeOk = _botAccountMax > 0 && _botAccountMax >= _botAccountMin &&
            accountId >= _botAccountMin && accountId <= _botAccountMax;
        bool prefixOk = false;
        if (!_botAccountPrefix.empty())
        {
            QueryResult result = LoginDatabase.Query("SELECT username FROM account WHERE id={} LIMIT 1", accountId);
            if (result)
            {
                std::string username = Lower(result->Fetch()[0].Get<std::string>());
                prefixOk = username.rfind(Lower(_botAccountPrefix), 0) == 0;
            }
        }

        bool accepted = rangeOk || prefixOk;
        _botAccountCache[accountId] = accepted;
        return accepted;
    }

    bool Manager::IsCandidate(Player* player)
    {
        if (!player || !player->GetSession() || !player->IsInWorld() || player->IsBeingTeleported())
            return false;
        if (!sRandomPlayerbotMgr.IsRandomBot(player) &&
            !IsConfiguredBotAccount(player->GetSession()->GetAccountId()))
            return false;
        if (_compatPvPLifeRespectReservations && PlayerbotsLife::IsReserved(player->GetGUID().GetCounter()))
            return false;

        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai || ::IsRealPlayer(player) || ai->HasGameClientMaster())
            return false;
        if (_respectPlayerbotActivity && !ai->AllowActivity(ALL_ACTIVITY))
            return false;
        if (!player->IsAlive() || player->IsInCombat() || player->IsInFlight() || player->duel)
            return false;
        if (player->InBattleground() || player->InBattlegroundQueue())
            return false;
        if (player->GetMap() && player->GetMap()->Instanceable())
            return false;
        if (_skipGroupedBots && player->GetGroup())
            return false;
        return !IsResident(player->GetGUID().GetCounter());
    }

    bool Manager::IsResidentUsable(Player* player) const
    {
        if (!player || !player->GetSession() || !player->IsInWorld())
            return false;
        if (!sRandomPlayerbotMgr.IsRandomBot(player) &&
            !const_cast<Manager*>(this)->IsConfiguredBotAccount(player->GetSession()->GetAccountId()))
            return false;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai || ::IsRealPlayer(player) || ai->HasGameClientMaster())
            return false;
        if (!player->IsAlive() || player->IsInFlight() || player->duel)
            return false;
        if (player->InBattleground() || player->InBattlegroundQueue())
            return false;
        if (player->GetMap() && player->GetMap()->Instanceable())
            return false;
        if (_skipGroupedBots && player->GetGroup())
            return false;
        return true;
    }

    bool Manager::IsResident(uint32 guidLow) const
    {
        return std::any_of(_residents.begin(), _residents.end(),
            [&](Resident const& resident) { return resident.Bot.GuidLow == guidLow; });
    }

    void Manager::DisengageResident(uint32 guidLow)
    {
        auto it = std::find_if(_residents.begin(), _residents.end(),
            [&](Resident const& r) { return r.Bot.GuidLow == guidLow; });

        if (it != _residents.end())
        {
            Player* player = FindPlayer(guidLow);
            if (player)
            {
                Activities::CrowdClearAfk(player);
                Activities::CrowdReleaseChat(player, *it);
            }
            ReleaseResident(*it, false); // Do not return-teleport away from group or master
            _residents.erase(it);

            if (_debug)
                LOG_INFO("module", "[CityLife] Disengaged resident {} on invite or control takeover.", guidLow);
        }
    }

    std::vector<BotCandidate> Manager::LoadCandidates(Hub const& hub)
    {
        std::vector<BotCandidate> candidates;
        std::vector<uint32> onlineGuids;
        {
            std::shared_lock<std::shared_mutex> playerLock(*HashMapHolder<Player>::GetLock());
            HashMapHolder<Player>::MapType const& players = ObjectAccessor::GetPlayers();
            onlineGuids.reserve(players.size());
            for (auto const& pair : players)
                if (pair.second)
                    onlineGuids.push_back(pair.second->GetGUID().GetCounter());
        }

        for (uint32 guidLow : onlineGuids)
        {
            Player* player = FindPlayer(guidLow);
            if (!player || player->GetLevel() < hub.MinLevel || player->GetLevel() > hub.MaxLevel)
                continue;
            if (!IsCandidate(player))
                continue;
            if (player->GetZoneId() == AREA_WINTERGRASP)
                continue;

            BotCandidate candidate;
            candidate.GuidLow = guidLow;
            candidate.AccountId = player->GetSession()->GetAccountId();
            candidate.Name = player->GetName();
            candidate.Level = player->GetLevel();
            candidate.Race = player->getRace();
            candidate.Team = TeamForRace(candidate.Race);
            if (hub.Team != TeamSide::Any && candidate.Team != hub.Team)
                continue;
            candidates.push_back(candidate);
        }

        for (size_t i = candidates.size(); i > 1; --i)
            std::swap(candidates[i - 1], candidates[urand(0, static_cast<uint32>(i - 1))]);
        if (candidates.size() > _botQueryLimit)
            candidates.resize(_botQueryLimit);
        return candidates;
    }

    Hub* Manager::FindHub(uint32 hubId)
    {
        auto found = std::find_if(_hubs.begin(), _hubs.end(),
            [&](Hub const& hub) { return hub.Id == hubId; });
        return found == _hubs.end() ? nullptr : &*found;
    }

    Hub* Manager::FindHub(std::string const& name)
    {
        std::string lowered = Lower(name);
        auto found = std::find_if(_hubs.begin(), _hubs.end(),
            [&](Hub const& hub) { return Lower(hub.Name) == lowered; });
        return found == _hubs.end() ? nullptr : &*found;
    }

    Hub const* Manager::FindHubByZone(uint32 zoneId) const
    {
        for (Hub const& hub : _hubs)
            if (hub.ZoneId != 0 && hub.ZoneId == zoneId)
                return &hub;
        return nullptr;
    }

    Spot const* Manager::FindSpot(uint32 hubId, uint32 spotId) const
    {
        for (Hub const& hub : _hubs)
        {
            if (hub.Id == hubId)
            {
                for (Spot const& spot : hub.Spots)
                    if (spot.Id == spotId)
                        return &spot;
            }
        }
        return nullptr;
    }

    bool Manager::IsStaticServiceSpot(Hub const& hub, Spot const& spot) const
    {
        return _staticServiceHubs.find(Lower(hub.Name)) != _staticServiceHubs.end() &&
            _staticServiceCategories.find(Lower(spot.Category)) != _staticServiceCategories.end();
    }

    bool Manager::UsesAlternatingHubRoute(std::string const& hubName) const
    {
        std::string lower = Lower(hubName);
        return lower == "stormwind" ||
               lower == "orgrimmar" ||
               lower == "ironforge" ||
               lower == "undercity" ||
               lower == "darnassus" ||
               lower == "exodar" ||
               lower == "thunderbluff" ||
               lower == "silvermoon" ||
               lower == "shattrath" ||
               lower == "dalaran";
    }

    bool Manager::IsHubDistrict(std::string const& hubName, std::string const& district) const
    {
        std::string h = Lower(hubName);
        std::string d = Lower(district);

        if (h == "stormwind")
            return d == "trade" || d == "tradedistrict";
        if (h == "orgrimmar")
            return d == "valley" || d == "valleyofstrength" || d == "valleyinn" || d == "valleymarket";
        if (h == "ironforge")
            return d == "greatforge" || d == "commons";
        if (h == "undercity")
            return d == "magicquarter" || d == "apothecarium";
        if (h == "darnassus")
            return d == "craftsmen" || d == "tradesmensterrace";
        if (h == "exodar")
            return d == "seat" || d == "seatofnaaru";
        if (h == "thunderbluff")
            return d == "lowerrise" || d == "central";
        if (h == "silvermoon")
            return d == "bazaar" || d == "sunfuryspire" || d == "sunfury";
        if (h == "shattrath")
            return d == "terrace" || d == "centralterrace";
        if (h == "dalaran")
            return d == "maguscommerce" || d == "centralloop";

        return false;
    }

    bool Manager::IsOutlyingDistrict(std::string const& hubName, std::string const& district) const
    {
        return !IsHubDistrict(hubName, district);
    }

    Spot const* Manager::ChooseSpot(Hub const& hub, uint32 ignoredSpotId, bool filterByDistrict, bool targetHubDistrict, Player* botForPathCheck) const
    {
        Spot const* best = nullptr;
        uint64 bestScore = std::numeric_limits<uint64>::max();
        bool hasAlternative = hub.Spots.size() > 1;

        std::vector<Spot const*> candidateList;
        if (filterByDistrict)
        {
            for (Spot const& spot : hub.Spots)
            {
                if (hasAlternative && spot.Id == ignoredSpotId)
                    continue;

                bool isHub = IsHubDistrict(hub.Name, spot.District);
                if (targetHubDistrict ? isHub : !isHub)
                    candidateList.push_back(&spot);
            }
        }

        if (candidateList.empty())
        {
            for (Spot const& spot : hub.Spots)
            {
                if (hasAlternative && spot.Id == ignoredSpotId)
                    continue;
                candidateList.push_back(&spot);
            }
        }

        for (Spot const* spotPtr : candidateList)
        {
            Spot const& spot = *spotPtr;

            // Water avoidance sensitivity check
            if (_waterAvoidance && botForPathCheck && spot.PoiType != CityPoiType::Dock)
            {
                if (!Movement::HasCompleteWalkablePath(botForPathCheck, spot.X, spot.Y, spot.Z, false))
                    continue;
            }

            uint32 occupants = static_cast<uint32>(std::count_if(_residents.begin(), _residents.end(),
                [&](Resident const& resident)
                {
                    return resident.HubId == hub.Id && resident.SpotId == spot.Id;
                }));
            uint64 weight = spot.Weight;
            if (IsStaticServiceSpot(hub, spot))
                weight = weight * _serviceSpotWeightMultiplier / 100;
            uint64 score = (static_cast<uint64>(occupants) + 1) * 100000 /
                std::max<uint64>(1, weight) + urand(0, 100);
            if (score < bestScore)
            {
                bestScore = score;
                best = &spot;
            }
        }

        // Fallback if all candidates rejected by path check
        if (!best && !candidateList.empty())
        {
            for (Spot const* spotPtr : candidateList)
            {
                Spot const& spot = *spotPtr;
                uint32 occupants = static_cast<uint32>(std::count_if(_residents.begin(), _residents.end(),
                    [&](Resident const& resident)
                    {
                        return resident.HubId == hub.Id && resident.SpotId == spot.Id;
                    }));
                uint64 weight = spot.Weight;
                if (IsStaticServiceSpot(hub, spot))
                    weight = weight * _serviceSpotWeightMultiplier / 100;
                uint64 score = (static_cast<uint64>(occupants) + 1) * 100000 /
                    std::max<uint64>(1, weight) + urand(0, 100);
                if (score < bestScore)
                {
                    bestScore = score;
                    best = &spot;
                }
            }
        }

        return best;
    }

    CitizenRole Manager::DetermineRole(Hub const& hub, Spot const& spot, BotCandidate const& candidate) const
    {
        if (hub.DuelHub || spot.PoiType == CityPoiType::DuelRing)
        {
            if (candidate.Level >= 80)
            {
                if (Lower(hub.Name).find("goldshire") != std::string::npos)
                    return CitizenRole::GoldshireDuelist;
                return CitizenRole::GateDuelist;
            }
        }

        if (spot.PoiType == CityPoiType::Dock && _actFishing)
            return CitizenRole::Fisher;

        if (Lower(hub.Name) == "goldshire" && spot.Category == "inn" && _actInnDancing)
            return CitizenRole::InnDancer;

        if (spot.PoiType == CityPoiType::Plaza && _actPlazaSocial)
            return CitizenRole::PlazaTalker;

        if ((spot.PoiType == CityPoiType::Bank || spot.PoiType == CityPoiType::AuctionHouse) &&
            _actCrowdScenes && _crowdEnable)
            return CitizenRole::Crowd;

        return CitizenRole::Flex;
    }

    CitizenActivity Manager::ChooseInitialActivity(CitizenRole role, Spot const& spot) const
    {
        switch (role)
        {
            case CitizenRole::Fisher:
                return CitizenActivity::FishAtDocks;
            case CitizenRole::InnDancer:
                return CitizenActivity::InnDance;
            case CitizenRole::PlazaTalker:
                return CitizenActivity::PlazaSocial;
            case CitizenRole::GateDuelist:
            case CitizenRole::GoldshireDuelist:
                return CitizenActivity::Duel;
            case CitizenRole::Crowd:
                return CitizenActivity::CrowdSocial;
            default:
                break;
        }

        switch (spot.PoiType)
        {
            case CityPoiType::Tavern:
                return _actTavernSitting ? CitizenActivity::SitInTavern : CitizenActivity::Idle;
            case CityPoiType::AuctionHouse:
                return _actAuctionHouse ? CitizenActivity::VisitAuctionHouse : CitizenActivity::Idle;
            case CityPoiType::Mailbox:
                return _actMailbox ? CitizenActivity::VisitMailbox : CitizenActivity::Idle;
            case CityPoiType::Trainer:
                return _actTrainer ? CitizenActivity::VisitTrainer : CitizenActivity::Idle;
            case CityPoiType::Vendor:
                return _actFoodVendor ? CitizenActivity::BuyFood : CitizenActivity::Idle;
            case CityPoiType::WallPatrol:
                return _actWallWalking ? CitizenActivity::WalkCityWalls : CitizenActivity::Idle;
            case CityPoiType::DuelRing:
                return _actDuelWatching ? CitizenActivity::WatchDuels : CitizenActivity::Idle;
            default:
                return _actRoaming ? CitizenActivity::WanderDistrict : CitizenActivity::RandomEmote;
        }
    }

    uint32 Manager::CountResidents(uint32 hubId) const
    {
        return static_cast<uint32>(std::count_if(_residents.begin(), _residents.end(),
            [&](Resident const& resident) { return resident.HubId == hubId; }));
    }

    uint32 Manager::CurrentTimeMultiplier() const
    {
        if (!_timeScaling)
            return 100;

        std::time_t timestamp = std::time(nullptr);
        std::tm const* local = std::localtime(&timestamp);
        if (!local)
            return 100;
        std::tm localTime = *local;
        if (localTime.tm_hour >= 6 && localTime.tm_hour < 12)
            return _morningMultiplier;
        if (localTime.tm_hour >= 12 && localTime.tm_hour < 18)
            return _dayMultiplier;
        if (localTime.tm_hour >= 18)
            return _eveningMultiplier;
        return _nightMultiplier;
    }

    uint32 Manager::CalculateTotalBudget(uint32 eligiblePoolCount) const
    {
        uint32 requested = (_budgetMode == BudgetMode::Count)
            ? _fixedCount
            : static_cast<uint32>(std::floor(eligiblePoolCount * _borrowPercent / 100.0f));

        return std::min({ requested, _maxTotalPopulation, eligiblePoolCount });
    }

    uint32 Manager::TargetPopulation(Hub const& hub) const
    {
        if (!hub.Enabled || hub.Spots.empty())
            return 0;

        uint32 nominal = hub.ConfiguredPopulation;
        if (_timeScaling && hub.UseTimeScaling)
        {
            nominal = static_cast<uint32>(std::lround(
                static_cast<double>(nominal) * CurrentTimeMultiplier() / 100.0));
        }

        if (_demandMode == DemandMode::Fixed || hub.DuelHub)
            return nominal;

        // Dynamic or Hybrid demand evaluation
        uint32 now = GameTimeSeconds();
        bool playerPresent = false;

        if (hub.ZoneId != 0)
        {
            for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
            {
                (void)guid;
                if (!player || !player->IsInWorld() || !player->GetSession() || player->GetSession()->IsBot())
                    continue;

                if (player->GetZoneId() == hub.ZoneId)
                {
                    playerPresent = true;
                    const_cast<Manager*>(this)->_hubLastRealPlayerTime[hub.Id] = now;
                    break;
                }
            }
        }

        if (!playerPresent)
        {
            auto lastSeenIt = _hubLastRealPlayerTime.find(hub.Id);
            if (lastSeenIt != _hubLastRealPlayerTime.end() && now - lastSeenIt->second < _burstGraceSeconds)
            {
                playerPresent = true; // In grace period
            }
        }

        if (playerPresent)
            return nominal;

        // When unoccupied:
        if (_demandMode == DemandMode::Dynamic)
            return hub.MinCount;

        // Hybrid mode baseline
        uint32 baseline = static_cast<uint32>(std::lround(nominal * _baselinePercent / 100.0));
        return std::max(baseline, hub.MinCount);
    }

    void Manager::OnPlayerZoneUpdate(Player* player, uint32 zoneId)
    {
        if (!player || !player->GetSession() || player->GetSession()->IsBot())
            return;

        if (Hub const* hub = FindHubByZone(zoneId))
        {
            _hubLastRealPlayerTime[hub->Id] = GameTimeSeconds();
        }
    }

    void Manager::OnPlayerLogin(Player* player)
    {
        if (!player || !player->GetSession() || player->GetSession()->IsBot())
            return;

        if (Hub const* hub = FindHubByZone(player->GetZoneId()))
        {
            _hubLastRealPlayerTime[hub->Id] = GameTimeSeconds();
        }
    }

    void Manager::OnPlayerLogout(Player* player)
    {
        if (player && player->GetSession() && player->GetSession()->IsBot())
        {
            uint32 guid = player->GetGUID().GetCounter();
            if (IsResident(guid))
                DisengageResident(guid);
        }
    }

    Player* Manager::FindCrowdChatPartner(uint32 hubId, Player* bot)
    {
        if (!bot)
            return nullptr;

        for (Resident const& r : _residents)
        {
            if (r.HubId != hubId || r.Bot.GuidLow == bot->GetGUID().GetCounter())
                continue;

            if (r.Role == CitizenRole::Crowd && r.CrowdPhaseState == CrowdPhase::Linger &&
                r.ChatPartnerGuid == 0 && r.Activity == CitizenActivity::CrowdSocial)
            {
                Player* p = FindPlayer(r.Bot.GuidLow);
                if (p && p->IsInWorld() && bot->GetDistance(p) <= 15.0f)
                    return p;
            }
        }
        return nullptr;
    }

    void Manager::BeginCrowdChat(Player* bot, Player* partner, Resident& resident, uint32 now)
    {
        if (!bot || !partner)
            return;

        uint32 partnerGuidLow = partner->GetGUID().GetCounter();
        Resident* partnerResident = nullptr;
        for (Resident& r : _residents)
        {
            if (r.Bot.GuidLow == partnerGuidLow)
            {
                partnerResident = &r;
                break;
            }
        }

        if (!partnerResident)
            return;

        // Determine dialogue pool
        CityPoiType pool = CityPoiType::Plaza;
        if (Spot const* spot = FindSpot(resident.HubId, resident.SpotId))
        {
            if (spot->PoiType == CityPoiType::Bank || spot->PoiType == CityPoiType::AuctionHouse)
                pool = spot->PoiType;
        }

        std::vector<int16> candidates;
        for (int16 i = 0; i < Dialogues::kCrowdDialogueCount; ++i)
        {
            if (Dialogues::kCrowdDialogues[i].Pool == pool || Dialogues::kCrowdDialogues[i].Pool == CityPoiType::Plaza)
                candidates.push_back(i);
        }

        if (candidates.empty())
            return;

        int16 dialogueId = candidates[urand(0, static_cast<uint32>(candidates.size() - 1))];

        resident.ChatDialogueId = dialogueId;
        resident.ChatPartnerGuid = partnerGuidLow;
        resident.ChatInitiator = true;
        resident.ChatLineIdx = 0;
        resident.ChatDeadlineMs = now + 45 * 1000;

        partnerResident->ChatPartnerGuid = bot->GetGUID().GetCounter();
        partnerResident->ChatInitiator = false;
        partnerResident->ChatDeadlineMs = resident.ChatDeadlineMs;

        bot->SetFacingToObject(partner);
        partner->SetFacingToObject(bot);
    }

    void Manager::ClearResidentChatPartner(uint32 partnerGuidLow, uint32 expectedGuidLow)
    {
        for (Resident& r : _residents)
        {
            if (r.Bot.GuidLow == partnerGuidLow && r.ChatPartnerGuid == expectedGuidLow)
            {
                r.ChatPartnerGuid = 0;
                r.ChatInitiator = false;
                r.ChatDialogueId = -1;
                r.ChatLineIdx = 0;
                r.ChatDeadlineMs = 0;
                r.LastCrowdChatMs = GameTimeMS();
                break;
            }
        }
    }

    Resident* Manager::FindResident(uint32 guidLow)
    {
        for (Resident& r : _residents)
        {
            if (r.Bot.GuidLow == guidLow)
                return &r;
        }
        return nullptr;
    }

    Player* Manager::FindDuelOpponent(Hub const& hub, Player* bot, float centerX, float centerY, float radius)
    {
        if (!bot)
            return nullptr;

        for (Resident const& r : _residents)
        {
            if (r.HubId != hub.Id || r.Bot.GuidLow == bot->GetGUID().GetCounter())
                continue;

            if (r.Activity == CitizenActivity::Duel && r.DuelPartnerGuid == 0 &&
                GameTimeMS() >= r.DuelCooldownUntilMs)
            {
                Player* opponent = FindPlayer(r.Bot.GuidLow);
                if (!opponent || !opponent->IsInWorld() || opponent->duel || opponent->IsInCombat())
                    continue;

                if (opponent->GetDistance2d(centerX, centerY) > radius)
                    continue;

                if (!bot->IsWithinLOSInMap(opponent))
                    continue;

                return opponent;
            }
        }

        if (_duelAllowPlayerChallenge && bot->GetMap())
        {
            uint32 nowSec = GameTimeSeconds();
            for (auto const& pRef : bot->GetMap()->GetPlayers())
            {
                Player* opponent = pRef.GetSource();
                if (!opponent || !opponent->IsInWorld() || opponent->duel || opponent->IsInCombat())
                    continue;

                if (opponent->IsGameMaster() || (opponent->GetSession() && (opponent->GetSession()->IsBot() || opponent->GetSession()->GetSecurity() > SEC_PLAYER)))
                    continue;

                uint32 oppGuidLow = opponent->GetGUID().GetCounter();
                if (!PlayerbotsLife::CanChallengeRealPlayer(oppGuidLow, nowSec))
                    continue;

                if (opponent->GetDistance2d(centerX, centerY) > radius)
                    continue;

                if (!bot->IsWithinLOSInMap(opponent))
                    continue;

                PlayerbotsLife::RecordRealPlayerChallenge(oppGuidLow, nowSec, _duelCooldownSec);
                return opponent;
            }
        }

        return nullptr;
    }

    void Manager::AssignSpot(Resident& resident, Hub const& hub, Spot const& spot, bool allowStationary)
    {
        float angle = frand(0.0f, TwoPi);
        float distance = spot.Radius > 0.0f ? frand(0.0f, spot.Radius) : 0.0f;
        resident.HubId = hub.Id;
        resident.SpotId = spot.Id;
        resident.TargetMap = hub.MapId;
        resident.TargetX = spot.X + std::cos(angle) * distance;
        resident.TargetY = spot.Y + std::sin(angle) * distance;
        resident.TargetZ = spot.Z;
        resident.TargetO = spot.O;
        resident.Stationary = allowStationary && IsStaticServiceSpot(hub, spot) &&
            urand(1, 100) <= _staticServiceChance;

        resident.Role = DetermineRole(hub, spot, resident.Bot);
        resident.Activity = ChooseInitialActivity(resident.Role, spot);
        resident.ActivityStartMs = GameTimeMS();
        resident.ActivityDurationMs = urand(45, 180) * 1000;
        resident.LastRoamWasHub = IsHubDistrict(hub.Name, spot.District);
    }

    bool Manager::AddResident(Hub const& hub)
    {
        Spot const* spot = ChooseSpot(hub);
        if (!spot)
            return false;
        std::vector<BotCandidate> candidates = LoadCandidates(hub);
        if (candidates.empty())
            return false;

        for (BotCandidate const& bot : candidates)
        {
            Player* player = FindPlayer(bot.GuidLow);
            if (!IsCandidate(player))
                continue;

            PlayerbotsLife::LeaseToken token;
            if (!PlayerbotsLife::TryAcquireLease(token, bot.GuidLow, PlayerbotsLife::ReservationOwner::CityLife,
                                                 hub.Id, PlayerbotsLife::LeaseState::Active))
                continue;

            Resident resident;
            resident.Bot = bot;
            resident.Token = token;
            resident.OriginalMap = player->GetMapId();
            resident.OriginalX = player->GetPositionX();
            resident.OriginalY = player->GetPositionY();
            resident.OriginalZ = player->GetPositionZ();
            resident.OriginalO = player->GetOrientation();
            AssignSpot(resident, hub, *spot, true);

            uint32 now = GameTimeSeconds();
            resident.NextMoveAt = now + urand(_relocateMinSeconds, _relocateMaxSeconds);
            resident.NextEmoteAt = now + urand(_emoteMinSeconds, _emoteMaxSeconds);
            resident.NextReservationRefreshAt = now + ReservationRefreshSeconds;
            resident.ReleaseAt = hub.ResidenceSeconds == 0 ? 0 : now + hub.ResidenceSeconds;
            resident.LastSweepZ = resident.TargetZ;

            ApplyCityStrategies(player);
            SetStayPosition(player, resident);
            if (sRandomPlayerbotMgr.IsRandomBot(player))
                sRandomPlayerbotMgr.ScheduleTeleport(bot.GuidLow,
                    std::max(ReservationSeconds, hub.ResidenceSeconds + MINUTE));

            Movement::MoveToPosition(player, hub.MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, true);
            _residents.push_back(resident);

            if (_debug)
                LOG_INFO("module", "[CityLife] Added resident {} (lvl {}, role {}) to {}/{}",
                    bot.Name, bot.Level, static_cast<uint32>(resident.Role), hub.Name, spot->Name);
            return true;
        }
        return false;
    }

    void Manager::ReleaseResident(Resident const& resident, bool returnHome)
    {
        if (resident.DuelStagingSlotId != 0)
            PlayerbotsLife::ReleaseDuelStagingSlot(resident.DuelStagingSlotId);

        PlayerbotsLife::BeginRelease(resident.Token);
        Player* player = FindPlayer(resident.Bot.GuidLow);
        if (player && player->GetSession())
        {
            Activities::CrowdClearAfk(player);
            Movement::DismountIfMounted(player);
            ClearCityStrategies(player);

            if (sRandomPlayerbotMgr.IsRandomBot(player))
                sRandomPlayerbotMgr.ScheduleTeleport(resident.Bot.GuidLow);

            if (returnHome && !player->IsBeingTeleported() && !player->IsInCombat())
            {
                Movement::MoveToPosition(player, resident.OriginalMap, resident.OriginalX, resident.OriginalY,
                    resident.OriginalZ, resident.OriginalO, true);
            }
        }
        PlayerbotsLife::ReleaseLease(resident.Token);
        PlayerbotsLife::Release(resident.Bot.GuidLow, PlayerbotsLife::ReservationOwner::CityLife);
    }

    void Manager::ReleaseAll(bool returnHome)
    {
        for (Resident const& resident : _residents)
            ReleaseResident(resident, returnHome && _returnBots);
        _residents.clear();
    }

    void Manager::Shutdown()
    {
        ReleaseAll(true);
    }

    void Manager::RemoveUnavailableResidents()
    {
        uint32 now = GameTimeSeconds();
        auto resident = _residents.begin();
        while (resident != _residents.end())
        {
            Player* player = FindPlayer(resident->Bot.GuidLow);
            Hub* hub = FindHub(resident->HubId);
            bool residenceExpired = resident->ReleaseAt != 0 && resident->ReleaseAt <= now && player &&
                !player->IsInCombat() && !player->IsBeingTeleported();
            if (!hub || !IsResidentUsable(player) || residenceExpired)
            {
                ReleaseResident(*resident, residenceExpired && _returnBots);
                resident = _residents.erase(resident);
            }
            else
                ++resident;
        }
    }

    void Manager::MaintainPopulation()
    {
        RemoveUnavailableResidents();

        // Calculate pool & total budget
        uint32 eligiblePoolCount = static_cast<uint32>(_residents.size());
        {
            std::shared_lock<std::shared_mutex> playerLock(*HashMapHolder<Player>::GetLock());
            eligiblePoolCount += static_cast<uint32>(ObjectAccessor::GetPlayers().size());
        }

        uint32 totalBudget = CalculateTotalBudget(eligiblePoolCount);

        std::unordered_map<uint32, uint32> targets;
        uint32 remaining = totalBudget;
        for (Hub const& hub : _hubs)
        {
            uint32 target = std::min(TargetPopulation(hub), remaining);
            targets[hub.Id] = target;
            remaining -= target;
        }

        uint32 changes = 0;
        for (Hub const& hub : _hubs)
        {
            uint32 target = targets[hub.Id];
            while (CountResidents(hub.Id) > target && changes < _maxChangesPerTick)
            {
                auto found = std::find_if(_residents.rbegin(), _residents.rend(),
                    [&](Resident const& resident)
                    {
                        Player* player = FindPlayer(resident.Bot.GuidLow);
                        return resident.HubId == hub.Id && player && !player->IsInCombat() &&
                            !player->IsBeingTeleported();
                    });
                if (found == _residents.rend())
                    break;
                auto eraseAt = std::next(found).base();
                ReleaseResident(*eraseAt, _returnBots);
                _residents.erase(eraseAt);
                ++changes;
            }
        }

        std::unordered_set<uint32> blockedHubs;
        while (changes < _maxChangesPerTick)
        {
            Hub const* selectedHub = nullptr;
            uint32 selectedCurrent = 0;
            uint32 selectedTarget = 0;
            for (Hub const& hub : _hubs)
            {
                uint32 target = targets[hub.Id];
                uint32 current = CountResidents(hub.Id);
                if (current >= target || blockedHubs.find(hub.Id) != blockedHubs.end())
                    continue;

                bool lowerFillRatio = selectedHub &&
                    static_cast<uint64>(current) * selectedTarget <
                    static_cast<uint64>(selectedCurrent) * target;
                if (!selectedHub || lowerFillRatio)
                {
                    selectedHub = &hub;
                    selectedCurrent = current;
                    selectedTarget = target;
                }
            }

            if (!selectedHub)
                break;
            if (AddResident(*selectedHub))
            {
                ++changes;
                continue;
            }

            blockedHubs.insert(selectedHub->Id);
        }
    }

    void Manager::RelocateResident(Resident& resident, Hub const& hub)
    {
        Player* player = FindPlayer(resident.Bot.GuidLow);
        if (!player)
            return;

        Spot const* spot = nullptr;
        if (_alternatingDistricts && UsesAlternatingHubRoute(hub.Name))
        {
            bool targetHub = !resident.LastRoamWasHub;
            spot = ChooseSpot(hub, resident.SpotId, true, targetHub, player);
            if (spot)
            {
                resident.LastRoamWasHub = IsHubDistrict(hub.Name, spot->District);
            }
        }

        if (!spot)
        {
            spot = ChooseSpot(hub, resident.SpotId, false, false, player);
            if (spot)
            {
                resident.LastRoamWasHub = IsHubDistrict(hub.Name, spot->District);
            }
        }

        if (!spot)
            return;

        AssignSpot(resident, hub, *spot, false);
        resident.NextMoveAt = GameTimeSeconds() + urand(_relocateMinSeconds, _relocateMaxSeconds);

        SetStayPosition(player, resident);

        // Mounting decision for outdoor distance
        float distance = player->GetDistance2d(resident.TargetX, resident.TargetY);
        if (_actMountedTravel && distance > 30.0f && urand(1, 100) <= _mountBetweenPoiChance)
        {
            Movement::TryMount(player);
        }

        Movement::MoveToPosition(player, hub.MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, false);
    }

    void Manager::MaintainResidents()
    {
        uint32 nowSec = GameTimeSeconds();
        uint32 nowMs = GameTimeMS();

        for (Resident& resident : _residents)
        {
            Hub* hub = FindHub(resident.HubId);
            Player* player = FindPlayer(resident.Bot.GuidLow);
            if (!hub || !player || player->IsBeingTeleported())
                continue;

            // Reservation refresh
            if (resident.NextReservationRefreshAt <= nowSec && sRandomPlayerbotMgr.IsRandomBot(player))
            {
                sRandomPlayerbotMgr.ScheduleTeleport(resident.Bot.GuidLow,
                    std::max(ReservationSeconds, hub->ResidenceSeconds + MINUTE));
                resident.NextReservationRefreshAt = nowSec + ReservationRefreshSeconds;
            }

            // Tactical Duel Combat & Boundary Maintenance (Option A)
            if (player->duel && (resident.Activity == CitizenActivity::Duel || resident.Role == CitizenRole::GateDuelist || resident.Role == CitizenRole::GoldshireDuelist) && _actDuels)
            {
                Activities::DriveActiveDuel(resident, player, *hub, nowMs, _duelMaxDistance);
                continue;
            }

            if (player->IsInCombat() || player->duel || player->IsInFlight())
                continue;

            // Sink Forensics Check
            Movement::CheckSinkForensics(player, resident.LastSweepZ, _sinkForensics);
            resident.LastSweepZ = player->GetPositionZ();

            // Multi-level floor protection: if way below lowest POI, recover
            float lowestZ = 0.0f;
            if (Movement::LowestPoiZWithin(*hub, player->GetPositionX(), player->GetPositionY(), 20.0f, &lowestZ))
            {
                if (player->GetPositionZ() < lowestZ - 15.0f && !player->IsBeingTeleported())
                {
                    Movement::MoveToPosition(player, hub->MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, true);
                    continue;
                }
            }

            // Leash check
            if (player->GetMapId() != hub->MapId ||
                player->GetDistance2d(resident.TargetX, resident.TargetY) > _leashRadius)
            {
                ApplyCityStrategies(player);
                SetStayPosition(player, resident);
                Movement::MoveToPosition(player, hub->MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, true);
                continue;
            }

            // Arrival check: dismount when close to target
            if (player->GetDistance2d(resident.TargetX, resident.TargetY) <= 4.0f)
            {
                if (!resident.AtSpot)
                {
                    resident.AtSpot = true;
                    Movement::DismountIfMounted(player);
                }
            }
            else
            {
                resident.AtSpot = false;
            }

            // Roaming relocation
            if (_roaming && !resident.Stationary && resident.NextMoveAt <= nowSec)
            {
                RelocateResident(resident, *hub);
                continue;
            }

            // Activity Execution Dispatch
            switch (resident.Activity)
            {
                case CitizenActivity::CrowdSocial:
                    if (_actCrowdScenes && _crowdEnable)
                        Activities::ExecuteLivingCrowd(resident, player, *hub, nowMs, _chatScriptedConversations,
                            _chatSayCooldownSec, _crowdPlayerRadius, _crowdChatCooldownSec, _crowdAfkMinSec, _crowdAfkMaxSec);
                    break;
                case CitizenActivity::Duel:
                    if (_actDuels)
                        Activities::ExecuteDuel(resident, player, *hub, nowMs, _duelCooldownSec, _duelMaxDistance, _duelMeetingPoint);
                    break;
                case CitizenActivity::WatchDuels:
                    if (_actDuelWatching)
                        Activities::ExecuteDuelSpectator(resident, player, *hub, nowMs);
                    break;
                case CitizenActivity::FishAtDocks:
                    if (_actFishing)
                        Activities::ExecuteFishing(resident, player, *hub, nowMs);
                    break;
                case CitizenActivity::InnDance:
                    if (_actInnDancing)
                        Activities::ExecuteInnDance(resident, player, *hub, nowMs, _chatGreetings, _chatSayCooldownSec);
                    break;
                case CitizenActivity::PlazaSocial:
                    if (_actPlazaSocial)
                        Activities::ExecutePlazaSocial(resident, player, *hub, nowMs, _chatScriptedConversations, _chatSayCooldownSec);
                    break;
                case CitizenActivity::SitInTavern:
                    if (_actTavernSitting)
                        Activities::ExecuteTavernSitting(resident, player, *hub, nowMs);
                    break;
                case CitizenActivity::DrinkInTavern:
                    if (_actTavernDrinking)
                        Activities::ExecuteTavernDrinking(resident, player, *hub, nowMs);
                    break;
                case CitizenActivity::WalkCityWalls:
                    if (_actWallWalking)
                        Activities::ExecuteWallPatrol(resident, player, *hub, nowMs);
                    break;
                case CitizenActivity::VisitAuctionHouse:
                    if (_actAuctionHouse)
                        Activities::ExecuteVisitNpc(resident, player, *hub, nowMs, UNIT_NPC_FLAG_AUCTIONEER);
                    break;
                case CitizenActivity::VisitTrainer:
                    if (_actTrainer)
                        Activities::ExecuteVisitNpc(resident, player, *hub, nowMs, UNIT_NPC_FLAG_TRAINER);
                    break;
                case CitizenActivity::VisitMailbox:
                    if (_actMailbox)
                        Activities::ExecuteVisitMailbox(resident, player, *hub, nowMs);
                    break;
                case CitizenActivity::BuyFood:
                    if (_actFoodVendor)
                        Activities::ExecuteBuyFood(resident, player, *hub, nowMs, _enableBuyFood);
                    break;
                case CitizenActivity::RandomEmote:
                default:
                    if (_actRandomEmotes && resident.NextEmoteAt <= nowSec)
                    {
                        Activities::ExecuteRandomEmote(resident, player, *hub, nowMs);
                        resident.NextEmoteAt = nowSec + urand(_emoteMinSeconds, _emoteMaxSeconds);
                    }
                    break;
            }
        }
    }

    void Manager::ApplyCityStrategies(Player* player) const
    {
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            if (!ai->HasStrategy("stay", BOT_STATE_NON_COMBAT))
                ai->ChangeStrategy(_nonCombatStrategies, BOT_STATE_NON_COMBAT);
            if (ai->HasStrategy("duel", BOT_STATE_NON_COMBAT) ||
                ai->HasStrategy("pvp", BOT_STATE_NON_COMBAT))
                ai->ChangeStrategy("-duel,-pvp", BOT_STATE_NON_COMBAT);
        }
    }

    void Manager::SetStayPosition(Player* player, Resident const& resident) const
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai)
            return;
        PositionMap& positions = ai->GetAiObjectContext()->GetValue<PositionMap&>("position")->Get();
        PositionInfo stay = positions["stay"];
        stay.Set(resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetMap);
        positions["stay"] = stay;
    }

    void Manager::ClearCityStrategies(Player* player) const
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai)
            return;
        PositionMap& positions = ai->GetAiObjectContext()->GetValue<PositionMap&>("position")->Get();
        PositionInfo stay = positions["stay"];
        stay.Reset();
        positions["stay"] = stay;
        ai->ResetStrategies();
    }

    void Manager::Update(uint32 diff)
    {
        if (!_enable)
            return;

        if (_startupElapsedMs < _startupDelaySeconds * IN_MILLISECONDS)
        {
            _startupElapsedMs += diff;
            return;
        }

        if (_timerMs > diff)
        {
            _timerMs -= diff;
            return;
        }

        _timerMs = _tickSeconds * IN_MILLISECONDS;
        MaintainPopulation();
        MaintainResidents();
    }

    void Manager::PrintStatus(ChatHandler* handler) const
    {
        uint32 startupTarget = _startupDelaySeconds * IN_MILLISECONDS;
        uint32 startupLeft = _startupElapsedMs < startupTarget ?
            (startupTarget - _startupElapsedMs + 999) / 1000 : 0;
        handler->PSendSysMessage(
            "City Life: enabled={} DB={} startup={}s residents={}/{} time={}%% hubs={} crowd={} duels={}",
            _enable ? 1 : 0, _databaseReady ? "ready" : "missing", startupLeft,
            _residents.size(), _maxTotalPopulation, CurrentTimeMultiplier(), _hubs.size(),
            _crowdEnable ? 1 : 0, _actDuels ? 1 : 0);
        for (Hub const& hub : _hubs)
        {
            handler->PSendSysMessage("{} [{}] lvl {}-{} cfg={} target={} current={} spots={}",
                hub.Name, TeamName(hub.Team), hub.MinLevel, hub.MaxLevel, hub.ConfiguredPopulation,
                TargetPopulation(hub), CountResidents(hub.Id), hub.Spots.size());
        }
    }

    void Manager::PrintDebugStatus(ChatHandler* handler) const
    {
        handler->PSendSysMessage("City Life Diagnostics:");
        handler->PSendSysMessage("BudgetMode: {} | DemandMode: {} | Baseline: {}%% | Grace: {}s",
            _budgetMode == BudgetMode::Percentage ? "Percentage" : "Count",
            _demandMode == DemandMode::Hybrid ? "Hybrid" : (_demandMode == DemandMode::Dynamic ? "Dynamic" : "Fixed"),
            _baselinePercent, _burstGraceSeconds);
        handler->PSendSysMessage("LivingCrowd: AwakeRadius={}yd ChatCooldown={}s AFK={}-{}s",
            _crowdPlayerRadius, _crowdChatCooldownSec, _crowdAfkMinSec, _crowdAfkMaxSec);
        handler->PSendSysMessage("Duels: Cooldown={}s MaxDist={:.1f}yd AllowPlayer={}",
            _duelCooldownSec, _duelMaxDistance, _duelAllowPlayerChallenge ? 1 : 0);
        handler->PSendSysMessage("Active Residents by Role:");

        uint32 crowdCount = 0, duelCount = 0, fisherCount = 0, dancerCount = 0, flexCount = 0;
        for (Resident const& r : _residents)
        {
            switch (r.Role)
            {
                case CitizenRole::Crowd: ++crowdCount; break;
                case CitizenRole::GateDuelist:
                case CitizenRole::GoldshireDuelist: ++duelCount; break;
                case CitizenRole::Fisher: ++fisherCount; break;
                case CitizenRole::InnDancer: ++dancerCount; break;
                default: ++flexCount; break;
            }
        }
        handler->PSendSysMessage("Crowd: {} | Duelers: {} | Fishers: {} | Dancers: {} | Ambient: {}",
            crowdCount, duelCount, fisherCount, dancerCount, flexCount);
    }

    void Manager::PrintHelp(ChatHandler* handler) const
    {
        handler->PSendSysMessage("City Life commands:");
        handler->PSendSysMessage(".citylife status");
        handler->PSendSysMessage(".citylife compat");
        handler->PSendSysMessage(".citylife reload");
        handler->PSendSysMessage(".citylife debug status");
        handler->PSendSysMessage(".citylife hub list");
        handler->PSendSysMessage(".citylife hub add <name> <alliance|horde|any> <minLevel> <maxLevel> <population> [priority]");
        handler->PSendSysMessage(".citylife hub population <name> <count>");
        handler->PSendSysMessage(".citylife hub enable|disable <name>");
        handler->PSendSysMessage(".citylife spot list <hub>");
        handler->PSendSysMessage(".citylife spot add <hub> <name> <category> [weight] [radius]");
        handler->PSendSysMessage(".citylife spot delete <hub> <name>");
    }

    void Manager::PrintCompat(ChatHandler* handler) const
    {
        if (!handler)
            return;

        uint32 activeModules = PlayerbotsLife::GetActiveModules();
        bool clActive = (activeModules & PlayerbotsLife::ModuleSystemFlags::MODULE_CITY_LIFE) != 0;
        bool pvpActive = (activeModules & PlayerbotsLife::ModuleSystemFlags::MODULE_PVP_LIFE) != 0;
        bool artActive = (activeModules & PlayerbotsLife::ModuleSystemFlags::MODULE_ARTISANS) != 0;

        auto& registry = PlayerbotsLife::GetReservationRegistry();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        uint32 clCount = 0;
        uint32 pvpCount = 0;
        uint32 artCount = 0;
        for (auto const& [guid, owner] : registry.Owners)
        {
            if (owner == PlayerbotsLife::ReservationOwner::CityLife)
                ++clCount;
            else if (owner == PlayerbotsLife::ReservationOwner::PvPLife)
                ++pvpCount;
            else if (owner == PlayerbotsLife::ReservationOwner::Artisans)
                ++artCount;
        }

        handler->PSendSysMessage("=== Playerbots Life Coexistence Status ===");
        handler->PSendSysMessage("Active Modules: CityLife=%s | PvPLife=%s | Artisans=%s",
            clActive ? "ACTIVE" : "INACTIVE",
            pvpActive ? "ACTIVE" : "INACTIVE",
            artActive ? "ACTIVE" : "INACTIVE");
        handler->PSendSysMessage("Active Reservations: CityLife=%u | PvPLife=%u | Artisans=%u (Total: %zu)",
            clCount, pvpCount, artCount, registry.Owners.size());
        handler->PSendSysMessage("Compatibility Mode: %s (Anti-Stealing Barrier: %s)",
            _compatPvPLifeHarmony ? "ENABLED" : "DISABLED",
            _compatPvPLifeRespectReservations ? "ACTIVE" : "DISABLED");
    }

    bool Manager::HandleDebugCommand(ChatHandler* handler, std::vector<std::string> const& args)
    {
        if (args.size() < 2)
            return false;

        std::string sub = Lower(args[1]);
        if (sub == "status")
        {
            PrintDebugStatus(handler);
            return true;
        }

        return false;
    }

    bool Manager::HandleHubCommand(ChatHandler* handler, std::vector<std::string> const& args)
    {
        if (args.size() < 2)
            return false;
        std::string operation = Lower(args[1]);
        if (operation == "list")
        {
            PrintStatus(handler);
            return true;
        }

        if (operation == "add")
        {
            if (args.size() < 7 || !handler->GetSession() || !handler->GetSession()->GetPlayer())
                return false;
            TeamSide team;
            uint32 minLevel, maxLevel, population;
            uint32 priority = 100;
            if (!ParseTeam(args[3], team) || !ParseUInt(args[4], minLevel) ||
                !ParseUInt(args[5], maxLevel) || !ParseUInt(args[6], population) ||
                (args.size() > 7 && !ParseUInt(args[7], priority)) || minLevel > maxLevel || maxLevel > 80)
            {
                handler->PSendSysMessage("City Life: Invalid faction, level, population, or priority.");
                return true;
            }

            Player* gm = handler->GetSession()->GetPlayer();
            std::string name = args[2];
            WorldDatabase.EscapeString(name);
            WorldDatabase.DirectExecute(
                "INSERT INTO city_life_hub "
                "(name,enabled,team,min_level,max_level,map_id,default_population,priority) "
                "VALUES ('{}',1,{},{},{},{},{},{}) ON DUPLICATE KEY UPDATE enabled=1,team=VALUES(team),"
                "min_level=VALUES(min_level),max_level=VALUES(max_level),map_id=VALUES(map_id),"
                "default_population=VALUES(default_population),priority=VALUES(priority)",
                name, static_cast<uint32>(team), minLevel, maxLevel, gm->GetMapId(), population, priority);
            LoadData();
            handler->PSendSysMessage("City Life: Saved hub '{}' at GM position.", name);
            return true;
        }

        if ((operation == "enable" || operation == "disable") && args.size() >= 3)
        {
            std::string name = args[2];
            WorldDatabase.EscapeString(name);
            WorldDatabase.DirectExecute("UPDATE city_life_hub SET enabled={} WHERE name='{}'",
                operation == "enable" ? 1 : 0, name);
            LoadData();
            handler->PSendSysMessage("City Life: Hub '{}' {}.", name, operation == "enable" ? "enabled" : "disabled");
            return true;
        }

        if (operation == "population" && args.size() >= 4)
        {
            uint32 population;
            if (!ParseUInt(args[3], population))
                return false;
            std::string name = args[2];
            WorldDatabase.EscapeString(name);
            WorldDatabase.DirectExecute(
                "UPDATE city_life_hub SET default_population={} WHERE name='{}'", population, name);
            LoadData();
            handler->PSendSysMessage(
                "City Life: Set DB population for hub '{}' to {}.", name, population);
            return true;
        }
        return false;
    }

    bool Manager::HandleSpotCommand(ChatHandler* handler, std::vector<std::string> const& args)
    {
        if (args.size() < 3)
            return false;
        std::string operation = Lower(args[1]);
        Hub* hub = FindHub(args[2]);
        if (!hub)
        {
            handler->PSendSysMessage("City Life: Hub '{}' not found.", args[2]);
            return true;
        }

        if (operation == "list")
        {
            handler->PSendSysMessage("City Life - Spot list for {}:", hub->Name);
            for (Spot const& spot : hub->Spots)
                handler->PSendSysMessage("#{} {} [{}] weight={} radius={} ({:.1f}, {:.1f}, {:.1f})",
                    spot.Id, spot.Name, spot.Category, spot.Weight, spot.Radius, spot.X, spot.Y, spot.Z);
            return true;
        }

        if (operation == "add")
        {
            if (args.size() < 5 || !handler->GetSession() || !handler->GetSession()->GetPlayer())
                return false;
            uint32 weight = 100;
            float radius = 4.0f;
            if ((args.size() > 5 && !ParseUInt(args[5], weight)) ||
                (args.size() > 6 && !ParseFloat(args[6], radius)))
                return false;
            weight = std::max<uint32>(1, weight);
            radius = std::clamp(radius, 0.0f, 30.0f);

            Player* gm = handler->GetSession()->GetPlayer();
            std::string spotName = args[3];
            std::string category = args[4];
            WorldDatabase.EscapeString(spotName);
            WorldDatabase.EscapeString(category);
            WorldDatabase.DirectExecute(
                "INSERT INTO city_life_spot (hub_id,name,category,enabled,x,y,z,o,radius,weight) "
                "VALUES ({},'{}','{}',1,{},{},{},{},{},{}) ON DUPLICATE KEY UPDATE category=VALUES(category),"
                "enabled=1,x=VALUES(x),y=VALUES(y),z=VALUES(z),o=VALUES(o),radius=VALUES(radius),"
                "weight=VALUES(weight)",
                hub->Id, spotName, category, gm->GetPositionX(), gm->GetPositionY(), gm->GetPositionZ(),
                gm->GetOrientation(), radius, weight);
            LoadData();
            handler->PSendSysMessage("City Life: Saved spot '{}/{}' at GM position.", hub->Name, spotName);
            return true;
        }

        if (operation == "delete" && args.size() >= 4)
        {
            std::string spotName = args[3];
            WorldDatabase.EscapeString(spotName);
            WorldDatabase.DirectExecute("DELETE FROM city_life_spot WHERE hub_id={} AND name='{}'",
                hub->Id, spotName);
            LoadData();
            handler->PSendSysMessage("City Life: Deleted spot '{}/{}'.", hub->Name, spotName);
            return true;
        }
        return false;
    }

    bool Manager::HandleCommand(ChatHandler* handler, std::string const& text)
    {
        std::vector<std::string> args = Tokenize(text);
        if (args.empty() || Lower(args[0]) == "help")
        {
            PrintHelp(handler);
            return true;
        }

        std::string command = Lower(args[0]);
        if (command == "status")
        {
            PrintStatus(handler);
            return true;
        }
        if (command == "compat")
        {
            PrintCompat(handler);
            return true;
        }
        if (command == "debug")
        {
            return HandleDebugCommand(handler, args);
        }
        if (command == "reload")
        {
            if (!_databaseReady || !LoadData())
                handler->PSendSysMessage("City Life: Reload failed. Check database and server logs.");
            else
                handler->PSendSysMessage("City Life: Successfully reloaded hubs and spots data.");
            return true;
        }
        if (command == "hub" && HandleHubCommand(handler, args))
            return true;
        if (command == "spot" && HandleSpotCommand(handler, args))
            return true;

        PrintHelp(handler);
        return true;
    }
}
