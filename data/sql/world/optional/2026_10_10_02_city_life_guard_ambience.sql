-- mod-playerbots-city-life
-- Optional Guard Ambiance SmartAI Scripts (Ported from mod-city-bots)
-- Apply to AzerothCore WORLD database. Completely optional flavor.

DELETE FROM `smart_scripts` WHERE `entryorguid` IN (68, 1976) AND `source_type` = 0 AND `id` BETWEEN 100 AND 104;

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(68, 0, 100, 0, 1, 0, 100, 1, 30000, 90000, 120000, 240000, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Stormwind Guard - random talk timer'),
(68, 0, 101, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 1, 25, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Stormwind Guard - emote talk'),
(68, 0, 102, 0, 61, 0, 50, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Stormwind Guard - say line'),
(68, 0, 103, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 24, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Stormwind Guard - evade reset'),
(1976, 0, 100, 0, 1, 0, 100, 1, 45000, 120000, 150000, 300000, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Stormwind Patroller - random talk timer'),
(1976, 0, 101, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Stormwind Patroller - emote bow'),
(1976, 0, 102, 0, 61, 0, 40, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Stormwind Patroller - say line'),
(1976, 0, 103, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 24, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Stormwind Patroller - evade reset');

-- Orgrimmar guards (entry 3296 Orgrimmar Grunt)
DELETE FROM `smart_scripts` WHERE `entryorguid` = 3296 AND `source_type` = 0 AND `id` BETWEEN 100 AND 104;

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(3296, 0, 100, 0, 1, 0, 100, 1, 30000, 90000, 120000, 240000, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Orgrimmar Grunt - random talk timer'),
(3296, 0, 101, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 1, 25, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Orgrimmar Grunt - emote talk'),
(3296, 0, 102, 0, 61, 0, 50, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Orgrimmar Grunt - say line'),
(3296, 0, 103, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 24, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'CityLife: Orgrimmar Grunt - evade reset');
