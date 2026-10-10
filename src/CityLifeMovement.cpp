/*
 * City Life
 * Copyright (C) 2026 iCore
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CityLifeMovement.h"

#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "PathGenerator.h"
#include "Player.h"
#include "SpellAuraDefines.h"
#include "SpellMgr.h"
#include "StringFormat.h"

#include <cmath>
#include <limits>

namespace CityLife
{
    namespace Movement
    {
        bool LowestPoiZWithin(Hub const& hub, float x, float y, float radius, float* outMinZ)
        {
            if (!outMinZ || hub.Spots.empty())
                return false;

            float minZ = std::numeric_limits<float>::max();
            bool found = false;

            for (Spot const& spot : hub.Spots)
            {
                float dx = spot.X - x;
                float dy = spot.Y - y;
                float dist2d = std::sqrt(dx * dx + dy * dy);
                if (dist2d <= radius)
                {
                    if (spot.Z < minZ)
                    {
                        minZ = spot.Z;
                        found = true;
                    }
                }
            }

            if (found)
            {
                *outMinZ = minZ;
                return true;
            }

            return false;
        }

        void CheckSinkForensics(Player* bot, float prevZ, bool sinkForensicsEnabled)
        {
            if (!sinkForensicsEnabled || !bot || !bot->IsInWorld() || prevZ == 0.0f)
                return;

            if (bot->IsBeingTeleported())
                return;

            float currentZ = bot->GetPositionZ();
            float drop = prevZ - currentZ;

            if (drop > 8.0f)
            {
                if (bot->IsInWater() || drop > 20.0f)
                {
                    LOG_WARN("module", "[CityLife] SINK WARNING: Bot '{}' (GUID {}) dropped {:.1f}y (Z {:.1f} -> {:.1f}) at ({:.1f}, {:.1f}) | moving={} inWater={}",
                        bot->GetName(), bot->GetGUID().GetCounter(), drop, prevZ, currentZ,
                        bot->GetPositionX(), bot->GetPositionY(), bot->isMoving() ? 1 : 0, bot->IsInWater() ? 1 : 0);
                }
                else
                {
                    LOG_DEBUG("module", "[CityLife] SINK INFO: Bot '{}' (GUID {}) descended {:.1f}y (Z {:.1f} -> {:.1f}) at ({:.1f}, {:.1f})",
                        bot->GetName(), bot->GetGUID().GetCounter(), drop, prevZ, currentZ,
                        bot->GetPositionX(), bot->GetPositionY());
                }
            }
        }

        void DismountIfMounted(Player* bot)
        {
            if (!bot)
                return;

            if (bot->HasMountedAura() || bot->IsMounted())
            {
                bot->Dismount();
                bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
            }
        }

        bool CanMountHere(Player* bot)
        {
            if (!bot || !bot->IsInWorld() || !bot->IsAlive())
                return false;

            if (bot->IsInCombat() || bot->duel || bot->IsInWater() || bot->IsBeingTeleported())
                return false;

            if (bot->GetMap() && bot->GetMap()->Instanceable())
                return false;

            return bot->IsOutdoors();
        }

        bool TryMount(Player* bot)
        {
            if (!CanMountHere(bot) || bot->HasMountedAura() || bot->isMoving())
                return false;

            // Popular ground mounts by race or generic
            static uint32 const mountSpells[] = {
                458,    // Brown Horse
                470,    // Black Stallion
                472,    // Pinto
                580,    // Timber Wolf
                6648,   // Chestnut Mare
                6653,   // Gray Wolf
                6654,   // Brown Wolf
                10793,  // Striped Frostsaber
                10796,  // Spotted Frostsaber
                10873,  // Red Mechanostrider
                10969,  // Blue Mechanostrider
                18989,  // Gray Kodo
                18990,  // Brown Kodo
                34795,  // Swift Palomino
                35710   // Black Hawkstrider
            };

            for (uint32 spellId : mountSpells)
            {
                if (bot->HasSpell(spellId))
                {
                    bot->CastSpell(bot, spellId, true);
                    return true;
                }
            }

            return false;
        }

        void MoveToPosition(Player* bot, uint32 mapId, float x, float y, float z, float o, bool teleport)
        {
            if (!bot)
                return;

            if (teleport || bot->GetMapId() != mapId)
            {
                bot->TeleportTo(mapId, x, y, z, o);
            }
            else
            {
                bot->GetMotionMaster()->MovePoint(0, x, y, z);
            }
        }

        bool IsAtSpot(Player* bot, Spot const& spot, float tolerance)
        {
            if (!bot)
                return false;

            return bot->GetDistance2d(spot.X, spot.Y) <= tolerance;
        }

        bool IsAtCoordinates(Player* bot, uint32 mapId, float x, float y, float tolerance)
        {
            if (!bot || bot->GetMapId() != mapId)
                return false;

            return bot->GetDistance2d(x, y) <= tolerance;
        }

        bool PathTouchesWater(Player* bot, PathGenerator const& path)
        {
            if (!bot)
                return false;

            Map const* map = bot->GetMap();
            if (!map)
                return false;

            uint32 const phase = bot->GetPhaseMask();
            float const ch = bot->GetCollisionHeight();
            ::Movement::PointsArray const& pts = path.GetPath();

            for (G3D::Vector3 const& p : pts)
            {
                if (map->IsInWater(phase, p.x, p.y, p.z, ch))
                    return true;
            }

            // Path points are several yards apart; sample the midpoints so a narrow canal
            // cannot slip between two dry points on opposite banks.
            for (std::size_t i = 1; i < pts.size(); ++i)
            {
                G3D::Vector3 const m = (pts[i - 1] + pts[i]) * 0.5f;
                if (map->IsInWater(phase, m.x, m.y, m.z, ch))
                    return true;
            }

            // If path is incomplete, check 5y radial ring around tail to ensure it doesn't end into water edge
            if (!pts.empty() && (path.GetPathType() & PATHFIND_INCOMPLETE))
            {
                G3D::Vector3 const& e = pts.back();
                static float const kRing[][2] = {
                    {5.0f, 0.0f}, {-5.0f, 0.0f}, {0.0f, 5.0f}, {0.0f, -5.0f},
                    {3.5f, 3.5f}, {-3.5f, 3.5f}, {3.5f, -3.5f}, {-3.5f, -3.5f}
                };
                for (auto const& r : kRing)
                {
                    if (map->IsInWater(phase, e.x + r[0], e.y + r[1], e.z, ch))
                        return true;
                }
            }

            return false;
        }

        bool HasCompleteWalkablePath(Player* bot, float x, float y, float z, bool allowWater)
        {
            if (!bot || !bot->IsInWorld())
                return false;

            PathGenerator path(bot);
            if (!path.CalculatePath(x, y, z, false))
                return false;

            uint32 const type = path.GetPathType();
            if (type & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH))
                return false;

            if (type & PATHFIND_INCOMPLETE)
            {
                G3D::Vector3 const& end = path.GetActualEndPosition();
                float const tx = end.x - x;
                float const ty = end.y - y;
                float const tz = end.z - z;
                if (tx * tx + ty * ty + tz * tz > 15.0f * 15.0f)
                    return false;
            }

            if (!allowWater && PathTouchesWater(bot, path))
                return false;

            return true;
        }
    }
}
