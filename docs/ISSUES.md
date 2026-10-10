# mod-playerbots-city-life Issues & Diagnostics Log

> **Location:** `mod-playerbots-city-life/docs/ISSUES.md`
> **Source Module:** [`mod-playerbots-city-life`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/mod-playerbots-city-life)  
> **Commander Navigator:** [`COMMANDER/README.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects%20Azerothcore/Azerothcore%20modules/COMMANDER/README.md)


---

## 1. Module Issues Summary

| Issue ID | Title | Severity | Status | Affected Component | Resolved / Documented In |
|---|---|---|---|---|---|
| **[MPCL-001]** | Missing Database Table `city_life_hub` on World Server Boot | Medium | **RESOLVED** | World Database (`acore_world`) / Schema Initializer | `data/sql/manual/world_city_life.sql` |

---

## 2. Technical Issue Details

<a id="mpcl-001"></a>

### [MPCL-001] Missing Database Table `city_life_hub` on World Server Boot
* **Severity**: Medium (Feature Degraded / Disabled at Startup)
* **Component**: Database Schema Initializer / `mod_playerbots_city_life.cpp`
* **Status**: **RESOLVED** (2026-10-09)
* **Affected Files & Locations**:
  - `modules/mod-playerbots-city-life/data/sql/manual/world_city_life.sql`
  - Table: `acore_world.city_life_hub`
* **Symptoms & Terminal Log Traces**:
  During worldserver initialization:
  ```text
  [CityLife] Missing WORLD table city_life_hub. Apply data/sql/manual/world_city_life.sql
  [CityLife] enable=0 database=0 hubs=0 maxTotal=300 timeScale=1 prefix='rndbot'
  ```
* **Root Cause Analysis**:
  The module relies on the table `city_life_hub` in the `acore_world` database to configure urban waypoints, hubs, and bot schedule intervals. When the table is absent, the module gracefully disables itself (`enable=0 database=0`).
* **Resolution**:
  Import `data/sql/manual/world_city_life.sql` into the `acore_world` database using the MySQL CLI or database manager when urban life behaviors are desired.
