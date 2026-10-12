-- mod-playerbots-city-life
-- Set default population and status for Wintergrasp hubs to 0 (disabled)
-- Ensures no bots are deployed to Wintergrasp camps by default.
-- Idempotent script for AzerothCore WORLD database.

UPDATE `city_life_hub`
SET `enabled` = 0, `default_population` = 0
WHERE `name` IN ('WintergraspAlliance', 'WintergraspHorde');
