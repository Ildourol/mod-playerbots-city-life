# Debugging & Diagnostic Playbook: `mod-playerbots-city-life`

> **Component:** `mod-playerbots-city-life` (Playerbots City Life & Ambient Roleplay)  
> **Target:** AzerothCore 3.3.5a (Build 12340)  
> **Module Issue Tracker:** [`mod-playerbots-city-life/docs/ISSUES.md`](file:///C:/Users/Admin/AntigravityProfiles/Projects Azerothcore/Azerothcore modules/mod-playerbots-city-life/docs/ISSUES.md)

---

## 1. Quick Diagnostic Checklist

When encountering errors, unexpected behavior, or performance drops related to `mod-playerbots-city-life`:
1. **Worldserver Console**: Look for module-specific error tags or warnings during boot (`OnConfigLoad`, `OnStartup`).
2. **Errors Log**: Inspect `Server/bin/Errors.log` for SQL query syntax errors or failed table queries.
3. **Database Consistency**: Check that required tables in `characters` or `world` exist and have expected columns.
4. **Configuration Check**: Ensure `conf/mod-playerbots-city-life.conf.dist` and active `Server/bin/configs/modules/mod-playerbots-city-life.conf` match.

---

## 2. Common Symptoms & Root Causes

### A. Server Crash or Access Violation During World Tick
* **Symptom**: `EXCEPTION_ACCESS_VIOLATION` in module script hooks.
* **Root Cause**: Holding raw entity pointers across ticks or accessing despawned units.
* **Resolution**: Re-fetch entity freshly via numeric GUID (`ObjectAccessor::FindPlayer(guid)` or creature accessor) and verify `!entity || !entity->IsInWorld()`.

### B. Module Inactive or Commands Missing
* **Symptom**: Module chat commands return unknown command or features do not trigger.
* **Root Cause**: Module disabled in configuration (`.Enable = 0`) or configuration key missing.
* **Resolution**: Check active `.conf` file and verify boolean toggles.

### C. Database Query Failures
* **Symptom**: `Table doesn't exist` or `Unknown column` in `Errors.log`.
* **Root Cause**: Unapplied or missing SQL migrations in `data/sql/`.
* **Resolution**: Apply idempotent migration from `data/sql/` manually or check database connection string.

---

## 3. Module Specific Invariants
- **Invariant**: Pathfinding routes inside cities must use standard ACore mmaps without falling through terrain.
- **Invariant**: City life behaviors must disengage instantly if the bot is invited to a party or commanded by an owner.
- **Invariant**: CPU usage must be strictly throttled when large numbers of ambient bots occupy the same cell.

---

## 4. Useful Diagnostic SQL Queries

```sql
-- Check character data associated with this module
SELECT * FROM information_schema.tables 
WHERE table_schema = DATABASE() 
  AND table_name LIKE '%playerbots_city_life%';
```
