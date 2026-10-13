/*
 * City Life
 * Copyright (C) 2026 iCore
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CityLifeActivities.h"
#include "CityLifeDialogues.h"
#include "CityLifeMovement.h"
#include "CityLifeMgr.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "MotionMaster.h"
#include "NearestGameObjects.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "Player.h"
#include "Item.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "SharedDefines.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "StringFormat.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ScriptedCreature.h"
#include "GameTime.h"

#include <cmath>

namespace CityLife
{
    namespace Activities
    {
        static inline uint32 GameTimeSeconds()
        {
            return static_cast<uint32>(GameTime::GetGameTime().count());
        }

        Player* FindNearbyRealPlayer(Player* bot, float radius)
        {
            if (!bot || !bot->IsInWorld())
                return nullptr;

            std::list<Player*> players;
            Acore::AnyPlayerInObjectRangeCheck check(bot, radius, /*reqAlive=*/true, /*disallowGM=*/false);
            Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(bot, players, check);
            Cell::VisitObjects(bot, searcher, radius);

            Player* best = nullptr;
            float bestDist = radius;

            for (Player* player : players)
            {
                if (!player || player == bot || !player->IsInWorld() || !::IsRealPlayer(player))
                    continue;

                float dist = bot->GetDistance(player);
                if (dist <= bestDist)
                {
                    best = player;
                    bestDist = dist;
                }
            }

            return best;
        }

        void CrowdClearAfk(Player* bot)
        {
            if (!bot)
                return;
            Resident* resident = sCityLifeMgr.FindResident(bot->GetGUID().GetCounter());
            if (!resident || !resident->AfkSetByModule ||
                (!PlayerbotsLife::ValidateActiveLease(resident->Token) &&
                 !PlayerbotsLife::ValidateLeaseState(resident->Token, PlayerbotsLife::LeaseState::Releasing)))
                return;
            if (bot->isAFK())
                bot->ToggleAFK();
            resident->AfkSetByModule = false;
        }

        void CrowdReleaseChat(Player* bot, Resident& resident)
        {
            if (resident.ChatPartnerGuid != 0)
            {
                ObjectGuid partnerGuid = ObjectGuid::Create<HighGuid::Player>(resident.ChatPartnerGuid);
                if (Player* partner = ObjectAccessor::FindPlayer(partnerGuid))
                {
                    // Clean partner state through manager's resident list
                    sCityLifeMgr.ClearResidentChatPartner(resident.ChatPartnerGuid, bot->GetGUID().GetCounter());
                }
            }

            resident.ChatPartnerGuid = 0;
            resident.ChatInitiator = false;
            resident.ChatDialogueId = -1;
            resident.ChatLineIdx = 0;
            resident.ChatDeadlineMs = 0;
            resident.LastCrowdChatMs = static_cast<uint32>(GameTime::GetGameTimeMS().count());
        }

        void ExecuteLivingCrowd(Resident& resident, Player* bot, Hub const& hub, uint32 now,
                                bool enableSay, uint32 sayCooldownSec, float playerRadius,
                                uint32 chatCooldownSec, uint32 afkMinSec, uint32 afkMaxSec,
                                bool enableAfk)
        {
            if (!bot || !bot->IsInWorld())
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            if (!enableAfk && bot->isAFK())
            {
                CrowdClearAfk(bot);
                if (resident.CrowdPhaseState == CrowdPhase::Afk)
                    resident.CrowdPhaseState = CrowdPhase::Linger;
            }

            if (resident.Sitting)
            {
                bot->SetStandState(UNIT_STAND_STATE_STAND);
                resident.Sitting = false;
            }

            // Stale conversation claim auto-release
            if (resident.ChatPartnerGuid != 0 && now >= resident.ChatDeadlineMs)
            {
                CrowdReleaseChat(bot, resident);
            }

            if (resident.CrowdHomeSpotId == 0)
                resident.CrowdHomeSpotId = resident.SpotId;

            // Proximity Wake Gate: If no real player is near, sleep scene cheaply
            Player* realPlayer = FindNearbyRealPlayer(bot, playerRadius);
            if (!realPlayer)
            {
                CrowdClearAfk(bot);
                resident.CrowdPhaseState = CrowdPhase::Linger;
                resident.CrowdPhaseEndMs = 0;
                return;
            }

            switch (resident.CrowdPhaseState)
            {
                case CrowdPhase::Afk:
                    if (enableAfk && now < resident.CrowdPhaseEndMs)
                        return; // Remain completely still
                    CrowdClearAfk(bot);
                    resident.CrowdPhaseState = CrowdPhase::Linger;
                    resident.CrowdPhaseEndMs = now + urand(20, 90) * 1000;
                    return;

                case CrowdPhase::WanderOut:
                    if (now < resident.CrowdPhaseEndMs)
                        return; // Dwelling at partner spot
                    // Walk back to home spot
                    if (Spot const* homeSpot = sCityLifeMgr.FindSpot(resident.HubId, resident.CrowdHomeSpotId))
                    {
                        Movement::MoveToPosition(bot, hub.MapId, homeSpot->X, homeSpot->Y, homeSpot->Z, homeSpot->O, false);
                    }
                    resident.CrowdPhaseState = CrowdPhase::WanderBack;
                    resident.CrowdPhaseEndMs = now + 30 * 1000;
                    return;

                case CrowdPhase::WanderBack:
                    if (bot->isMoving() && now < resident.CrowdPhaseEndMs)
                        return;
                    resident.CrowdPhaseState = CrowdPhase::Linger;
                    resident.CrowdPhaseEndMs = now + urand(20, 90) * 1000;
                    return;

                case CrowdPhase::Linger:
                default:
                    break;
            }

            // Dialogue delivery & initiator state
            if (!enableSay)
            {
                if (resident.ChatPartnerGuid != 0)
                    CrowdReleaseChat(bot, resident);
            }
            else
            {
                if (resident.ChatInitiator && resident.ChatDialogueId >= 0 &&
                    resident.ChatDialogueId < Dialogues::kCrowdDialogueCount)
                {
                    if (now >= resident.ChatDeadlineMs)
                    {
                        CrowdReleaseChat(bot, resident);
                    }
                    else
                    {
                        Dialogues::DialogueEntry const& d = Dialogues::kCrowdDialogues[resident.ChatDialogueId];
                        if (resident.ChatLineIdx < d.LineCount &&
                            now >= resident.LastSayMs + urand(3000, 6000))
                        {
                            bool const partnerLine = (resident.ChatLineIdx % 2) == 1;
                            Player* speaker = bot;
                            if (partnerLine)
                            {
                                ObjectGuid partnerGuid = ObjectGuid::Create<HighGuid::Player>(resident.ChatPartnerGuid);
                                speaker = ObjectAccessor::FindPlayer(partnerGuid);
                                if (!speaker || !speaker->IsInWorld() || bot->GetDistance(speaker) > 20.0f)
                                {
                                    CrowdReleaseChat(bot, resident);
                                    return;
                                }
                            }

                            speaker->Say(d.Lines[resident.ChatLineIdx], LANG_UNIVERSAL);
                            resident.LastSayMs = now;
                            ++resident.ChatLineIdx;

                            if (resident.ChatLineIdx >= d.LineCount)
                                CrowdReleaseChat(bot, resident);
                        }
                    }
                }
                else if (resident.ChatPartnerGuid == 0 &&
                         now >= resident.LastCrowdChatMs + chatCooldownSec * 1000 &&
                         roll_chance_i(25))
                {
                    // Scan for eligible partner in linger
                    Player* partner = sCityLifeMgr.FindCrowdChatPartner(resident.HubId, bot);
                    if (partner)
                    {
                        sCityLifeMgr.BeginCrowdChat(bot, partner, resident, now);
                    }
                }
            }

            if (now < resident.CrowdPhaseEndMs)
            {
                // Occasional idle emote while lingering
                if (roll_chance_i(12) && now >= resident.NextEmoteAtMs)
                {
                    resident.NextEmoteAtMs = now + 25 * 1000;
                    static uint32 const idleEmotes[] = {
                        EMOTE_ONESHOT_TALK, EMOTE_ONESHOT_NO, EMOTE_ONESHOT_POINT,
                        EMOTE_ONESHOT_LAUGH, EMOTE_ONESHOT_QUESTION
                    };
                    bot->HandleEmoteCommand(idleEmotes[urand(0, 4)]);
                }
                return;
            }

            // Hold linger if in conversation
            if (resident.ChatPartnerGuid != 0)
                return;

            // Transition roll
            uint32 roll = urand(1, 100);
            if (enableAfk && roll <= 15)
            {
                if (!bot->isAFK())
                {
                    bot->ToggleAFK();
                    resident.AfkSetByModule = true;
                }
                resident.CrowdPhaseState = CrowdPhase::Afk;
                uint32 minSec = afkMinSec;
                uint32 maxSec = afkMaxSec < minSec ? minSec : afkMaxSec;
                resident.CrowdPhaseEndMs = now + urand(minSec, maxSec) * 1000;
            }
            else if (roll <= 35)
            {
                // Wander out to partner spot
                Spot const* curSpot = sCityLifeMgr.FindSpot(resident.HubId, resident.SpotId);
                Spot const* partnerSpot = nullptr;
                if (curSpot && curSpot->PartnerSpotId != 0)
                {
                    partnerSpot = sCityLifeMgr.FindSpot(resident.HubId, curSpot->PartnerSpotId);
                }
                if (!partnerSpot && curSpot)
                {
                    // Fallback: dynamic partner resolution between Bank and Auction House in the same hub
                    CityPoiType targetType = (curSpot->PoiType == CityPoiType::Bank) ? CityPoiType::AuctionHouse : CityPoiType::Bank;
                    for (Spot const& s : hub.Spots)
                    {
                        if (s.PoiType == targetType && s.Id != curSpot->Id)
                        {
                            partnerSpot = &s;
                            break;
                        }
                    }
                }

                if (partnerSpot)
                {
                    Movement::MoveToPosition(bot, hub.MapId, partnerSpot->X, partnerSpot->Y, partnerSpot->Z, partnerSpot->O, false);
                    resident.CrowdPhaseState = CrowdPhase::WanderOut;
                    resident.CrowdPhaseEndMs = now + urand(30, 120) * 1000;
                    return;
                }
                resident.CrowdPhaseEndMs = now + urand(20, 90) * 1000;
            }
            else
            {
                resident.CrowdPhaseEndMs = now + urand(20, 90) * 1000;
            }
        }

        void ExecuteFishing(Resident& resident, Player* bot, Hub const& hub, uint32 now)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving() || !resident.AtSpot)
                return;

            if (bot->GetMapId() != hub.MapId || (hub.ZoneId != 0 && bot->GetZoneId() != hub.ZoneId))
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            Movement::DismountIfMounted(bot);

            // If fisher is in resting/break phase, remain seated by the water
            if (resident.Sitting)
                return;

            if (bot->getStandState() != UNIT_STAND_STATE_STAND)
                bot->SetStandState(UNIT_STAND_STATE_STAND);

            Spot const* spot = sCityLifeMgr.FindSpot(resident.HubId, resident.SpotId);
            if (spot && std::abs(bot->GetOrientation() - spot->O) > 0.05f)
                bot->SetFacingTo(spot->O);

            if (bot->IsNonMeleeSpellCast(false))
                return;

            // Visual fishing loop: enforce fishing stance (EMOTE_STATE_FISHING) so the bot holds
            // the fishing pole stance continuously, with occasional ONESHOT_FISHING cast animations.
            if (resident.NextEmoteAtMs <= now)
            {
                if (roll_chance_i(40))
                {
                    bot->HandleEmoteCommand(EMOTE_ONESHOT_FISHING);
                    // Schedule returning to sustained fishing stance shortly after casting motion
                    resident.NextEmoteAtMs = now + urand(3, 5) * 1000;
                }
                else
                {
                    bot->HandleEmoteCommand(EMOTE_STATE_FISHING);
                    resident.NextEmoteAtMs = now + urand(12, 25) * 1000;
                }
            }
        }

        void ExecuteInnDance(Resident& resident, Player* bot, Hub const& hub, uint32 now,
                             bool enableSay, uint32 sayCooldownSec)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving() || !resident.AtSpot)
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            Movement::DismountIfMounted(bot);

            // If dancer is in resting/break phase, remain seated and do not override
            if (resident.Sitting)
                return;

            if (bot->getStandState() != UNIT_STAND_STATE_STAND)
                bot->SetStandState(UNIT_STAND_STATE_STAND);

            bot->HandleEmoteCommand(EMOTE_STATE_DANCE);

            // Greet nearby real player or shout tavern dialogue
            if (enableSay && now >= resident.LastSayMs + sayCooldownSec * 1000)
            {
                Player* nearby = FindNearbyRealPlayer(bot, 6.0f);
                if (nearby)
                {
                    bot->SetFacingToObject(nearby);
                    resident.LastSayMs = now;
                    static char const* greetings[] = {
                        "Hey there, {}!",
                        "Welcome to the inn, {}!",
                        "Enjoy your stay, {}!",
                        "Hi, {}!"
                    };
                    bot->Say(Acore::StringFormat(greetings[urand(0, 3)], nearby->GetName()), LANG_UNIVERSAL);
                }
                else if (roll_chance_i(20))
                {
                    resident.LastSayMs = now;
                    static char const* phrases[] = {
                        "Welcome to the tavern!",
                        "Best inn around!",
                        "Another round for the house!",
                        "Dance like nobody's watching!"
                    };
                    bot->Say(phrases[urand(0, 3)], LANG_UNIVERSAL);
                }
            }
        }

        void ExecutePlazaSocial(Resident& resident, Player* bot, Hub const& hub, uint32 now,
                                bool enableUnrestricted, uint32 randomSayChance, uint32 sayCooldownSec)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving() || !resident.AtSpot)
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            if (resident.Sitting)
            {
                bot->SetStandState(UNIT_STAND_STATE_STAND);
                resident.Sitting = false;
            }

            // Scan for a nearby citizen in the square
            Player* nearby = nullptr;
            float bestDist = 15.0f;
            for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
            {
                (void)guid;
                if (!player || player == bot || !player->IsInWorld())
                    continue;
                if (!player->GetSession() || !player->GetSession()->IsBot())
                    continue;
                if (player->GetMapId() != bot->GetMapId())
                    continue;

                float dist = bot->GetDistance(player);
                if (dist < bestDist)
                {
                    nearby = player;
                    bestDist = dist;
                }
            }

            if (nearby && roll_chance_i(40))
            {
                bot->SetFacingToObject(nearby);
                if (roll_chance_i(50))
                    bot->HandleEmoteCommand(EMOTE_ONESHOT_WAVE);
                else
                    bot->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);

                if (now >= resident.LastSayMs + sayCooldownSec * 1000 && (enableUnrestricted || roll_chance_i(25)))
                {
                    resident.LastSayMs = now;
                    static char const* phrases[] = {
                        "Hey {}! Good to see you.",
                        "Lovely day in the city, {}!",
                        "Busy day around here, {}!",
                        "How fares the adventure, {}?"
                    };
                    bot->Say(Acore::StringFormat(phrases[urand(0, 3)], nearby->GetName()), LANG_UNIVERSAL);
                }
                return;
            }

            if (roll_chance_i(35))
            {
                static uint32 const emotes[] = { EMOTE_ONESHOT_WAVE, EMOTE_ONESHOT_CHEER, EMOTE_ONESHOT_LAUGH };
                bot->HandleEmoteCommand(emotes[urand(0, 2)]);
            }

            if (enableUnrestricted && randomSayChance > 0 && roll_chance_i(randomSayChance) && now >= resident.LastSayMs + sayCooldownSec * 1000)
            {
                resident.LastSayMs = now;
                static char const* shouts[] = {
                    "Lovely weather in the city today!",
                    "Who's up for an adventure?",
                    "Busy day in the square!",
                    "Good to see everyone around!"
                };
                bot->Say(shouts[urand(0, 3)], LANG_UNIVERSAL);
            }
        }

        void ExecuteTavernSitting(Resident& resident, Player* bot, Hub const& hub, uint32 now)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving() || !resident.AtSpot)
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            if (!resident.Sitting)
            {
                bool nearChair = false;
                std::list<GameObject*> gameobjects;
                AnyGameObjectInObjectRangeCheck check(bot, 3.5f);
                Acore::GameObjectListSearcher<AnyGameObjectInObjectRangeCheck> searcher(bot, gameobjects, check);
                Cell::VisitObjects(bot, searcher, 3.5f);
                for (GameObject* go : gameobjects)
                {
                    if (go && go->GetGoType() == GAMEOBJECT_TYPE_CHAIR)
                    {
                        bot->SetFacingToObject(go);
                        nearChair = true;
                        break;
                    }
                }

                bot->SetStandState(nearChair ? UNIT_STAND_STATE_SIT_CHAIR : UNIT_STAND_STATE_SIT);
                resident.Sitting = true;
                if (hub.SocialHub && roll_chance_i(35))
                    bot->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
            }
            else if (roll_chance_i(15))
            {
                bot->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
            }
        }

        void ExecuteTavernDrinking(Resident& resident, Player* bot, Hub const& hub, uint32 now)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving() || !resident.AtSpot)
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            if (!resident.Sitting)
            {
                bool nearChair = false;
                std::list<GameObject*> gameobjects;
                AnyGameObjectInObjectRangeCheck check(bot, 3.5f);
                Acore::GameObjectListSearcher<AnyGameObjectInObjectRangeCheck> searcher(bot, gameobjects, check);
                Cell::VisitObjects(bot, searcher, 3.5f);
                for (GameObject* go : gameobjects)
                {
                    if (go && go->GetGoType() == GAMEOBJECT_TYPE_CHAIR)
                    {
                        bot->SetFacingToObject(go);
                        nearChair = true;
                        break;
                    }
                }

                bot->SetStandState(nearChair ? UNIT_STAND_STATE_SIT_CHAIR : UNIT_STAND_STATE_SIT);
                resident.Sitting = true;
            }

            if (roll_chance_i(35))
            {
                bot->HandleEmoteCommand(EMOTE_ONESHOT_EAT_NO_SHEATHE);
            }
            else if (roll_chance_i(25))
            {
                bot->HandleEmoteCommand(EMOTE_ONESHOT_APPLAUD);
            }
        }

        void ExecuteVisitNpc(Resident& resident, Player* bot, Hub const& hub, uint32 now, uint32 npcFlag)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving() || !resident.AtSpot)
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            Movement::DismountIfMounted(bot);

            if (resident.Sitting || bot->getStandState() != UNIT_STAND_STATE_STAND)
            {
                bot->SetStandState(UNIT_STAND_STATE_STAND);
                resident.Sitting = false;
            }

            // Search for nearby creature with matching NPCFlag
            std::list<Creature*> creatures;
            Acore::AnyUnitInObjectRangeCheck check(bot, 25.0f);
            Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(bot, creatures, check);
            Cell::VisitObjects(bot, searcher, 25.0f);

            Creature* bestTarget = nullptr;
            for (Creature* creature : creatures)
            {
                if (!creature || !creature->IsAlive())
                    continue;

                bool matches = false;
                if (npcFlag == UNIT_NPC_FLAG_AUCTIONEER)
                    matches = creature->HasNpcFlag(UNIT_NPC_FLAG_AUCTIONEER);
                else if (npcFlag == UNIT_NPC_FLAG_TRAINER)
                {
                    matches = creature->IsTrainer() || creature->HasNpcFlag(static_cast<NPCFlags>(UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_TRAINER_CLASS | UNIT_NPC_FLAG_TRAINER_PROFESSION));
                    if (matches)
                    {
                        CreatureTemplate const* cInfo = creature->GetCreatureTemplate();
                        if (cInfo)
                        {
                            std::string subLower = cInfo->SubName;
                            std::transform(subLower.begin(), subLower.end(), subLower.begin(), [](unsigned char c){ return std::tolower(c); });
                            bool classMatch = false;
                            switch (bot->getClass())
                            {
                                case CLASS_WARRIOR: classMatch = subLower.find("warrior") != std::string::npos; break;
                                case CLASS_PALADIN: classMatch = subLower.find("paladin") != std::string::npos; break;
                                case CLASS_HUNTER: classMatch = subLower.find("hunter") != std::string::npos; break;
                                case CLASS_ROGUE: classMatch = subLower.find("rogue") != std::string::npos; break;
                                case CLASS_PRIEST: classMatch = subLower.find("priest") != std::string::npos; break;
                                case CLASS_DEATH_KNIGHT: classMatch = subLower.find("death knight") != std::string::npos; break;
                                case CLASS_SHAMAN: classMatch = subLower.find("shaman") != std::string::npos; break;
                                case CLASS_MAGE: classMatch = subLower.find("mage") != std::string::npos; break;
                                case CLASS_WARLOCK: classMatch = subLower.find("warlock") != std::string::npos; break;
                                case CLASS_DRUID: classMatch = subLower.find("druid") != std::string::npos; break;
                            }
                            if (classMatch)
                            {
                                bestTarget = creature;
                                break;
                            }
                        }
                    }
                }
                else if (npcFlag == UNIT_NPC_FLAG_VENDOR)
                    matches = creature->IsVendor() || creature->HasNpcFlag(static_cast<NPCFlags>(UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_VENDOR_FOOD));
                else
                    matches = creature->HasNpcFlag(static_cast<NPCFlags>(npcFlag));

                if (matches && !bestTarget)
                    bestTarget = creature;
            }

            if (bestTarget)
            {
                bestTarget->StopMoving();
                bot->GetMotionMaster()->Clear();
                bot->SetFacingToObject(bestTarget);
                WorldPacket packet(CMSG_GOSSIP_HELLO, 8);
                packet << bestTarget->GetGUID();
                bot->GetSession()->HandleGossipHelloOpcode(packet);

                if (now >= resident.NextEmoteAtMs)
                {
                    resident.NextEmoteAtMs = now + urand(10, 20) * 1000;
                    static uint32 const npcEmotes[] = {
                        EMOTE_ONESHOT_TALK, EMOTE_ONESHOT_QUESTION,
                        EMOTE_ONESHOT_YES, EMOTE_ONESHOT_SALUTE, EMOTE_ONESHOT_BOW
                    };
                    bot->HandleEmoteCommand(npcEmotes[urand(0, 4)]);
                }
            }
        }

        void ExecuteVisitMailbox(Resident& resident, Player* bot, Hub const& hub, uint32 now)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving() || !resident.AtSpot)
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            Movement::DismountIfMounted(bot);

            if (resident.Sitting || bot->getStandState() != UNIT_STAND_STATE_STAND)
            {
                bot->SetStandState(UNIT_STAND_STATE_STAND);
                resident.Sitting = false;
            }

            std::list<GameObject*> gameobjects;
            AnyGameObjectInObjectRangeCheck check(bot, 20.0f);
            Acore::GameObjectListSearcher<AnyGameObjectInObjectRangeCheck> searcher(bot, gameobjects, check);
            Cell::VisitObjects(bot, searcher, 20.0f);

            for (GameObject* go : gameobjects)
            {
                if (go && go->GetGoType() == GAMEOBJECT_TYPE_MAILBOX)
                {
                    bot->SetFacingToObject(go);
                    go->Use(bot);
                    if (now >= resident.NextEmoteAtMs)
                    {
                        resident.NextEmoteAtMs = now + urand(8, 16) * 1000;
                        static uint32 const mailEmotes[] = {
                            EMOTE_STATE_USE_STANDING, EMOTE_ONESHOT_LOOT,
                            EMOTE_STATE_USE_STANDING_NO_SHEATHE, EMOTE_ONESHOT_YES
                        };
                        bot->HandleEmoteCommand(mailEmotes[urand(0, 3)]);
                    }
                    break;
                }
            }
        }

        void ExecuteBuyFood(Resident& resident, Player* bot, Hub const& hub, uint32 now, bool enableInteractions)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving() || !resident.AtSpot)
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            if (enableInteractions)
            {
                if (PlayerbotAI* ai = GET_PLAYERBOT_AI(bot))
                {
                    ai->DoSpecificAction("rpg buy", Event("rpg action", "vendor"), true);
                }
            }

            // Visual visit fallback with vendor gossip and eating/drinking emote
            ExecuteVisitNpc(resident, bot, hub, now, UNIT_NPC_FLAG_VENDOR);
            if (now >= resident.NextEmoteAtMs)
            {
                resident.NextEmoteAtMs = now + urand(10, 20) * 1000;
                bot->HandleEmoteCommand(roll_chance_i(50) ? EMOTE_ONESHOT_EAT_NO_SHEATHE : EMOTE_ONESHOT_TALK);
            }
        }

        void ExecuteWallPatrol(Resident& resident, Player* bot, Hub const& hub, uint32 now)
        {
            if (!bot || !bot->IsInWorld())
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            Spot const* spot = sCityLifeMgr.FindSpot(resident.HubId, resident.SpotId);
            if (!spot)
                return;

            if (!Movement::IsAtSpot(bot, *spot, 4.0f))
            {
                if (!bot->isMoving())
                    Movement::MoveToPosition(bot, hub.MapId, spot->X, spot->Y, spot->Z, spot->O, false);
            }
            else if (now >= resident.NextMoveAt)
            {
                Spot const* partner = nullptr;
                if (spot->PartnerSpotId != 0)
                    partner = sCityLifeMgr.FindSpot(resident.HubId, spot->PartnerSpotId);

                if (!partner)
                {
                    // Fallback: cycle through any other wall patrol spot in the hub
                    for (Spot const& s : hub.Spots)
                    {
                        if (s.PoiType == CityPoiType::WallPatrol && s.Id != spot->Id)
                        {
                            partner = &s;
                            break;
                        }
                    }
                }

                if (partner)
                {
                    sCityLifeMgr.AssignSpot(resident, hub, *partner, false);
                    resident.NextMoveAt = GameTimeSeconds() + urand(15, 35);
                    Movement::MoveToPosition(bot, hub.MapId, partner->X, partner->Y, partner->Z, partner->O, false);
                }
            }
        }

        void ExecuteReleaseCycle(Resident& resident, Player* bot, Hub const& hub, uint32 now)
        {
            if (!bot || !bot->IsInWorld())
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            Spot const* spot = sCityLifeMgr.FindSpot(resident.HubId, resident.SpotId);
            if (!spot)
                return;

            if (!Movement::IsAtSpot(bot, *spot, 3.5f))
            {
                if (!bot->isMoving())
                    Movement::MoveToPosition(bot, hub.MapId, spot->X, spot->Y, spot->Z, spot->O, false);
                return;
            }

            Movement::DismountIfMounted(bot);

            if (bot->getStandState() != UNIT_STAND_STATE_SLEEP)
            {
                bot->SetStandState(UNIT_STAND_STATE_SLEEP);
            }
        }

        void ExecuteRandomEmote(Resident& resident, Player* bot, Hub const& hub, uint32 now)
        {
            if (!bot || !bot->IsInWorld() || bot->isMoving())
                return;

            if (!sCityLifeMgr.ValidateResidentAction(bot, resident))
                return;

            static uint32 const emotes[] = {
                EMOTE_ONESHOT_WAVE, EMOTE_ONESHOT_CHEER, EMOTE_ONESHOT_BOW,
                EMOTE_ONESHOT_LAUGH, EMOTE_ONESHOT_POINT, EMOTE_ONESHOT_APPLAUD
            };

            bot->HandleEmoteCommand(emotes[urand(0, 5)]);
        }
    }
}
