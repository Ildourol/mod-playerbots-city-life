/*
 * City Life
 * Copyright (C) 2026 iCore
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CityLifeMgr.h"
#include "CityLifeActivities.h"
#include "CityLifeDialogues.h"
#include "CityLifeMovement.h"
#include "CityLifePopulation.h"
#include "Item.h"
#include "StringFormat.h"
#include "LifeBotReservation.h"

#include "AreaDefines.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "SharedDefines.h"
#include "SocialMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldPacket.h"
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
        if (cat == "mailbox" || cat == "mail")
            return CityLife::CityPoiType::Mailbox;
        if (cat == "tavern")
            return CityLife::CityPoiType::Tavern;
        if (cat == "trainer")
            return CityLife::CityPoiType::Trainer;
        if (cat == "dock" || cat == "harbor" || cat == "fish")
            return CityLife::CityPoiType::Dock;
        if (cat == "wall" || cat == "patrol")
            return CityLife::CityPoiType::WallPatrol;
        if (cat == "vendor" || cat == "food")
            return CityLife::CityPoiType::Vendor;
        if (cat == "bed" || cat == "innbed")
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
        CancelAdmissions();
        ++_dataGeneration;
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
        _maxTotalPopulation = sConfigMgr->GetOption<uint32>("CityLife.Population.MaxTotal", 120);
        if (_maxTotalPopulation > 2000)
            _maxTotalPopulation = 2000;

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
        uint32 rawQueryLimit = sConfigMgr->GetOption<uint32>("CityLife.Bots.QueryLimit", 500);
        _botQueryLimit = (rawQueryLimit == 0) ? 0 : std::max<uint32>(50, rawQueryLimit);
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
        _relocateMinSeconds = std::max<uint32>(30, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.RelocateMinSeconds", 60));
        _relocateMaxSeconds = std::max(_relocateMinSeconds, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.RelocateMaxSeconds", 180));
        _leashRadius = std::max<uint32>(10, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.LeashRadius", 35));
        _emotes = sConfigMgr->GetOption<bool>("CityLife.Behaviour.Emotes", true);
        _emoteChance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.EmoteChancePct", 35));
        _emoteMinSeconds = std::max<uint32>(30, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.EmoteMinSeconds", 120));
        _emoteMaxSeconds = std::max(_emoteMinSeconds, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.EmoteMaxSeconds", 360));
        _serviceSpotWeightMultiplier = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("CityLife.Behaviour.ServiceSpotWeightMultiplierPct", 100), 100, 1000);
        _staticServiceChance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("CityLife.Behaviour.StaticServiceChancePct", 20));

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
        _actFishing = sConfigMgr->GetOption<bool>("CityLife.Activities.Fishing.Enable", true);
        _actInnDancing = sConfigMgr->GetOption<bool>("CityLife.Activities.InnDancing.Enable", true);
        _actPlazaSocial = sConfigMgr->GetOption<bool>("CityLife.Activities.PlazaSocial.Enable", true);
        _actCrowdScenes = sConfigMgr->GetOption<bool>("CityLife.Activities.CrowdScenes.Enable", true);
        _actMountedTravel = sConfigMgr->GetOption<bool>("CityLife.Activities.MountedTravel.Enable", true);
        _activitiesAfkEnable = sConfigMgr->GetOption<bool>("CityLife.Activities.AFK.Enable", true);

        // Living Crowd Scenes
        _crowdEnable = sConfigMgr->GetOption<bool>("CityLife.Crowd.Enable", true);
        _crowdPlayerRadius = sConfigMgr->GetOption<float>("CityLife.Crowd.PlayerRadius", 100.0f);
        _crowdChatCooldownSec = sConfigMgr->GetOption<uint32>("CityLife.Crowd.ChatCooldownSec", 90);
        _crowdAfkMinSec = sConfigMgr->GetOption<uint32>("CityLife.Crowd.AfkMinSec", 60);
        _crowdAfkMaxSec = sConfigMgr->GetOption<uint32>("CityLife.Crowd.AfkMaxSec", 180);

        // Movement & Sink Protection
        _mountBetweenPoiChance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("CityLife.Movement.MountBetweenPoiChance", 90));
        _sinkForensics = sConfigMgr->GetOption<bool>("CityLife.Movement.SinkForensics", true);
        _maxPathRefuseStreak = sConfigMgr->GetOption<uint32>("CityLife.Movement.MaxPathRefuseStreak", 3);

        // Chat & Interactions
        _chatScriptedConversations = sConfigMgr->GetOption<bool>("CityLife.Chat.ScriptedConversations", true);
        _chatGreetings = sConfigMgr->GetOption<bool>("CityLife.Chat.Greetings", true);
        _chatUnrestricted = sConfigMgr->GetOption<bool>("CityLife.Chat.Unrestricted", false);
        _chatRandomSayChance = std::min<uint32>(100, sConfigMgr->GetOption<uint32>("CityLife.Chat.RandomSayChance", 0));
        _chatSayCooldownSec = sConfigMgr->GetOption<uint32>("CityLife.Chat.SayCooldownSec", 30);
        _enableInteractions = sConfigMgr->GetOption<bool>("CityLife.Interactions.Enable", true);
        _enablePlayerTrading = sConfigMgr->GetOption<bool>("CityLife.Interactions.PlayerTrading", false);
        _enableBuyFood = sConfigMgr->GetOption<bool>("CityLife.Interactions.BuyFood", true);
        _blockAchievements = sConfigMgr->GetOption<bool>("CityLife.Achievements.BlockAchievements", false);

        // Advanced Micro-Behaviors (Ported from mod-city-bots)
        _alternatingDistricts = sConfigMgr->GetOption<bool>("CityLife.AlternatingDistricts.Enable", true);
        _waterAvoidance = sConfigMgr->GetOption<bool>("CityLife.Movement.WaterAvoidance.Enable", true);

        // Configurable Activity Weights (Ported from mod-city-bots)
        _weightWanderDistrict = sConfigMgr->GetOption<uint32>("CityLife.Weight.WanderDistrict", 30);
        _weightSitInTavern = sConfigMgr->GetOption<uint32>("CityLife.Weight.SitInTavern", 20);
        _weightRandomEmote = sConfigMgr->GetOption<uint32>("CityLife.Weight.RandomEmote", 15);
        _weightVisitAuctionHouse = sConfigMgr->GetOption<uint32>("CityLife.Weight.VisitAuctionHouse", 10);
        _weightVisitMailbox = sConfigMgr->GetOption<uint32>("CityLife.Weight.VisitMailbox", 10);
        _weightBuyFood = sConfigMgr->GetOption<uint32>("CityLife.Weight.BuyFood", 10);
        _weightVisitTrainer = sConfigMgr->GetOption<uint32>("CityLife.Weight.VisitTrainer", 8);
        _weightDrinkInTavern = sConfigMgr->GetOption<uint32>("CityLife.Weight.DrinkInTavern", 10);
        _weightWalkCityWalls = sConfigMgr->GetOption<uint32>("CityLife.Weight.WalkCityWalls", 10);
        _weightFishAtDocks = sConfigMgr->GetOption<uint32>("CityLife.Weight.FishAtDocks", 5);
        _weightReturnToInn = sConfigMgr->GetOption<uint32>("CityLife.Weight.ReturnToInn", 15);

        ImportPlayerbotDefaults();
        CheckCoexistence();

        _botAccountCache.clear();
        RefreshAccountCache();
        _startupElapsedMs = 0;
        _timerMs = 0;

        _databaseReady = VerifyDatabase();
        if (_enable && _databaseReady)
            _databaseReady = LoadData();

        LOG_INFO("module", "[CityLife] Initialized: enable={} DB={} hubs={} residents={} budgetMode={} demandMode={}",
            _enable ? 1 : 0, _databaseReady ? "ready" : "missing", _hubs.size(), _residents.size(),
            budgetModeStr, demandModeStr);
    }

    void Manager::ImportPlayerbotDefaults()
    {
        if (!_usePlayerbotConfig)
            return;

        std::string configuredPrefix = sConfigMgr->GetOption<std::string>(
            "Playerbots.RandomBotAccountPrefix",
            sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotAccountPrefix", "rndbot", false),
            false);
        uint32 loginDelay = sConfigMgr->GetOption<uint32>(
            "Playerbots.DisabledWithoutRealPlayerLoginDelay",
            sConfigMgr->GetOption<uint32>("AiPlayerbot.DisabledWithoutRealPlayerLoginDelay", 30, false),
            false);
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
        CancelAdmissions();
        std::vector<Hub> hubs;

        bool hasZoneId = WorldDatabase.Query("SHOW COLUMNS FROM `city_life_hub` LIKE 'zone_id'") != nullptr;
        std::string hubQuery = hasZoneId
            ? "SELECT id,name,enabled,team,min_level,max_level,map_id,default_population,priority,zone_id FROM city_life_hub ORDER BY priority DESC,id ASC"
            : "SELECT id,name,enabled,team,min_level,max_level,map_id,default_population,priority FROM city_life_hub ORDER BY priority DESC,id ASC";

        QueryResult hubResult = WorldDatabase.Query(hubQuery);
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

                if (hasZoneId)
                    hub.ZoneId = fields[9].Get<uint32>();
                else
                    hub.ZoneId = 0;

                // Known Zone mapping fallback if zone_id was not explicitly configured
                std::string lowerName = Lower(hub.Name);
                if (hub.ZoneId == 0)
                {
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
                }

                hub.SocialHub = (lowerName == "goldshire");

                std::string configPrefix = "CityLife.Hub." + hub.Name;
                bool isWintergrasp = (lowerName.find("wintergrasp") != std::string::npos);
                uint32 defaultPop = isWintergrasp ? 0 : hub.DefaultPopulation;
                bool defaultEnable = isWintergrasp ? false : true;

                hub.ConfiguredPopulation = sConfigMgr->GetOption<uint32>(
                    configPrefix + ".Population", defaultPop, false);
                bool configEnabled = sConfigMgr->GetOption<bool>(
                    configPrefix + ".Enable", defaultEnable, false);
                hub.Enabled = hub.Enabled && configEnabled;
                hub.UseTimeScaling = sConfigMgr->GetOption<bool>(configPrefix + ".TimeScaling", !isWintergrasp, false);
                hub.ResidenceSeconds = sConfigMgr->GetOption<uint32>(
                    configPrefix + ".ResidenceSeconds", 0, false);
                hub.MinCount = sConfigMgr->GetOption<uint32>(
                    configPrefix + ".Min", 0, false);

                hubs.push_back(hub);
            } while (hubResult->NextRow());
        }

        // Check if extended columns exist in city_life_spot
        bool hasPoiType = WorldDatabase.Query("SHOW COLUMNS FROM `city_life_spot` LIKE 'poi_type'") != nullptr;
        bool hasDistrict = WorldDatabase.Query("SHOW COLUMNS FROM `city_life_spot` LIKE 'district'") != nullptr;
        bool hasPartner = WorldDatabase.Query("SHOW COLUMNS FROM `city_life_spot` LIKE 'partner_spot_id'") != nullptr;
        bool hasExtendedColumns = hasPoiType && hasDistrict && hasPartner;

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

        for (Hub& hub : hubs)
        {
            EnsureCanonicalFallbackSpots(hub);
        }

        {
            std::lock_guard<std::mutex> lock(_hubRealPlayerMutex);
            _zoneToHubIds.clear();
            for (Hub const& h : hubs)
            {
                if (h.ZoneId != 0)
                    _zoneToHubIds[h.ZoneId].push_back(h.Id);
            }
        }

        _hubs.swap(hubs);

        // Recompute per-hub resident counts
        _hubResidentCounts.clear();
        for (Resident const& r : _residents)
            ++_hubResidentCounts[r.HubId];

        // Release residents belonging to hubs that were disabled or deleted during reload
        for (auto const& resident : _residents)
        {
            Hub* hub = FindHub(resident.HubId);
            if (!hub || !hub->Enabled)
                ReleaseResident(resident, true);
        }

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

    void Manager::RefreshAccountCache()
    {
        std::string prefix = Lower(_botAccountPrefix);
        LoginDatabase.EscapeString(prefix);
        std::string sql = Acore::StringFormat(
            "SELECT id,username FROM account WHERE ({} > 0 AND id BETWEEN {} AND {}) OR "
            "({} > 0 AND LEFT(LOWER(username),{})='{}')",
            _botAccountMax, _botAccountMin, _botAccountMax, _botAccountPrefix.size(), _botAccountPrefix.size(), prefix);
        uint64 generation = _dataGeneration;
        _queryProcessor.AddCallback(LoginDatabase.AsyncQuery(sql).WithCallback([this, generation](QueryResult result)
        {
            if (generation != _dataGeneration || !result)
                return;
            do
            {
                uint32 id = result->Fetch()[0].Get<uint32>();
                bool range = _botAccountMax > 0 && id >= _botAccountMin && id <= _botAccountMax;
                bool prefixMatch = !_botAccountPrefix.empty() &&
                    Lower(result->Fetch()[1].Get<std::string>()).rfind(Lower(_botAccountPrefix), 0) == 0;
                _botAccountCache[id] = range || prefixMatch;
            } while (result->NextRow());
        }));
    }

    bool Manager::IsConfiguredBotAccount(uint32 accountId)
    {
        auto cached = _botAccountCache.find(accountId);
        if (cached != _botAccountCache.end())
            return cached->second;
        bool range = accountId && _botAccountMax > 0 && _botAccountMax >= _botAccountMin &&
            accountId >= _botAccountMin && accountId <= _botAccountMax;
        return range;
    }

    bool Manager::IsHumanControlled(Player* player) const
    {
        if (!player || !player->GetSession())
            return true;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai || ::IsRealPlayer(player) || ::IsSelfBot(player) || ai->HasGameClientMaster())
            return true;
        if (Group* group = player->GetGroup())
            for (GroupReference const* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource(); member && (::IsRealPlayer(member) || ::IsSelfBot(member)))
                    return true;
        return false;
    }

    bool Manager::IsCandidate(Player* player)
    {
        if (!player || !player->GetSession() || !player->IsInWorld() || player->IsBeingTeleported())
            return false;
        if (!sRandomPlayerbotMgr.IsRandomBot(player) &&
            !IsConfiguredBotAccount(player->GetSession()->GetAccountId()))
            return false;
        if (PlayerbotsLife::IsReserved(player->GetGUID().GetCounter()))
            return false;

        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (IsHumanControlled(player))
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
        if (!ai || ::IsRealPlayer(player) || ::IsSelfBot(player) || ai->HasGameClientMaster())
            return false;
        if (!player->IsAlive() || player->IsInFlight() || player->duel)
            return false;
        if (player->InBattleground() || player->InBattlegroundQueue())
            return false;
        if (player->GetMap() && player->GetMap()->Instanceable())
            return false;
        if (Group* group = player->GetGroup())
            for (GroupReference const* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource(); member && (::IsRealPlayer(member) || ::IsSelfBot(member)))
                    return false;
        if (_skipGroupedBots && player->GetGroup())
            return false;
        return true;
    }

    uint32 Manager::GetLiveWorldBotCount(bool forceRefresh) const
    {
        uint32 nowMs = GameTimeMS();
        if (!forceRefresh && _lastCensusTimeMs != 0 && (nowMs - _lastCensusTimeMs < 5000))
            return _cachedLiveBotCount;

        uint32 count = 0;
        std::vector<uint32> accountIdsToCheck;
        std::unordered_set<uint32> activeRealPlayerZones;

        {
            std::shared_lock<std::shared_mutex> playerLock(*HashMapHolder<Player>::GetLock());
            HashMapHolder<Player>::MapType const& players = ObjectAccessor::GetPlayers();
            accountIdsToCheck.reserve(players.size());
            for (auto const& pair : players)
            {
                Player* p = pair.second;
                if (!p || !p->GetSession() || !p->IsInWorld())
                    continue;

                if (::IsRealPlayer(p))
                {
                    activeRealPlayerZones.insert(p->GetZoneId());
                    continue;
                }

                if (IsHumanControlled(p) || !p->IsAlive() || p->InBattleground() || p->InBattlegroundQueue() ||
                    (p->GetMap() && p->GetMap()->Instanceable()))
                    continue;

                if (sRandomPlayerbotMgr.IsRandomBot(p))
                    ++count;
                else
                    accountIdsToCheck.push_back(p->GetSession()->GetAccountId());
            }
        }

        if (!activeRealPlayerZones.empty())
        {
            uint32 now = GameTimeSeconds();
            std::lock_guard<std::mutex> lock(_hubRealPlayerMutex);
            for (uint32 zoneId : activeRealPlayerZones)
            {
                auto it = _zoneToHubIds.find(zoneId);
                if (it != _zoneToHubIds.end())
                {
                    for (uint32 hubId : it->second)
                        _hubLastRealPlayerTime[hubId] = now;
                }
            }
        }

        for (uint32 accountId : accountIdsToCheck)
        {
            if (const_cast<Manager*>(this)->IsConfiguredBotAccount(accountId))
                ++count;
        }

        _cachedLiveBotCount = count;
        _lastCensusTimeMs = nowMs;
        return count;
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
            auto countIt = _hubResidentCounts.find(it->HubId);
            if (countIt != _hubResidentCounts.end() && countIt->second > 0)
                --countIt->second;

            ReleaseResident(*it, false); // Do not return-teleport away from group or master
            _residents.erase(it);

            if (_debug)
                LOG_INFO("module", "[CityLife] Disengaged resident {} on invite or control takeover.", guidLow);
        }
    }

    std::vector<BotCandidate> Manager::LoadCandidates(Hub const& hub, uint32 poolCount)
    {
        std::vector<BotCandidate> candidates;
        uint32 nowMs = GameTimeMS();
        if (_cachedOnlineGuids.empty() || (nowMs - _lastCandidateScanMs > 5000))
        {
            _cachedOnlineGuids.clear();
            std::shared_lock<std::shared_mutex> playerLock(*HashMapHolder<Player>::GetLock());
            HashMapHolder<Player>::MapType const& players = ObjectAccessor::GetPlayers();
            _cachedOnlineGuids.reserve(players.size());
            for (auto const& pair : players)
                if (pair.second)
                    _cachedOnlineGuids.push_back(pair.second->GetGUID().GetCounter());
            _lastCandidateScanMs = nowMs;
        }

        if (_cachedOnlineGuids.empty())
            return candidates;

        // Smart Hybrid Dynamic QueryLimit:
        // 1. Calculate dynamic census ceiling
        uint32 dynamicCeiling = _botQueryLimit;
        if (dynamicCeiling == 0)
        {
            uint32 liveBots = (poolCount > 0) ? poolCount : GetLiveWorldBotCount();
            dynamicCeiling = std::clamp<uint32>(liveBots / 2, 100, 1500);
        }

        // 2. Demand-driven early exit target
        uint32 currentCount = CountResidents(hub.Id) + static_cast<uint32>(std::count_if(
            _pendingAdmissions.begin(), _pendingAdmissions.end(),
            [&](PendingAdmission const& pending) { return pending.HubId == hub.Id; }));
        auto target = _hubTargets.find(hub.Id);
        uint32 targetPop = target == _hubTargets.end() ? TargetPopulation(hub) : target->second;
        uint32 deficit = (targetPop > currentCount) ? (targetPop - currentCount) : 1;
        uint32 candidateTarget = std::min<uint32>(dynamicCeiling, std::max<uint32>(20, deficit * 5));

        // 3. Rotational sampling across online bots: prioritize local zone and map bots
        std::vector<BotCandidate> localZoneCandidates;
        std::vector<BotCandidate> localMapCandidates;
        std::vector<BotCandidate> foreignCandidates;

        size_t n = _cachedOnlineGuids.size();
        size_t offset = urand(0, static_cast<uint32>(n - 1));

        for (size_t i = 0; i < n; ++i)
        {
            uint32 guidLow = _cachedOnlineGuids[(offset + i) % n];
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

            if (player->GetMapId() == hub.MapId && hub.ZoneId != 0 && player->GetZoneId() == hub.ZoneId)
                localZoneCandidates.push_back(candidate);
            else if (player->GetMapId() == hub.MapId)
                localMapCandidates.push_back(candidate);
            else
                foreignCandidates.push_back(candidate);

            if (localZoneCandidates.size() >= candidateTarget)
                break;
        }

        candidates = std::move(localZoneCandidates);
        if (candidates.size() < candidateTarget)
        {
            for (auto const& c : localMapCandidates)
            {
                candidates.push_back(c);
                if (candidates.size() >= candidateTarget)
                    break;
            }
        }
        if (candidates.size() < candidateTarget)
        {
            for (auto const& c : foreignCandidates)
            {
                candidates.push_back(c);
                if (candidates.size() >= candidateTarget)
                    break;
            }
        }

        for (size_t i = candidates.size(); i > 1; --i)
            std::swap(candidates[i - 1], candidates[urand(0, static_cast<uint32>(i - 1))]);

        return candidates;
    }

    Hub* Manager::FindHub(uint32 hubId)
    {
        auto found = std::find_if(_hubs.begin(), _hubs.end(),
            [&](Hub const& hub) { return hub.Id == hubId; });
        return found == _hubs.end() ? nullptr : &*found;
    }

    Hub const* Manager::FindHub(uint32 hubId) const
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

    Spot const* Manager::FindInnBedSpot(Hub const& hub) const
    {
        for (Spot const& spot : hub.Spots)
        {
            if (spot.PoiType == CityPoiType::InnBed || spot.Category == "inn" || spot.Category == "bed")
                return &spot;
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

    bool Manager::DistrictMatchesClass(std::string const& hubName, std::string const& district, uint8 botClass)
    {
        std::string h = Lower(hubName);
        std::string d = Lower(district);

        if (h == "stormwind")
        {
            switch (botClass)
            {
                case CLASS_MAGE:
                case CLASS_WARLOCK:
                    return d.find("mage") != std::string::npos;
                case CLASS_ROGUE:
                case CLASS_WARRIOR:
                case CLASS_DEATH_KNIGHT:
                    return d.find("old") != std::string::npos;
                case CLASS_PALADIN:
                case CLASS_PRIEST:
                    return d.find("cathedral") != std::string::npos;
                case CLASS_HUNTER:
                case CLASS_SHAMAN:
                    return d.find("dwarf") != std::string::npos;
                case CLASS_DRUID:
                    return d.find("park") != std::string::npos || d.find("cathedral") != std::string::npos;
                default:
                    return true;
            }
        }
        else if (h == "orgrimmar")
        {
            switch (botClass)
            {
                case CLASS_WARRIOR:
                case CLASS_ROGUE:
                case CLASS_DEATH_KNIGHT:
                    return d.find("strength") != std::string::npos;
                case CLASS_HUNTER:
                case CLASS_SHAMAN:
                case CLASS_PRIEST:
                case CLASS_MAGE:
                    return d.find("spirit") != std::string::npos;
                case CLASS_DRUID:
                    return d.find("wisdom") != std::string::npos;
                case CLASS_WARLOCK:
                    return d.find("cleft") != std::string::npos || d.find("shadow") != std::string::npos;
                default:
                    return true;
            }
        }
        else if (h == "ironforge")
        {
            switch (botClass)
            {
                case CLASS_WARRIOR:
                case CLASS_PALADIN:
                case CLASS_HUNTER:
                case CLASS_ROGUE:
                case CLASS_DEATH_KNIGHT:
                    return d.find("military") != std::string::npos || d.find("commons") != std::string::npos;
                case CLASS_MAGE:
                case CLASS_PRIEST:
                case CLASS_WARLOCK:
                    return d.find("mystic") != std::string::npos;
                default:
                    return true;
            }
        }
        else if (h == "undercity")
        {
            switch (botClass)
            {
                case CLASS_WARRIOR:
                case CLASS_ROGUE:
                case CLASS_DEATH_KNIGHT:
                    return d.find("war") != std::string::npos || d.find("center") != std::string::npos;
                case CLASS_MAGE:
                case CLASS_WARLOCK:
                case CLASS_PRIEST:
                    return d.find("magic") != std::string::npos || d.find("apothecary") != std::string::npos;
                default:
                    return true;
            }
        }
        else if (h == "darnassus")
        {
            switch (botClass)
            {
                case CLASS_WARRIOR:
                case CLASS_ROGUE:
                    return d.find("warrior") != std::string::npos;
                case CLASS_PRIEST:
                case CLASS_PALADIN:
                    return d.find("temple") != std::string::npos;
                case CLASS_DRUID:
                case CLASS_HUNTER:
                    return d.find("cenarion") != std::string::npos;
                default:
                    return true;
            }
        }
        return true;
    }

    void Manager::EnsureCanonicalFallbackSpots(Hub& hub)
    {
        std::string h = Lower(hub.Name);
        bool hasTrainers = std::any_of(hub.Spots.begin(), hub.Spots.end(),
            [](Spot const& s) { return s.PoiType == CityPoiType::Trainer; });
        bool hasDocks = std::any_of(hub.Spots.begin(), hub.Spots.end(),
            [](Spot const& s) { return s.PoiType == CityPoiType::Dock; });

        uint32 baseId = 90000 + hub.Id * 100;
        uint32 idx = 1;

        if (!hasTrainers)
        {
            if (h == "stormwind")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SW_Trainer_OldTown", "trainer", -8705.0f, 812.0f, 97.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "OldTown", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SW_Trainer_MageQuarter", "trainer", -9000.0f, 884.0f, 29.6f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "MageQuarter", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SW_Trainer_DwarvenDistrict", "trainer", -8425.0f, 610.0f, 95.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "DwarvenDistrict", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SW_Trainer_Cathedral", "trainer", -8575.0f, 880.0f, 106.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "CathedralSquare", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SW_Trainer_Trade", "trainer", -8780.0f, 610.0f, 97.0f, 0.0f, 5.0f, 100, CityPoiType::Trainer, "Trade", 0 });
            }
            else if (h == "orgrimmar")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "ORG_Trainer_Strength", "trainer", 1720.0f, -4310.0f, 33.0f, 0.0f, 5.0f, 100, CityPoiType::Trainer, "ValleyOfStrength", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "ORG_Trainer_Spirits", "trainer", 1450.0f, -4185.0f, 44.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "ValleyOfSpirits", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "ORG_Trainer_Wisdom", "trainer", 1938.0f, -4675.0f, 32.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "ValleyOfWisdom", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "ORG_Trainer_Shadow", "trainer", 1835.0f, -4370.0f, -16.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "CleftOfShadow", 0 });
            }
            else if (h == "ironforge")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "IF_Trainer_Military", "trainer", -5010.0f, -1250.0f, 501.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "MilitaryWard", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "IF_Trainer_Mystic", "trainer", -4650.0f, -960.0f, 501.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "MysticWard", 0 });
            }
            else if (h == "undercity")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "UC_Trainer_War", "trainer", 1700.0f, 90.0f, -62.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "WarQuarter", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "UC_Trainer_Magic", "trainer", 1660.0f, 210.0f, -43.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "MagicQuarter", 0 });
            }
            else if (h == "darnassus")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "DAR_Trainer_Warriors", "trainer", 10040.0f, 2530.0f, 1316.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "WarriorsTerrace", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "DAR_Trainer_Temple", "trainer", 9705.0f, 2510.0f, 1335.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "TempleGardens", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "DAR_Trainer_Enclave", "trainer", 10080.0f, 2205.0f, 1328.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "CenarionEnclave", 0 });
            }
            else if (h == "exodar")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "EXO_Trainer_Traders", "trainer", -3860.0f, -11695.0f, -137.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "TradersTier", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "EXO_Trainer_Trainers", "trainer", -4215.0f, -11740.0f, -139.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "Trainers", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "EXO_Trainer_Vault", "trainer", -4100.0f, -11830.0f, -106.0f, 0.0f, 5.0f, 70, CityPoiType::Trainer, "Vault", 0 });
            }
            else if (h == "thunderbluff")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "TB_Trainer_Hunter", "trainer", -1050.0f, 250.0f, 135.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "HunterRise", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "TB_Trainer_Spirit", "trainer", -1425.0f, -55.0f, 158.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "SpiritRise", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "TB_Trainer_Central", "trainer", -1110.0f, -35.0f, 132.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "CentralRise", 0 });
            }
            else if (h == "silvermoon")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SMC_Trainer_Royal", "trainer", 9605.0f, -7380.0f, 14.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "RoyalExchange", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SMC_Trainer_Murder", "trainer", 9800.0f, -7335.0f, 14.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "MurderRow", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SMC_Trainer_Farstriders", "trainer", 9900.0f, -7420.0f, 14.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "FarstridersSquare", 0 });
            }
            else if (h == "shattrath")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SHAT_Trainer_Scryer", "trainer", -2000.0f, 5350.0f, -8.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "ScryerRise", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SHAT_Trainer_Aldor", "trainer", -1745.0f, 5760.0f, 130.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "AldorRise", 0 });
            }
            else if (h == "dalaran")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "DAL_Trainer_Krasus", "trainer", 5775.0f, 620.0f, 650.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "Krasus", 0 });
                hub.Spots.push_back({ baseId + idx++, hub.Id, "DAL_Trainer_Violet", "trainer", 5810.0f, 760.0f, 640.0f, 0.0f, 5.0f, 80, CityPoiType::Trainer, "VioletCitadel", 0 });
            }
        }

        if (!hasDocks)
        {
            if (h == "stormwind")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "SW_Dock_Harbor", "dock", -8642.0f, 1336.0f, 5.3f, 3.14f, 4.0f, 80, CityPoiType::Dock, "StormwindHarbor", 0 });
            }
            else if (h == "orgrimmar")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "ORG_Dock_Spirits", "dock", 1960.0f, -4180.0f, 25.0f, 0.0f, 4.0f, 80, CityPoiType::Dock, "ValleyOfSpirits", 0 });
            }
            else if (h == "ironforge")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "IF_Dock_Cavern", "dock", -4820.0f, -1150.0f, 502.0f, 0.0f, 4.0f, 80, CityPoiType::Dock, "ForlornCavern", 0 });
            }
            else if (h == "undercity")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "UC_Dock_Canal", "dock", 1670.0f, 310.0f, -60.0f, 0.0f, 4.0f, 80, CityPoiType::Dock, "Canal", 0 });
            }
            else if (h == "darnassus")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "DAR_Dock_Ruttheran", "dock", 9870.0f, 2580.0f, 1316.0f, 0.0f, 4.0f, 80, CityPoiType::Dock, "Ruttheran", 0 });
            }
            else if (h == "thunderbluff")
            {
                hub.Spots.push_back({ baseId + idx++, hub.Id, "TB_Dock_Lower", "dock", -1300.0f, -20.0f, 125.0f, 0.0f, 4.0f, 80, CityPoiType::Dock, "LowerPond", 0 });
            }
        }
    }

    Spot const* Manager::ChooseSpot(Hub const& hub, uint32 ignoredSpotId, bool filterByDistrict, bool targetHubDistrict, Player* botForPathCheck) const
    {
        Spot const* best = nullptr;
        uint64 bestScore = std::numeric_limits<uint64>::max();
        bool hasAlternative = hub.Spots.size() > 1;

        Spot const* currentSpot = nullptr;
        if (ignoredSpotId != 0)
        {
            for (Spot const& s : hub.Spots)
            {
                if (s.Id == ignoredSpotId)
                {
                    currentSpot = &s;
                    break;
                }
            }
        }

        auto passesInnFilter = [&](Spot const& spot)
        {
            if (!currentSpot)
                return true;
            bool currentIsInInn = (currentSpot->Category == "inn" || currentSpot->PoiType == CityPoiType::Tavern);
            bool candIsInInn = (spot.Category == "inn" || spot.PoiType == CityPoiType::Tavern);
            if (currentIsInInn && !candIsInInn && !_actLeaveInn)
                return false;
            if (!currentIsInInn && candIsInInn && !_actReturnToInn)
                return false;
            return true;
        };

        std::vector<Spot const*> candidateList;
        if (filterByDistrict)
        {
            for (Spot const& spot : hub.Spots)
            {
                if (hasAlternative && spot.Id == ignoredSpotId)
                    continue;

                if (!passesInnFilter(spot))
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

                if (!passesInnFilter(spot))
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
        if (spot.PoiType == CityPoiType::Dock && _actFishing)
        {
            Player* p = FindPlayer(candidate.GuidLow);
            Item* pole = p ? p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND) : nullptr;
            if (p && p->GetSkillValue(SKILL_FISHING) > 0 && pole &&
                pole->GetTemplate()->Class == ITEM_CLASS_WEAPON &&
                pole->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
            {
                Item* mainItem = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                if (mainItem && mainItem->GetTemplate() &&
                    mainItem->GetTemplate()->Class == ITEM_CLASS_WEAPON &&
                    mainItem->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
                {
                    return CitizenRole::Fisher;
                }
            }
            return CitizenRole::Fisher;
        }

        if (Lower(hub.Name) == "goldshire" && (spot.District == "LionsPrideUp" || spot.Name.find("Dance") != std::string::npos || spot.Category == "inn") && _actInnDancing)
            return CitizenRole::InnDancer;

        if (spot.PoiType == CityPoiType::Plaza && _actPlazaSocial)
            return CitizenRole::PlazaTalker;

        if (spot.PoiType == CityPoiType::Bank && _actCrowdScenes && _crowdEnable)
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
            case CitizenRole::Crowd:
                return CitizenActivity::CrowdSocial;
            default:
                break;
        }

        switch (spot.PoiType)
        {
            case CityPoiType::Inn:
            case CityPoiType::Tavern:
                if (_actTavernSitting && _actTavernDrinking)
                    return roll_chance_i(50) ? CitizenActivity::DrinkInTavern : CitizenActivity::SitInTavern;
                if (_actTavernDrinking)
                    return CitizenActivity::DrinkInTavern;
                return _actTavernSitting ? CitizenActivity::SitInTavern : CitizenActivity::Idle;
            case CityPoiType::InnBed:
                return CitizenActivity::ReleaseCycle;
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
            case CityPoiType::Bank:
                if (_actCrowdScenes && _crowdEnable)
                    return CitizenActivity::CrowdSocial;
                return _actAuctionHouse ? CitizenActivity::VisitAuctionHouse : CitizenActivity::Idle;
            case CityPoiType::Plaza:
                return _actPlazaSocial ? CitizenActivity::PlazaSocial : CitizenActivity::Idle;
            case CityPoiType::Dock:
                return _actFishing ? CitizenActivity::FishAtDocks : (_actRoaming ? CitizenActivity::WanderDistrict : CitizenActivity::Idle);
            default:
                return _actRoaming ? CitizenActivity::WanderDistrict : CitizenActivity::RandomEmote;
        }
    }

    CitizenActivity Manager::PickNextActivity(Resident const& resident, Hub const& hub) const
    {
        if (resident.Role != CitizenRole::Flex)
            return resident.Activity;

        struct ActivityWeightOption
        {
            CitizenActivity Activity;
            uint32 Weight;
            bool Enabled;
            CityPoiType RequiredPoi;
        };

        ActivityWeightOption pool[] = {
            { CitizenActivity::WanderDistrict,     _weightWanderDistrict,     _actRoaming,        CityPoiType::Plaza },
            { CitizenActivity::SitInTavern,         _weightSitInTavern,         _actTavernSitting,  CityPoiType::Tavern },
            { CitizenActivity::DrinkInTavern,       _weightDrinkInTavern,       _actTavernDrinking, CityPoiType::Tavern },
            { CitizenActivity::RandomEmote,         _weightRandomEmote,         _actRandomEmotes,   CityPoiType::Plaza },
            { CitizenActivity::VisitAuctionHouse,   _weightVisitAuctionHouse,   _actAuctionHouse,   CityPoiType::AuctionHouse },
            { CitizenActivity::VisitMailbox,        _weightVisitMailbox,        _actMailbox,        CityPoiType::Mailbox },
            { CitizenActivity::BuyFood,             _weightBuyFood,             _actFoodVendor,     CityPoiType::Vendor },
            { CitizenActivity::VisitTrainer,        _weightVisitTrainer,        _actTrainer,        CityPoiType::Trainer },
            { CitizenActivity::WalkCityWalls,       _weightWalkCityWalls,       _actWallWalking,    CityPoiType::WallPatrol },
            { CitizenActivity::FishAtDocks,         _weightFishAtDocks,         _actFishing,        CityPoiType::Dock },
            { CitizenActivity::ReturnToInn,         _weightReturnToInn,         _actReturnToInn,    CityPoiType::Inn }
        };

        std::vector<CitizenActivity> candidates;
        for (auto const& item : pool)
        {
            if (!item.Enabled || item.Weight == 0)
                continue;

            // Don't repeat the exact same activity immediately if other options exist
            if (item.Activity == resident.Activity)
                continue;

            bool hasPoi = std::any_of(hub.Spots.begin(), hub.Spots.end(),
                [&](Spot const& s) {
                    return s.PoiType == item.RequiredPoi || (item.RequiredPoi == CityPoiType::Tavern && s.PoiType == CityPoiType::Inn);
                });

            if (!hasPoi && item.RequiredPoi != CityPoiType::Plaza)
                continue;

            for (uint32 i = 0; i < item.Weight; ++i)
                candidates.push_back(item.Activity);
        }

        if (candidates.empty())
            return CitizenActivity::WanderDistrict;

        return candidates[urand(0, static_cast<uint32>(candidates.size()) - 1)];
    }

    Spot const* Manager::ChooseSpotForActivity(Hub const& hub, CitizenActivity activity, uint32 ignoredSpotId, Player* botForPathCheck) const
    {
        CityPoiType targetType = CityPoiType::Plaza;
        switch (activity)
        {
            case CitizenActivity::VisitAuctionHouse:
                targetType = CityPoiType::AuctionHouse;
                break;
            case CitizenActivity::VisitMailbox:
                targetType = CityPoiType::Mailbox;
                break;
            case CitizenActivity::VisitTrainer:
                targetType = CityPoiType::Trainer;
                break;
            case CitizenActivity::BuyFood:
                targetType = CityPoiType::Vendor;
                break;
            case CitizenActivity::WalkCityWalls:
                targetType = CityPoiType::WallPatrol;
                break;
            case CitizenActivity::FishAtDocks:
                targetType = CityPoiType::Dock;
                break;
            case CitizenActivity::SitInTavern:
            case CitizenActivity::DrinkInTavern:
                targetType = CityPoiType::Tavern;
                break;
            case CitizenActivity::ReturnToInn:
                targetType = CityPoiType::Inn;
                break;
            case CitizenActivity::WanderDistrict:
            case CitizenActivity::RandomEmote:
            case CitizenActivity::PlazaSocial:
            default:
                targetType = CityPoiType::Plaza;
                break;
        }

        std::vector<Spot const*> matchingSpots;
        for (Spot const& spot : hub.Spots)
        {
            if (spot.Id == ignoredSpotId)
                continue;

            if (spot.PoiType == targetType || (targetType == CityPoiType::Tavern && spot.PoiType == CityPoiType::Inn))
                matchingSpots.push_back(&spot);
        }

        if (targetType == CityPoiType::Trainer && botForPathCheck)
        {
            std::vector<Spot const*> classSpots;
            uint8 bClass = botForPathCheck->getClass();
            for (Spot const* sp : matchingSpots)
            {
                if (DistrictMatchesClass(hub.Name, sp->District, bClass) ||
                    DistrictMatchesClass(hub.Name, sp->Name, bClass))
                {
                    classSpots.push_back(sp);
                }
            }
            if (!classSpots.empty())
                matchingSpots = std::move(classSpots);
        }

        if (matchingSpots.empty() && targetType == CityPoiType::Trainer)
        {
            for (Spot const& spot : hub.Spots)
            {
                if (spot.PoiType == CityPoiType::Trainer)
                    matchingSpots.push_back(&spot);
            }
        }

        if (matchingSpots.empty())
            return ChooseSpot(hub, ignoredSpotId, false, false, botForPathCheck);

        Spot const* best = nullptr;
        uint64 bestScore = std::numeric_limits<uint64>::max();

        for (Spot const* spotPtr : matchingSpots)
        {
            Spot const& spot = *spotPtr;
            if (_waterAvoidance && botForPathCheck && spot.PoiType != CityPoiType::Dock)
            {
                if (!Movement::HasCompleteWalkablePath(botForPathCheck, spot.X, spot.Y, spot.Z, false))
                    continue;
            }

            uint32 occupants = static_cast<uint32>(std::count_if(_residents.begin(), _residents.end(),
                [&](Resident const& r) { return r.HubId == hub.Id && r.SpotId == spot.Id; }));

            uint64 score = (static_cast<uint64>(occupants) + 1) * 10000 / std::max<uint32>(1, spot.Weight) + urand(0, 50);
            if (score < bestScore)
            {
                bestScore = score;
                best = &spot;
            }
        }

        if (!best && !matchingSpots.empty())
            best = matchingSpots[urand(0, static_cast<uint32>(matchingSpots.size()) - 1)];

        return best ? best : ChooseSpot(hub, ignoredSpotId, false, false, botForPathCheck);
    }

    uint32 Manager::CountResidents(uint32 hubId) const
    {
        auto it = _hubResidentCounts.find(hubId);
        return it != _hubResidentCounts.end() ? it->second : 0;
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

        if (_maxTotalPopulation > 0)
            return std::min({ requested, _maxTotalPopulation, eligiblePoolCount });
        return std::min(requested, eligiblePoolCount);
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

        if (_demandMode == DemandMode::Fixed)
            return nominal;

        // Dynamic or Hybrid demand evaluation
        uint32 now = GameTimeSeconds();
        bool playerPresent = false;

        {
            std::lock_guard<std::mutex> lock(_hubRealPlayerMutex);
            auto lastSeenIt = _hubLastRealPlayerTime.find(hub.Id);
            if (lastSeenIt != _hubLastRealPlayerTime.end())
            {
                if (now >= lastSeenIt->second && (now - lastSeenIt->second) <= _burstGraceSeconds)
                    playerPresent = true;
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

        std::lock_guard<std::mutex> lock(_hubRealPlayerMutex);
        auto it = _zoneToHubIds.find(zoneId);
        if (it != _zoneToHubIds.end())
        {
            uint32 now = GameTimeSeconds();
            for (uint32 hubId : it->second)
                _hubLastRealPlayerTime[hubId] = now;
        }
    }

    void Manager::OnPlayerLogin(Player* player)
    {
        if (!player || !player->GetSession() || player->GetSession()->IsBot())
            return;

        std::lock_guard<std::mutex> lock(_hubRealPlayerMutex);
        auto it = _zoneToHubIds.find(player->GetZoneId());
        if (it != _zoneToHubIds.end())
        {
            uint32 now = GameTimeSeconds();
            for (uint32 hubId : it->second)
                _hubLastRealPlayerTime[hubId] = now;
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
        for (ReleasingResident& pending : _releasingResidents)
            if (pending.Actor.Bot.GuidLow == guidLow)
                return &pending.Actor;
        return nullptr;
    }

    Resident const* Manager::FindResident(uint32 guidLow) const
    {
        for (Resident const& r : _residents)
        {
            if (r.Bot.GuidLow == guidLow)
                return &r;
        }
        for (ReleasingResident const& pending : _releasingResidents)
            if (pending.Actor.Bot.GuidLow == guidLow)
                return &pending.Actor;
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

        resident.Role = DetermineRole(hub, spot, resident.Bot);
        if (resident.Activity != CitizenActivity::ReleaseCycle)
        {
            resident.Activity = ChooseInitialActivity(resident.Role, spot);
        }

        // Dedicated venue performers (inn dancers, fishers, bed sleepers) remain stationary at their venue
        bool isDedicatedPerformer = (resident.Role == CitizenRole::InnDancer ||
                                     resident.Role == CitizenRole::Fisher ||
                                     resident.Activity == CitizenActivity::ReleaseCycle ||
                                     spot.PoiType == CityPoiType::InnBed);

        resident.Stationary = isDedicatedPerformer || (allowStationary && IsStaticServiceSpot(hub, spot) &&
            urand(1, 100) <= _staticServiceChance);
        resident.ActivityStartMs = GameTimeMS();
        resident.ActivityDurationMs = urand(45, 180) * 1000;
        resident.LastRoamWasHub = IsHubDistrict(hub.Name, spot.District);
    }

    bool Manager::AddResident(Hub const& hub, uint32 poolCount)
    {
        if (std::any_of(_pendingAdmissions.begin(), _pendingAdmissions.end(),
            [&](PendingAdmission const& pending) { return pending.HubId == hub.Id; }))
            return false;
        std::vector<BotCandidate> candidates = LoadCandidates(hub, poolCount);
        if (candidates.empty())
            return false;
        uint32 now = GameTimeSeconds();
        PendingAdmission pending;
        pending.HubId = hub.Id;
        pending.Bot = candidates.front();
        pending.Deadline = now + std::max<uint32>(30, _tickSeconds * 3);
        pending.RequestId = PlayerbotsLife::SubmitAdmissionRequest(PlayerbotsLife::ReservationOwner::CityLife,
            ++_nextAssignmentId, { pending.Bot.GuidLow }, CalculateTotalBudget(poolCount), now, pending.Deadline);
        if (!pending.RequestId)
            return false;
        _pendingAdmissions.push_back(pending);
        return true;
    }

    bool Manager::AdmitResident(Hub const& hub, BotCandidate const& bot, PlayerbotsLife::LeaseToken token)
    {
        Player* player = FindPlayer(bot.GuidLow);
        Spot const* spot = ChooseSpot(hub);
        if (!hub.Enabled || !spot || !IsResidentUsable(player) || player->IsInCombat() ||
            player->IsBeingTeleported() || token.GuidLow != bot.GuidLow ||
            token.Owner != PlayerbotsLife::ReservationOwner::CityLife ||
            !PlayerbotsLife::ValidateLeaseState(token, PlayerbotsLife::LeaseState::Provisional) ||
            !PlayerbotsLife::PromoteLease(token))
            return false;
        Resident resident;
        resident.Bot = bot;
        resident.Token = token;
        resident.OriginalMap = player->GetMapId();
        resident.OriginalX = player->GetPositionX();
        resident.OriginalY = player->GetPositionY();
        resident.OriginalZ = player->GetPositionZ();
        resident.OriginalO = player->GetOrientation();
        AssignSpot(resident, hub, *spot, true);
        if (!Movement::ValidateCityDestination(player, hub.MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, spot->PoiType == CityPoiType::Dock))
        {
            if (_debug)
                LOG_WARN("module", "[CityLife] AdmitResident: Spot '{}/{}' rejected by ValidateCityDestination.", hub.Name, spot->Name);
            return false;
        }

        uint32 now = GameTimeSeconds();
        resident.NextMoveAt = now + urand(_relocateMinSeconds, _relocateMaxSeconds);
        resident.NextEmoteAtMs = GameTimeMS() + urand(_emoteMinSeconds, _emoteMaxSeconds) * 1000;
        resident.NextReservationRefreshAt = now + ReservationRefreshSeconds;
        resident.ReleaseAt = hub.ResidenceSeconds == 0 ? 0 : now + hub.ResidenceSeconds;
        resident.LastSweepZ = resident.TargetZ;
        ApplyCityStrategies(player, resident);
        SetStayPosition(player, resident);
        if (sRandomPlayerbotMgr.IsRandomBot(player))
            sRandomPlayerbotMgr.ScheduleTeleport(bot.GuidLow, std::max(ReservationSeconds, hub.ResidenceSeconds + MINUTE));
        Movement::MoveToPosition(player, hub.MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, true);
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
            ai->HandleTeleportAck();
        _residents.push_back(resident);
        ++_hubResidentCounts[resident.HubId];
        ++_changesThisTick;
        return true;
    }

    void Manager::CancelAdmissions()
    {
        PlayerbotsLife::CancelAdmissionRequest(0, PlayerbotsLife::ReservationOwner::CityLife);
        for (PendingAdmission const& pending : _pendingAdmissions)
            PlayerbotsLife::ReleaseLeaseExact(pending.Token);
        _pendingAdmissions.clear();
    }

    void Manager::ProcessAdmissionGrants()
    {
        uint32 now = GameTimeSeconds();
        auto pending = _pendingAdmissions.begin();
        while (pending != _pendingAdmissions.end())
        {
            Hub* hub = FindHub(pending->HubId);
            auto target = _hubTargets.find(pending->HubId);
            if (!hub || !hub->Enabled || pending->Deadline <= now ||
                target == _hubTargets.end() || CountResidents(pending->HubId) >= target->second)
            {
                PlayerbotsLife::CancelAdmissionRequest(pending->RequestId, PlayerbotsLife::ReservationOwner::CityLife);
                PlayerbotsLife::ReleaseLeaseExact(pending->Token);
                pending = _pendingAdmissions.erase(pending);
                continue;
            }
            if (!pending->Token.Generation)
            {
                std::vector<PlayerbotsLife::LeaseToken> tokens;
                if (!PlayerbotsLife::TakeAdmissionGrant(pending->RequestId, PlayerbotsLife::ReservationOwner::CityLife, tokens))
                {
                    ++pending;
                    continue;
                }
                if (tokens.size() != 1)
                {
                    for (auto const& token : tokens)
                        PlayerbotsLife::ReleaseLeaseExact(token);
                    pending = _pendingAdmissions.erase(pending);
                    continue;
                }
                pending->Token = tokens.front();
            }
            if (_changesThisTick >= _maxChangesPerTick)
            {
                ++pending;
                continue;
            }
            uint32 budget = CalculateTotalBudget(GetLiveWorldBotCount());
            if (PlayerbotsLife::GetCooperationSnapshot().CityLifeCount > budget ||
                !AdmitResident(*hub, pending->Bot, pending->Token))
                PlayerbotsLife::ReleaseLeaseExact(pending->Token);
            pending = _pendingAdmissions.erase(pending);
        }
    }

    void Manager::ReleaseResident(Resident const& resident, bool returnHome)
    {
        if (!PlayerbotsLife::BeginRelease(resident.Token))
            return;
        if (std::none_of(_releasingResidents.begin(), _releasingResidents.end(),
            [&](ReleasingResident const& pending) { return pending.Actor.Token.Generation == resident.Token.Generation; }))
        {
            _releasingResidents.push_back({ resident, returnHome, GameTimeSeconds(), 0 });
            _releasingDirty = true;
        }
    }

    void Manager::ProcessReleasingResidents(bool isShutdown)
    {
        uint32 now = GameTimeSeconds();
        auto pending = _releasingResidents.begin();
        while (pending != _releasingResidents.end())
        {
            Resident& resident = pending->Actor;
            ++pending->Attempts;
            bool owned = PlayerbotsLife::ValidateLeaseState(resident.Token, PlayerbotsLife::LeaseState::Releasing);
            Player* player = owned ? FindPlayer(resident.Bot.GuidLow) : nullptr;
            bool isHuman = player && IsHumanControlled(player);
            bool timedOut = (pending->ReleaseStartedAt != 0 && (now - pending->ReleaseStartedAt >= 30)) || (pending->Attempts > 15);
            bool relinquish = !owned || !player || isHuman || isShutdown || timedOut;

            if (!relinquish)
            {
                if (!player->IsInWorld() || player->IsBeingTeleported() || _changesThisTick >= _maxChangesPerTick)
                {
                    ++pending;
                    continue;
                }
                if (player->IsInCombat())
                {
                    ++pending;
                    continue;
                }
                Activities::CrowdClearAfk(player);
                Activities::CrowdReleaseChat(player, resident);
                RestoreResidentState(player, resident);
                if (sRandomPlayerbotMgr.IsRandomBot(player))
                    sRandomPlayerbotMgr.ScheduleTeleport(resident.Bot.GuidLow);
                if (pending->ReturnHome && player->IsAlive())
                    Movement::MoveToPosition(player, resident.OriginalMap, resident.OriginalX,
                        resident.OriginalY, resident.OriginalZ, resident.OriginalO, true);
                ++_changesThisTick;
            }
            else
            {
                // Terminal fallback / takeover / timeout / shutdown path
                if (player && !isHuman)
                {
                    Activities::CrowdClearAfk(player);
                    Activities::CrowdReleaseChat(player, resident);
                    RestoreResidentState(player, resident);
                    if (!player->IsInCombat() && !player->IsBeingTeleported() && pending->ReturnHome && player->IsAlive())
                        Movement::MoveToPosition(player, resident.OriginalMap, resident.OriginalX,
                            resident.OriginalY, resident.OriginalZ, resident.OriginalO, true);
                }
                // If human-controlled: DO NOT touch player strategies, state, or teleports!
            }

            if (owned)
                PlayerbotsLife::ReleaseLeaseExact(resident.Token);
            pending = _releasingResidents.erase(pending);
        }
    }

    void Manager::ReleaseAll(bool returnHome)
    {
        CancelAdmissions();
        for (Resident const& resident : _residents)
            ReleaseResident(resident, returnHome && _returnBots);
        _residents.clear();
        _hubResidentCounts.clear();
    }

    void Manager::Shutdown()
    {
        ++_dataGeneration;
        ReleaseAll(true);
        ProcessReleasingResidents(true);
        _releasingResidents.clear();
    }

    void Manager::RemoveUnavailableResidents()
    {
        uint32 now = GameTimeSeconds();
        uint32 nowMs = GameTimeMS();
        auto resident = _residents.begin();
        while (resident != _residents.end())
        {
            Player* player = FindPlayer(resident->Bot.GuidLow);
            Hub* hub = FindHub(resident->HubId);
            bool residenceExpired = resident->ReleaseAt != 0 && resident->ReleaseAt <= now && player &&
                !player->IsInCombat() && !player->IsBeingTeleported();

            // Rest & Sleep Sequence: if expired and not yet resting, transition to ReleaseCycle at inn bed
            if (residenceExpired && resident->Activity != CitizenActivity::ReleaseCycle && _actReturnToInn && hub)
            {
                if (Spot const* restSpot = FindInnBedSpot(*hub))
                {
                    resident->Activity = CitizenActivity::ReleaseCycle;
                    resident->ActivityStartMs = nowMs;
                    resident->ReleaseAt = now + 20; // 20s rest in bed before final release
                    resident->Stationary = true;
                    AssignSpot(*resident, *hub, *restSpot, true);
                    resident->Activity = CitizenActivity::ReleaseCycle;
                    Movement::MoveToPosition(player, hub->MapId, restSpot->X, restSpot->Y, restSpot->Z, restSpot->O, false);
                    ++resident;
                    continue;
                }
            }

            if (!hub || !IsResidentUsable(player) || residenceExpired)
            {
                if (player && player->getStandState() == UNIT_STAND_STATE_SLEEP)
                    player->SetStandState(UNIT_STAND_STATE_STAND);
                if (player)
                    player->HandleEmoteCommand(EMOTE_ONESHOT_NONE);

                auto countIt = _hubResidentCounts.find(resident->HubId);
                if (countIt != _hubResidentCounts.end() && countIt->second > 0)
                    --countIt->second;

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

        // Live World Bot Sensor: measure exact active random bots in the world
        uint32 eligiblePoolCount = GetLiveWorldBotCount();
        uint32 totalBudget = CalculateTotalBudget(eligiblePoolCount);

        std::unordered_map<uint32, uint32> targets;
        if (_distributionMode == DistributionMode::Scaled)
        {
            std::vector<PopulationWeight> weights;
            uint64 baseWeight = 0;
            uint64 effectiveWeight = 0;
            for (Hub const& hub : _hubs)
            {
                targets[hub.Id] = 0;
                if (!hub.Enabled || hub.Spots.empty())
                    continue;
                uint32 demand = TargetPopulation(hub);
                weights.push_back({ hub.Id, demand, hub.MinCount, hub.Priority });
                baseWeight += hub.ConfiguredPopulation;
                effectiveWeight += demand;
            }
            double factor = baseWeight ? static_cast<double>(effectiveWeight) / baseWeight : 1.0;
            uint32 requested = effectiveWeight ? static_cast<uint32>(
                std::lround(totalBudget * std::min(1.0, factor))) : 0;
            auto allocation = AllocatePopulation(weights, requested);
            for (size_t index = 0; index < weights.size(); ++index)
                targets[weights[index].HubId] = allocation[index];
        }
        else // Legacy sequential allocation
        {
            uint32 remaining = totalBudget;
            for (Hub const& hub : _hubs)
            {
                uint32 target = std::min(TargetPopulation(hub), remaining);
                targets[hub.Id] = target;
                remaining -= target;
            }
        }

        _hubTargets = targets;
        uint32 changes = _changesThisTick;
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
                            !player->IsBeingTeleported() && !player->duel;
                    });
                if (found == _residents.rend())
                    break;
                auto eraseAt = std::next(found).base();
                auto countIt = _hubResidentCounts.find(eraseAt->HubId);
                if (countIt != _hubResidentCounts.end() && countIt->second > 0)
                    --countIt->second;
                ReleaseResident(*eraseAt, _returnBots);
                _residents.erase(eraseAt);
                ++changes;
                ++_changesThisTick;
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
                uint32 current = CountResidents(hub.Id) + static_cast<uint32>(std::count_if(
                    _pendingAdmissions.begin(), _pendingAdmissions.end(),
                    [&](PendingAdmission const& pending) { return pending.HubId == hub.Id; }));
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
            if (AddResident(*selectedHub, eligiblePoolCount))
            {
                ++changes;
                continue;
            }

            blockedHubs.insert(selectedHub->Id);
        }
    }

    void Manager::RelocateResident(Resident& resident, Hub const& hub)
    {
        if (resident.Stationary ||
            resident.Activity == CitizenActivity::ReleaseCycle ||
            resident.Activity == CitizenActivity::InnDance ||
            resident.Activity == CitizenActivity::FishAtDocks ||
            resident.Role == CitizenRole::InnDancer ||
            resident.Role == CitizenRole::Fisher)
        {
            return;
        }

        Player* player = FindPlayer(resident.Bot.GuidLow);
        if (!player)
            return;

        CitizenActivity nextActivity = PickNextActivity(resident, hub);
        Spot const* spot = ChooseSpotForActivity(hub, nextActivity, resident.SpotId, player);

        if (!spot && _alternatingDistricts && UsesAlternatingHubRoute(hub.Name))
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

        if (resident.Sitting || player->getStandState() != UNIT_STAND_STATE_STAND)
        {
            player->SetStandState(UNIT_STAND_STATE_STAND);
            resident.Sitting = false;
        }
        player->HandleEmoteCommand(EMOTE_ONESHOT_NONE);

        AssignSpot(resident, hub, *spot, false);
        resident.Activity = nextActivity;
        resident.NextMoveAt = GameTimeSeconds() + urand(_relocateMinSeconds, _relocateMaxSeconds);
        resident.ActivityStartMs = GameTimeMS();
        resident.ActivityDurationMs = urand(45, 180) * 1000;

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
            if (!hub || !player ||
                !PlayerbotsLife::ValidateActiveLease(resident.Token) ||
                !IsResidentUsable(player))
                continue;

            if (player->IsBeingTeleported())
            {
                if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
                    ai->HandleTeleportAck();
                continue;
            }

            if ((!_activitiesAfkEnable || !_crowdEnable || !_actCrowdScenes) && resident.AfkSetByModule)
                Activities::CrowdClearAfk(player);
            // Reservation refresh
            if (resident.NextReservationRefreshAt <= nowSec && sRandomPlayerbotMgr.IsRandomBot(player))
            {
                sRandomPlayerbotMgr.ScheduleTeleport(resident.Bot.GuidLow,
                    std::max(ReservationSeconds, hub->ResidenceSeconds + MINUTE));
                resident.NextReservationRefreshAt = nowSec + ReservationRefreshSeconds;
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

            // Foreign map/zone or dedicated performer positioning
            bool const inForeignZone = (player->GetMapId() != hub->MapId) ||
                                       (hub->ZoneId != 0 && player->GetZoneId() != hub->ZoneId);
            bool const isDedicatedPerformer = (resident.Role == CitizenRole::Fisher ||
                                               resident.Role == CitizenRole::InnDancer);
            float const distToTarget = player->GetDistance2d(resident.TargetX, resident.TargetY);

            if (inForeignZone || (isDedicatedPerformer && distToTarget > 20.0f))
            {
                ApplyCityStrategies(player, resident);
                SetStayPosition(player, resident);
                Movement::MoveToPosition(player, hub->MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, true);
                if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
                    ai->HandleTeleportAck();

                // If bot remains stuck outside the hub zone across sweeps, release it cleanly so it does not linger in foreign zones
                if (inForeignZone && (nowMs - resident.ActivityStartMs > 30000))
                {
                    ReleaseResident(resident, false);
                    continue;
                }
                continue;
            }

            // Arrival check: dismount when close to target
            if (distToTarget <= 4.0f)
            {
                if (!resident.AtSpot)
                {
                    resident.AtSpot = true;
                    Movement::DismountIfMounted(player);
                    SetStayPosition(player, resident);
                }
            }
            else
            {
                resident.AtSpot = false;
            }

            // Leash check: do not teleport-snap bots that are actively walking/navigating to their spot!
            if (resident.AtSpot && !player->isMoving() && distToTarget > _leashRadius)
            {
                ApplyCityStrategies(player, resident);
                SetStayPosition(player, resident);
                Movement::MoveToPosition(player, hub->MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, true);
                if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
                    ai->HandleTeleportAck();
                continue;
            }

            // Pathfinding unstick: if bot has been traveling towards target for > 60s and stopped moving without arriving
            if (!resident.AtSpot && !player->isMoving() && resident.ActivityDurationMs && (nowMs - resident.ActivityStartMs > 60000))
            {
                Movement::MoveToPosition(player, hub->MapId, resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetO, true);
                if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
                    ai->HandleTeleportAck();
                resident.AtSpot = true;
                continue;
            }

            if (resident.AtSpot && resident.ActivityDurationMs &&
                resident.Activity != CitizenActivity::ReleaseCycle &&
                nowMs - resident.ActivityStartMs >= resident.ActivityDurationMs &&
                !player->IsNonMeleeSpellCast(false) && !resident.ChatPartnerGuid &&
                resident.CrowdPhaseState == CrowdPhase::Linger)
            {
                if (Spot const* spot = FindSpot(resident.HubId, resident.SpotId))
                {
                    if (resident.Activity == CitizenActivity::InnDance)
                    {
                        if (!resident.Sitting)
                        {
                            // Active dance completed: take a bow/cheer and sit down to rest
                            player->HandleEmoteCommand(roll_chance_i(50) ? EMOTE_ONESHOT_BOW : EMOTE_ONESHOT_APPLAUD);
                            player->SetStandState(UNIT_STAND_STATE_SIT);
                            resident.Sitting = true;
                            resident.ActivityStartMs = nowMs;
                            resident.ActivityDurationMs = urand(60, 150) * 1000;
                        }
                        else
                        {
                            // Resting completed: stand up and resume dancing
                            player->SetStandState(UNIT_STAND_STATE_STAND);
                            resident.Sitting = false;
                            player->HandleEmoteCommand(EMOTE_STATE_DANCE);
                            resident.ActivityStartMs = nowMs;
                            resident.ActivityDurationMs = urand(60, 180) * 1000;
                        }
                    }
                    else if (resident.Activity == CitizenActivity::FishAtDocks)
                    {
                        if (!resident.Sitting)
                        {
                            // Active fishing completed: sit down by the dock to rest
                            player->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
                            player->SetStandState(UNIT_STAND_STATE_SIT);
                            resident.Sitting = true;
                            resident.ActivityStartMs = nowMs;
                            resident.ActivityDurationMs = urand(45, 120) * 1000;
                        }
                        else
                        {
                            // Resting completed: stand up and resume fishing
                            player->SetStandState(UNIT_STAND_STATE_STAND);
                            resident.Sitting = false;
                            resident.ActivityStartMs = nowMs;
                            resident.ActivityDurationMs = urand(60, 180) * 1000;
                        }
                    }
                    else if (resident.Activity == CitizenActivity::SitInTavern || resident.Activity == CitizenActivity::DrinkInTavern)
                    {
                        if (resident.Stationary)
                        {
                            // Dedicated tavern dwellers alternate between drinking and sitting while staying seated in tavern
                            resident.Activity = (resident.Activity == CitizenActivity::SitInTavern) ? CitizenActivity::DrinkInTavern : CitizenActivity::SitInTavern;
                            resident.ActivityStartMs = nowMs;
                            resident.ActivityDurationMs = urand(60, 180) * 1000;
                        }
                        else if (_roaming && _actRoaming)
                        {
                            // Non-stationary visitor finished drinking/resting: move on to city activities
                            if (resident.Sitting)
                            {
                                player->SetStandState(UNIT_STAND_STATE_STAND);
                                resident.Sitting = false;
                            }
                            player->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
                            RelocateResident(resident, *hub);
                            continue;
                        }
                        else
                        {
                            resident.Activity = (resident.Activity == CitizenActivity::SitInTavern) ? CitizenActivity::DrinkInTavern : CitizenActivity::SitInTavern;
                            resident.ActivityStartMs = nowMs;
                            resident.ActivityDurationMs = urand(60, 180) * 1000;
                        }
                    }
                    else
                    {
                        if (resident.Sitting)
                        {
                            player->SetStandState(UNIT_STAND_STATE_STAND);
                            resident.Sitting = false;
                        }
                        player->HandleEmoteCommand(EMOTE_ONESHOT_NONE);

                        if (!resident.Stationary && _roaming && _actRoaming &&
                            resident.Activity != CitizenActivity::ReleaseCycle &&
                            resident.Activity != CitizenActivity::InnDance &&
                            resident.Activity != CitizenActivity::FishAtDocks &&
                            resident.Role != CitizenRole::InnDancer &&
                            resident.Role != CitizenRole::Fisher)
                        {
                            RelocateResident(resident, *hub);
                            continue;
                        }
                        else
                        {
                            resident.Role = DetermineRole(*hub, *spot, resident.Bot);
                            resident.Activity = ChooseInitialActivity(resident.Role, *spot);
                            resident.ActivityStartMs = nowMs;
                            resident.ActivityDurationMs = urand(45, 180) * 1000;
                        }
                    }
                }
            }
            // Roaming relocation
            if (_roaming && _actRoaming && !resident.Stationary &&
                resident.Activity != CitizenActivity::ReleaseCycle &&
                resident.Activity != CitizenActivity::InnDance &&
                resident.Activity != CitizenActivity::FishAtDocks &&
                resident.Role != CitizenRole::InnDancer &&
                resident.Role != CitizenRole::Fisher &&
                resident.NextMoveAt <= nowSec)
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
                            _chatSayCooldownSec, _crowdPlayerRadius, _crowdChatCooldownSec, _crowdAfkMinSec, _crowdAfkMaxSec, _activitiesAfkEnable);
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
                        Activities::ExecutePlazaSocial(resident, player, *hub, nowMs, _chatUnrestricted, _chatRandomSayChance, _chatSayCooldownSec);
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
                        Activities::ExecuteBuyFood(resident, player, *hub, nowMs, _enableInteractions && _enableBuyFood);
                    break;
                case CitizenActivity::WanderDistrict:
                    if (_actRoaming && resident.NextEmoteAtMs <= nowMs)
                    {
                        Activities::ExecuteRandomEmote(resident, player, *hub, nowMs);
                        resident.NextEmoteAtMs = nowMs + urand(_emoteMinSeconds, _emoteMaxSeconds) * 1000;
                    }
                    break;
                case CitizenActivity::ReleaseCycle:
                    Activities::ExecuteReleaseCycle(resident, player, *hub, nowMs);
                    break;
                case CitizenActivity::RandomEmote:
                default:
                    if (_actRandomEmotes && resident.NextEmoteAtMs <= nowMs)
                    {
                        Activities::ExecuteRandomEmote(resident, player, *hub, nowMs);
                        resident.NextEmoteAtMs = nowMs + urand(_emoteMinSeconds, _emoteMaxSeconds) * 1000;
                    }
                    break;
            }
        }
    }

    void Manager::ApplyCityStrategies(Player* player, Resident& resident) const
    {
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            std::vector<std::string> beforeStrategies = ai->GetStrategies(BOT_STATE_NON_COMBAT);
            if (!resident.Journal.CapturedOriginalStrategies)
            {
                resident.Journal.CapturedOriginalStrategies = true;
                resident.Journal.OriginalNonCombatStrategies = beforeStrategies;
            }

            std::string strategiesToApply = _nonCombatStrategies;
            if (!_chatScriptedConversations && !_chatGreetings && !_chatUnrestricted)
                strategiesToApply += ",-chat,-emote";

            ai->ChangeStrategy(strategiesToApply, BOT_STATE_NON_COMBAT);

            std::vector<std::string> afterStrategies = ai->GetStrategies(BOT_STATE_NON_COMBAT);

            // Record actual additions (including native sibling side effects):
            for (std::string const& s : afterStrategies)
            {
                if (std::find(beforeStrategies.begin(), beforeStrategies.end(), s) == beforeStrategies.end())
                {
                    if (std::find(resident.Journal.AppliedStrategies.begin(), resident.Journal.AppliedStrategies.end(), s) == resident.Journal.AppliedStrategies.end())
                        resident.Journal.AppliedStrategies.push_back(s);
                }
            }

            // Record actual removals (including native sibling side effects):
            for (std::string const& s : beforeStrategies)
            {
                if (std::find(afterStrategies.begin(), afterStrategies.end(), s) == afterStrategies.end())
                {
                    if (std::find(resident.Journal.RemovedStrategies.begin(), resident.Journal.RemovedStrategies.end(), s) == resident.Journal.RemovedStrategies.end())
                        resident.Journal.RemovedStrategies.push_back(s);
                }
            }
        }
    }

    void Manager::SetStayPosition(Player* player, Resident& resident) const
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai)
            return;
        PositionMap& positions = ai->GetAiObjectContext()->GetValue<PositionMap&>("position")->Get();
        PositionInfo stay = positions["stay"];
        if (!resident.Journal.ModifiedStayPosition)
        {
            resident.Journal.ModifiedStayPosition = true;
            resident.Journal.HadStayPosition = stay.isSet();
            resident.Journal.PrevStayX = stay.x;
            resident.Journal.PrevStayY = stay.y;
            resident.Journal.PrevStayZ = stay.z;
            resident.Journal.PrevStayMap = stay.mapId;
        }
        resident.Journal.AppliedStayX = resident.TargetX;
        resident.Journal.AppliedStayY = resident.TargetY;
        resident.Journal.AppliedStayZ = resident.TargetZ;
        resident.Journal.AppliedStayMap = resident.TargetMap;
        stay.Set(resident.TargetX, resident.TargetY, resident.TargetZ, resident.TargetMap);
        positions["stay"] = stay;
    }

    void Manager::RestoreResidentState(Player* player, Resident const& resident) const
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai)
            return;

        PositionMap& positions = ai->GetAiObjectContext()->GetValue<PositionMap&>("position")->Get();
        PositionInfo stay = positions["stay"];
        if (resident.Journal.ModifiedStayPosition && stay.isSet() &&
            stay.x == resident.Journal.AppliedStayX && stay.y == resident.Journal.AppliedStayY &&
            stay.z == resident.Journal.AppliedStayZ && stay.mapId == resident.Journal.AppliedStayMap)
        {
            if (resident.Journal.HadStayPosition)
                stay.Set(resident.Journal.PrevStayX, resident.Journal.PrevStayY, resident.Journal.PrevStayZ, resident.Journal.PrevStayMap);
            else
                stay.Reset();
            positions["stay"] = stay;
        }

        // True delta restoration: revert only mutations made by City Life if current state still matches
        if (resident.Journal.CapturedOriginalStrategies)
        {
            std::vector<std::string> currentStrategies = ai->GetStrategies(BOT_STATE_NON_COMBAT);

            // Revert strategies added by City Life if still active and was NOT originally present
            for (std::string const& s : resident.Journal.AppliedStrategies)
            {
                bool wasOriginal = std::find(resident.Journal.OriginalNonCombatStrategies.begin(),
                                             resident.Journal.OriginalNonCombatStrategies.end(), s) != resident.Journal.OriginalNonCombatStrategies.end();
                if (!wasOriginal && std::find(currentStrategies.begin(), currentStrategies.end(), s) != currentStrategies.end())
                    ai->ChangeStrategy("-" + s, BOT_STATE_NON_COMBAT);
            }

            // Restore strategies removed by City Life if originally present and not currently active
            for (std::string const& s : resident.Journal.RemovedStrategies)
            {
                bool wasOriginal = std::find(resident.Journal.OriginalNonCombatStrategies.begin(),
                                             resident.Journal.OriginalNonCombatStrategies.end(), s) != resident.Journal.OriginalNonCombatStrategies.end();
                if (!wasOriginal || std::find(currentStrategies.begin(), currentStrategies.end(), s) != currentStrategies.end())
                    continue;
                auto siblings = ai->GetAiObjectContext()->GetSiblingStrategy(s);
                auto live = ai->GetStrategies(BOT_STATE_NON_COMBAT);
                bool newerSibling = std::any_of(live.begin(), live.end(),
                    [&](std::string const& current) { return siblings.count(current) != 0; });
                if (!newerSibling)
                    ai->ChangeStrategy("+" + s, BOT_STATE_NON_COMBAT);
            }
        }
    }

    void Manager::Update(uint32 diff)
    {
        _queryProcessor.ProcessReadyCallbacks();
        _transactionProcessor.ProcessReadyCallbacks();
        PlayerbotsLife::ResolveAdmissionRequests(GameTimeSeconds());
        bool managementTick = _timerMs <= diff;
        if (managementTick)
        {
            _timerMs = _tickSeconds * IN_MILLISECONDS;
            _changesThisTick = 0;
        }
        else
            _timerMs -= diff;

        _releasingTimerMs += diff;
        if (_releasingDirty || _releasingTimerMs >= 250)
        {
            _releasingTimerMs = 0;
            _releasingDirty = false;
            ProcessReleasingResidents(false);
        }

        if (!_enable || !_databaseReady)
            return;
        if (_startupElapsedMs < _startupDelaySeconds * IN_MILLISECONDS)
        {
            _startupElapsedMs += diff;
            return;
        }
        if (managementTick)
        {
            uint32 nowSec = GameTimeSeconds();
            PlayerbotsLife::ScavengeStaleLeases(nowSec);
            PlayerbotsLife::PruneRealPlayerCooldowns(nowSec);
            MaintainPopulation();
            MaintainResidents();
        }
        ProcessAdmissionGrants(); // Acknowledge five-second grants independently of management ticks.
    }

    void Manager::PrintStatus(ChatHandler* handler) const
    {
        handler->PSendSysMessage("Lifecycle: pendingAdmissions={} releasing={} changesThisTick={}/{}",
            _pendingAdmissions.size(), _releasingResidents.size(), _changesThisTick, _maxChangesPerTick);
        uint32 startupTarget = _startupDelaySeconds * IN_MILLISECONDS;
        uint32 startupLeft = _startupElapsedMs < startupTarget ?
            (startupTarget - _startupElapsedMs + 999) / 1000 : 0;
        uint32 liveBots = GetLiveWorldBotCount();
        handler->PSendSysMessage(
            "City Life: enabled={} DB={} startup={}s residents={}/{} (live bots={}) time={}%% hubs={} crowd={}",
            _enable ? 1 : 0, _databaseReady ? "ready" : "missing", startupLeft,
            _residents.size(), _maxTotalPopulation == 0 ? "uncapped" : std::to_string(_maxTotalPopulation).c_str(),
            liveBots, CurrentTimeMultiplier(), _hubs.size(),
            _crowdEnable ? 1 : 0);
        for (Hub const& hub : _hubs)
        {
            handler->PSendSysMessage("{} [{}] lvl {}-{} cfg={} target={} current={} spots={}",
                hub.Name, TeamName(hub.Team), hub.MinLevel, hub.MaxLevel, hub.ConfiguredPopulation,
                TargetPopulation(hub), CountResidents(hub.Id), hub.Spots.size());
        }
        auto snapshot = PlayerbotsLife::GetCooperationSnapshot();
        handler->PSendSysMessage("Admissions pending={} shared waiting={} unacknowledged={} ownership={} work used={}/{}",
            _pendingAdmissions.size(), snapshot.WaitingAdmissions, snapshot.UnacknowledgedGrants,
            snapshot.CityLifeCount, _changesThisTick, _maxChangesPerTick);
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
        handler->PSendSysMessage("Active Residents by Role:");

        uint32 crowdCount = 0, fisherCount = 0, dancerCount = 0, flexCount = 0;
        for (Resident const& r : _residents)
        {
            switch (r.Role)
            {
                case CitizenRole::Crowd: ++crowdCount; break;
                case CitizenRole::Fisher: ++fisherCount; break;
                case CitizenRole::InnDancer: ++dancerCount; break;
                default: ++flexCount; break;
            }
        }
        PlayerbotsLife::CooperationSnapshot snap = PlayerbotsLife::GetCooperationSnapshot();
        handler->PSendSysMessage("Cooperation: Reserved Total={} (City={}, PvP={}) | Leases: Prov={} Active={} Rel={}",
            snap.TotalReserved, snap.CityLifeCount, snap.PvPLifeCount,
            snap.ProvisionalCount, snap.ActiveCount, snap.ReleasingCount);
    }

    void Manager::PrintHelp(ChatHandler* handler) const
    {
        handler->PSendSysMessage("City Life commands:");
        handler->PSendSysMessage(".citylife status");
        handler->PSendSysMessage(".citylife compat");
        handler->PSendSysMessage(".citylife reload [config|data|all]");
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

        auto snapshot = PlayerbotsLife::GetCooperationSnapshot(GameTimeSeconds());

        handler->PSendSysMessage("=== Playerbots Life Coexistence Status ===");
        handler->PSendSysMessage("Active Modules: CityLife={} | PvPLife={} | Artisans={}",
            clActive ? "ACTIVE" : "INACTIVE",
            pvpActive ? "ACTIVE" : "INACTIVE",
            artActive ? "ACTIVE" : "INACTIVE");
        handler->PSendSysMessage("Protocol Version: {} | Registry Address: {:p}",
            PlayerbotsLife::GetReservationProtocolVersion(),
            PlayerbotsLife::GetReservationRegistryAddress());
        handler->PSendSysMessage("Active Reservations: CityLife={} | PvPLife={} | Artisans={} (Total: {})",
            snapshot.CityLifeCount, snapshot.PvPLifeCount, snapshot.ArtisansCount, snapshot.TotalReserved);
        handler->PSendSysMessage("Lease States: Active={} | Provisional={} | Releasing={} (Current Gen: {})",
            snapshot.ActiveCount, snapshot.ProvisionalCount, snapshot.ReleasingCount, snapshot.CurrentGeneration);
        handler->PSendSysMessage("Stale Leases: Stale Provisional={} (oldest={}s) | Stale Releasing={} (oldest={}s)",
            snapshot.StaleProvisionalCount, snapshot.OldestProvisionalAgeSec,
            snapshot.StaleReleasingCount, snapshot.OldestReleasingAgeSec);
        handler->PSendSysMessage("Arbitration: Waiting Admissions={} | Unacked Grants={}",
            snapshot.WaitingAdmissions, snapshot.UnacknowledgedGrants);
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

    void Manager::QueueMutation(std::string const& sql, uint32 gmGuidLow, std::string const& successMsg)
    {
        if (_pendingDbWrites >= 32)
        {
            LOG_WARN("module", "[CityLife] Database write queue full; GM mutation skipped.");
            if (gmGuidLow)
            {
                if (Player* gm = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(gmGuidLow)))
                    ChatHandler(gm->GetSession()).PSendSysMessage("City Life: Edit queue full; try again shortly.");
            }
            return;
        }

        WorldDatabaseTransaction transaction = WorldDatabase.BeginTransaction();
        transaction->Append(sql);
        ++_pendingDbWrites;
        uint64 gen = _dataGeneration;

        _transactionProcessor.AddCallback(WorldDatabase.AsyncCommitTransaction(transaction)).AfterComplete(
            [this, gen, gmGuidLow, successMsg](bool success)
            {
                --_pendingDbWrites;
                if (!success)
                {
                    LOG_ERROR("module", "[CityLife] GM database mutation transaction failed.");
                    if (gmGuidLow)
                    {
                        if (Player* gm = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(gmGuidLow)))
                            ChatHandler(gm->GetSession()).PSendSysMessage("City Life: Database edit failed.");
                    }
                    return;
                }

                if (gen == _dataGeneration && _pendingDbWrites == 0)
                    LoadData();

                if (gmGuidLow && !successMsg.empty())
                {
                    if (Player* gm = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(gmGuidLow)))
                        ChatHandler(gm->GetSession()).PSendSysMessage("{}", successMsg);
                }
            });
    }

    bool Manager::ValidateResidentAction(Player* bot, Resident const& resident) const
    {
        if (!bot || !bot->IsInWorld() || bot->IsBeingTeleported())
            return false;

        if (resident.Token.Generation == 0)
            return false;

        if (!PlayerbotsLife::ValidateActiveLease(resident.Token))
            return false;

        if (IsHumanControlled(bot))
            return false;

        return true;
    }

    bool Manager::IsResidentInSyntheticActivity(uint32 guidLow) const
    {
        for (Resident const& resident : _residents)
        {
            if (resident.Bot.GuidLow == guidLow)
            {
                return resident.Activity == CitizenActivity::PlazaSocial ||
                       resident.Activity == CitizenActivity::CrowdSocial;
            }
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
            uint32 zoneId = gm->GetZoneId();
            uint32 gmGuidLow = gm->GetGUID().GetCounter();
            std::string name = args[2];
            WorldDatabase.EscapeString(name);

            bool hasZoneId = WorldDatabase.Query("SHOW COLUMNS FROM `city_life_hub` LIKE 'zone_id'") != nullptr;
            std::string sql = hasZoneId
                ? Acore::StringFormat(
                    "INSERT INTO city_life_hub "
                    "(name,enabled,team,min_level,max_level,map_id,zone_id,default_population,priority) "
                    "VALUES ('{}',1,{},{},{},{},{},{},{}) ON DUPLICATE KEY UPDATE enabled=1,team=VALUES(team),"
                    "min_level=VALUES(min_level),max_level=VALUES(max_level),map_id=VALUES(map_id),zone_id=VALUES(zone_id),"
                    "default_population=VALUES(default_population),priority=VALUES(priority)",
                    name, static_cast<uint32>(team), minLevel, maxLevel, gm->GetMapId(), zoneId, population, priority)
                : Acore::StringFormat(
                    "INSERT INTO city_life_hub "
                    "(name,enabled,team,min_level,max_level,map_id,default_population,priority) "
                    "VALUES ('{}',1,{},{},{},{},{},{}) ON DUPLICATE KEY UPDATE enabled=1,team=VALUES(team),"
                    "min_level=VALUES(min_level),max_level=VALUES(max_level),map_id=VALUES(map_id),"
                    "default_population=VALUES(default_population),priority=VALUES(priority)",
                    name, static_cast<uint32>(team), minLevel, maxLevel, gm->GetMapId(), population, priority);

            std::string successMsg = Acore::StringFormat(
                "City Life: Saved hub '{}' (zone {}). Add spots with '.citylife spot add {} <name> <category>' before population can appear.",
                name, zoneId, name);
            QueueMutation(sql, gmGuidLow, successMsg);
            handler->PSendSysMessage("City Life: Queued hub '{}' save.", name);
            return true;
        }

        if ((operation == "enable" || operation == "disable") && args.size() >= 3)
        {
            std::string name = args[2];
            WorldDatabase.EscapeString(name);
            uint32 gmGuidLow = (handler->GetSession() && handler->GetSession()->GetPlayer()) ? handler->GetSession()->GetPlayer()->GetGUID().GetCounter() : 0;
            std::string sql = Acore::StringFormat("UPDATE city_life_hub SET enabled={} WHERE name='{}'",
                operation == "enable" ? 1 : 0, name);
            std::string successMsg = Acore::StringFormat("City Life: Hub '{}' {}.", name, operation == "enable" ? "enabled" : "disabled");
            QueueMutation(sql, gmGuidLow, successMsg);
            handler->PSendSysMessage("City Life: Queued hub '{}' update.", name);
            return true;
        }

        if (operation == "population" && args.size() >= 4)
        {
            uint32 population;
            if (!ParseUInt(args[3], population))
                return false;
            std::string name = args[2];
            WorldDatabase.EscapeString(name);
            uint32 gmGuidLow = (handler->GetSession() && handler->GetSession()->GetPlayer()) ? handler->GetSession()->GetPlayer()->GetGUID().GetCounter() : 0;
            std::string sql = Acore::StringFormat("UPDATE city_life_hub SET default_population={} WHERE name='{}'", population, name);
            std::string successMsg = Acore::StringFormat("City Life: Set DB population for hub '{}' to {}.", name, population);
            QueueMutation(sql, gmGuidLow, successMsg);
            handler->PSendSysMessage("City Life: Queued hub '{}' population update.", name);
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

        uint32 hubId = hub->Id;
        std::string hubName = hub->Name;

        if (operation == "list")
        {
            handler->PSendSysMessage("City Life - Spot list for {}:", hubName);
            for (Spot const& spot : hub->Spots)
                handler->PSendSysMessage("#{} {} [{}] weight={} radius={} ({:.1f}, {:.1f}, {:.1f})",
                    spot.Id, spot.Name, spot.Category, spot.Weight, spot.Radius, spot.X, spot.Y, spot.Z);
            return true;
        }

        if (operation == "add")
        {
            if (args.size() < 5 || !handler->GetSession() || !handler->GetSession()->GetPlayer())
                return false;
            Player* gm = handler->GetSession()->GetPlayer();
            if (hub->MapId != gm->GetMapId())
            {
                handler->PSendSysMessage("City Life: Cannot add spot to hub '{}' (map {}) while GM is on map {}.",
                    hubName, hub->MapId, gm->GetMapId());
                return true;
            }
            if (!std::isfinite(gm->GetPositionX()) || !std::isfinite(gm->GetPositionY()) ||
                !std::isfinite(gm->GetPositionZ()) || !std::isfinite(gm->GetOrientation()))
            {
                handler->PSendSysMessage("City Life: GM coordinates are not finite.");
                return true;
            }

            uint32 weight = 100;
            float radius = 4.0f;
            if ((args.size() > 5 && !ParseUInt(args[5], weight)) ||
                (args.size() > 6 && !ParseFloat(args[6], radius)))
                return false;
            weight = std::max<uint32>(1, weight);
            radius = std::clamp(radius, 0.0f, 30.0f);

            std::string spotName = args[3];
            std::string category = args[4];
            WorldDatabase.EscapeString(spotName);
            WorldDatabase.EscapeString(category);
            uint32 gmGuidLow = gm->GetGUID().GetCounter();

            bool hasPoiType = WorldDatabase.Query("SHOW COLUMNS FROM `city_life_spot` LIKE 'poi_type'") != nullptr;
            std::string sql = hasPoiType
                ? Acore::StringFormat(
                    "INSERT INTO city_life_spot (hub_id,name,category,enabled,x,y,z,o,radius,weight,poi_type,district) "
                    "VALUES ({},'{}','{}',1,{},{},{},{},{},{},{},'{}') ON DUPLICATE KEY UPDATE category=VALUES(category),"
                    "enabled=1,x=VALUES(x),y=VALUES(y),z=VALUES(z),o=VALUES(o),radius=VALUES(radius),"
                    "weight=VALUES(weight),poi_type=VALUES(poi_type),district=VALUES(district)",
                    hubId, spotName, category, gm->GetPositionX(), gm->GetPositionY(), gm->GetPositionZ(),
                    gm->GetOrientation(), radius, weight, static_cast<uint32>(CategoryToPoiType(category)), category)
                : Acore::StringFormat(
                    "INSERT INTO city_life_spot (hub_id,name,category,enabled,x,y,z,o,radius,weight) "
                    "VALUES ({},'{}','{}',1,{},{},{},{},{},{}) ON DUPLICATE KEY UPDATE category=VALUES(category),"
                    "enabled=1,x=VALUES(x),y=VALUES(y),z=VALUES(z),o=VALUES(o),radius=VALUES(radius),"
                    "weight=VALUES(weight)",
                    hubId, spotName, category, gm->GetPositionX(), gm->GetPositionY(), gm->GetPositionZ(),
                    gm->GetOrientation(), radius, weight);

            std::string successMsg = Acore::StringFormat("City Life: Saved spot '{}/{}' at GM position.", hubName, spotName);
            QueueMutation(sql, gmGuidLow, successMsg);
            handler->PSendSysMessage("City Life: Queued spot '{}/{}' save.", hubName, spotName);
            return true;
        }

        if (operation == "delete" && args.size() >= 4)
        {
            std::string spotName = args[3];
            WorldDatabase.EscapeString(spotName);
            uint32 gmGuidLow = (handler->GetSession() && handler->GetSession()->GetPlayer()) ? handler->GetSession()->GetPlayer()->GetGUID().GetCounter() : 0;
            std::string sql = Acore::StringFormat("DELETE FROM city_life_spot WHERE hub_id={} AND name='{}'", hubId, spotName);
            std::string successMsg = Acore::StringFormat("City Life: Deleted spot '{}/{}'.", hubName, spotName);
            QueueMutation(sql, gmGuidLow, successMsg);
            handler->PSendSysMessage("City Life: Queued spot '{}/{}' deletion.", hubName, spotName);
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
            std::string sub = args.size() > 1 ? Lower(args[1]) : "all";
            bool dataOk = true;

            if (sub == "config" || sub == "all")
            {
                LoadConfig();
            }

            if (sub == "data" || sub == "all")
            {
                _databaseReady = VerifyDatabase() && LoadData();
                dataOk = _databaseReady;
            }

            if (!dataOk)
                handler->PSendSysMessage("City Life: Reload encountered database errors. Check logs.");
            else if (sub == "config")
                handler->PSendSysMessage("City Life: Successfully reloaded configuration.");
            else if (sub == "data")
                handler->PSendSysMessage("City Life: Successfully reloaded hubs and spots data.");
            else
                handler->PSendSysMessage("City Life: Successfully reloaded configuration and data.");
            return true;
        }
        if (command == "hub" && HandleHubCommand(handler, args))
            return true;
        if (command == "spot" && HandleSpotCommand(handler, args))
            return true;

        PrintHelp(handler);
        return true;
    }

    static char const* GetActivityNameString(CitizenActivity act)
    {
        switch (act)
        {
            case CitizenActivity::LeaveInn: return "LeaveInn";
            case CitizenActivity::VisitAuctionHouse: return "VisitAuctionHouse";
            case CitizenActivity::VisitMailbox: return "VisitMailbox";
            case CitizenActivity::BuyFood: return "BuyFood";
            case CitizenActivity::WanderDistrict: return "WanderDistrict";
            case CitizenActivity::SitInTavern: return "SitInTavern";
            case CitizenActivity::VisitTrainer: return "VisitTrainer";
            case CitizenActivity::DrinkInTavern: return "DrinkInTavern";
            case CitizenActivity::WalkCityWalls: return "WalkCityWalls";
            case CitizenActivity::ReturnToInn: return "ReturnToInn";
            case CitizenActivity::ReleaseCycle: return "ReleaseCycle";
            case CitizenActivity::RandomEmote: return "RandomEmote";
            case CitizenActivity::FishAtDocks: return "FishAtDocks";
            case CitizenActivity::Idle: return "Idle";
            case CitizenActivity::InnDance: return "InnDance";
            case CitizenActivity::PlazaSocial: return "PlazaSocial";
            case CitizenActivity::CrowdSocial: return "CrowdSocial";
            default: return "Activity";
        }
    }

    static char const* GetRoleNameString(CitizenRole role)
    {
        switch (role)
        {
            case CitizenRole::InnDancer: return "Inn Dancer";
            case CitizenRole::PlazaTalker: return "Plaza Talker";
            case CitizenRole::Fisher: return "Fisher";
            case CitizenRole::Crowd: return "Crowd";
            case CitizenRole::Flex:
            default: return "Citizen";
        }
    }

    static std::string ResolveCityAction(Resident const& res, Player* p, Spot const* spot)
    {
        if (p)
        {
            if (!p->IsAlive())
                return "Fallen / Dead";
            if (p->IsBeingTeleported())
                return "Teleporting";
        }

        if (res.Activity == CitizenActivity::ReleaseCycle)
            return "Resting in Bed";

        if (res.Activity == CitizenActivity::FishAtDocks)
            return res.Sitting ? "Resting by Dock" : "Fishing at Dock";

        if (res.Activity == CitizenActivity::DrinkInTavern)
            return "Drinking Ale";

        if (res.Activity == CitizenActivity::SitInTavern)
        {
            if (res.Sitting)
                return (p && p->getStandState() == UNIT_STAND_STATE_SIT_CHAIR) ? "Seated on Chair" : "Sitting on Bench";
            return "Taking a Seat";
        }

        if (res.Activity == CitizenActivity::InnDance)
            return res.Sitting ? "Resting between Dances" : "Dancing in Tavern";

        if (res.Activity == CitizenActivity::WalkCityWalls)
            return "Wall Patrol Node " + std::to_string(res.SpotId);

        if (res.Activity == CitizenActivity::VisitAuctionHouse)
            return "Browsing Auctioneer";

        if (res.Activity == CitizenActivity::VisitMailbox)
            return "Checking Mailbox";

        if (res.Activity == CitizenActivity::VisitTrainer)
            return "Visiting Class Trainer";

        if (res.Activity == CitizenActivity::BuyFood)
            return "Purchasing Rations";

        if (res.Activity == CitizenActivity::CrowdSocial)
        {
            if (res.ChatPartnerGuid != 0)
            {
                Player* partner = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(res.ChatPartnerGuid));
                return "Talking to " + (partner ? partner->GetName() : "Partner");
            }
            if (res.CrowdPhaseState == CrowdPhase::Afk)
                return "Resting (AFK)";
            if (res.CrowdPhaseState == CrowdPhase::WanderOut)
                return "Browsing Counter";
            if (res.CrowdPhaseState == CrowdPhase::WanderBack)
                return "Returning to Queue";
            return "In Social Crowd";
        }

        if (res.Activity == CitizenActivity::PlazaSocial)
            return "Plaza Gathering";

        if (p && p->isMoving())
        {
            if (spot && !spot->District.empty())
                return "Walking to " + spot->District;
            return "Walking to Spot";
        }

        return res.AtSpot ? "At Spot" : "Moving to Spot";
    }

    uint32 Manager::GetTotalTargetPopulation() const
    {
        uint32 total = 0;
        for (Hub const& hub : _hubs)
        {
            if (hub.Enabled && !hub.Spots.empty())
                total += TargetPopulation(hub);
        }
        if (_maxTotalPopulation > 0 && total > _maxTotalPopulation)
            total = _maxTotalPopulation;
        return total;
    }

    void Manager::BuildTelemetryRoster(std::vector<std::string>& outRecords) const
    {
        uint32 nowSec = GameTimeSeconds();
        for (Resident const& res : _residents)
        {
            ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(res.Bot.GuidLow);
            char guidBuf[32];
            std::snprintf(guidBuf, sizeof(guidBuf), "0x%016llX", static_cast<unsigned long long>(playerGuid.GetRawValue()));

            std::string name = res.Bot.Name;
            std::string mod = "city";
            uint8 classId = 1;
            uint8 level = res.Bot.Level;
            std::string faction = (res.Bot.Team == TeamSide::Alliance) ? "alliance" : "horde";
            float x = res.TargetX;
            float y = res.TargetY;
            float z = res.TargetZ;
            std::string loc = "World";

            Player* p = ObjectAccessor::FindPlayer(playerGuid);
            if (p)
            {
                classId = p->getClass();
                level = p->GetLevel();
                x = p->GetPositionX();
                y = p->GetPositionY();
                z = p->GetPositionZ();
                if (p->GetTeamId() == TEAM_ALLIANCE)
                    faction = "alliance";
                else
                    faction = "horde";
            }

            Hub const* hub = nullptr;
            for (Hub const& h : _hubs)
            {
                if (h.Id == res.HubId)
                {
                    hub = &h;
                    break;
                }
            }

            Spot const* spot = FindSpot(res.HubId, res.SpotId);
            if (hub)
            {
                if (spot && !spot->District.empty())
                    loc = hub->Name + " - " + spot->District;
                else
                    loc = hub->Name;
            }

            std::string act = GetActivityNameString(res.Activity);
            std::string roleStr = GetRoleNameString(res.Role);
            std::string lastAct = ResolveCityAction(res, p, spot);
            if (lastAct.size() > 32)
                lastAct = lastAct.substr(0, 32);

            std::string nextT = "Relocate to next POI";
            if (res.Activity == CitizenActivity::CrowdSocial)
                nextT = "Social crowd dialogue";
            else if (res.Activity == CitizenActivity::ReleaseCycle)
                nextT = "Conclude residence session";

            int32 dueMs = 0;
            if (res.NextMoveAt > nowSec)
                dueMs = static_cast<int32>((res.NextMoveAt - nowSec) * 1000);

            std::string truth = "Deadline";
            std::string status = "Active";
            if (res.Activity == CitizenActivity::ReleaseCycle) status = "Resting";
            else if (res.Sitting) status = "Sitting";
            else if (!res.AtSpot) status = "Patrolling";

            // Format: guid|name|module|classId|level|faction|location|activity|lastAction|nextTask|dueMs|truthClass|status|x|y|z|zone|role
            char rec[512];
            std::snprintf(rec, sizeof(rec), "%s|%s|%s|%u|%u|%s|%s|%s|%s|%s|%d|%s|%s|%.1f|%.1f|%.1f|%s|%s",
                guidBuf, name.c_str(), mod.c_str(), static_cast<uint32>(classId), static_cast<uint32>(level),
                faction.c_str(), loc.c_str(), act.c_str(), lastAct.c_str(), nextT.c_str(),
                dueMs, truth.c_str(), status.c_str(), x, y, z, hub ? hub->Name.c_str() : loc.c_str(), roleStr.c_str());

            outRecords.emplace_back(rec);
        }
    }

    bool Manager::BuildBotDetail(uint32 guidLow, std::string& outPayload) const
    {
        Resident const* res = FindResident(guidLow);
        if (!res)
            return false;

        Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(guidLow));
        if (!player)
            return false;

        char guidBuf[32];
        std::snprintf(guidBuf, sizeof(guidBuf), "0x%016llX",
            static_cast<unsigned long long>(player->GetGUID().GetRawValue()));

        std::string role = GetRoleNameString(res->Role);
        Spot const* spot = FindSpot(res->HubId, res->SpotId);
        std::string action = ResolveCityAction(*res, player, spot);

        std::string targetName = "None";
        uint32 targetHp = 100;
        if (Unit* vic = player->GetVictim())
        {
            targetName = vic->GetName();
            targetHp = static_cast<uint32>(vic->GetHealthPct());
        }

        std::string spellName = "None";
        if (Spell* sp = player->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            if (sp->GetSpellInfo() && sp->GetSpellInfo()->SpellName[0])
                spellName = sp->GetSpellInfo()->SpellName[0];

        std::string strategies = "None";
        std::string lastActions = "None";
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            strategies = ai->HandleRemoteCommand("strategy");
            if (strategies.empty())
                strategies = "None";

            lastActions = ai->HandleRemoteCommand("action");
            if (lastActions.empty())
                lastActions = "None";
        }

        // Format: guid|role|action|targetName|targetHp|spellName|strategies|lastAction
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "%s|%s|%s|%s|%u|%s|%s|%s",
            guidBuf, role.c_str(), action.c_str(), targetName.c_str(), targetHp,
            spellName.c_str(), strategies.c_str(), lastActions.c_str());

        outPayload = buf;
        return true;
    }

    void Manager::BuildTelemetryHubs(std::string& outPayload) const
    {
        outPayload.clear();
        for (Hub const& hub : _hubs)
        {
            if (!hub.Enabled) continue;
            uint32 target = TargetPopulation(hub);
            uint32 active = CountResidents(hub.Id);
            uint32 pending = 0;
            uint32 poiCount = static_cast<uint32>(hub.Spots.size());
            float timeMult = static_cast<float>(CurrentTimeMultiplier()) / 100.0f;

            char line[256];
            std::snprintf(line, sizeof(line), "%u|%s|%u|%u|%u|%u|%.1f\n",
                hub.Id, hub.Name.c_str(), target, active, pending, poiCount, timeMult);
            outPayload += line;
        }
    }

    void Manager::BuildTelemetryQueues(std::string& outPayload) const
    {
        outPayload.clear();
    }
}
