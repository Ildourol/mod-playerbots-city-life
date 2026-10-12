-- mod-playerbots-city-life
-- Seed canonical class trainer districts and dock POIs across all major capitals
-- Matches fallback POI registry from mod-city-bots
-- Idempotent script for AzerothCore WORLD database.

-- Stormwind
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SW_Trainer_DwarvenDistrict', 'trainer', 1, -8425.0, 610.0, 95.0, 0.0, 5, 80, 4, 'DwarvenDistrict', 0 FROM `city_life_hub` h WHERE h.name='Stormwind';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SW_Trainer_Cathedral', 'trainer', 1, -8575.0, 880.0, 106.0, 0.0, 5, 80, 4, 'CathedralSquare', 0 FROM `city_life_hub` h WHERE h.name='Stormwind';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SW_Dock_Harbor', 'dock', 1, -8642.0, 1336.0, 5.3, 3.14, 4, 80, 5, 'Harbor', 0 FROM `city_life_hub` h WHERE h.name='Stormwind';

-- Orgrimmar
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'ORG_Trainer_Strength', 'trainer', 1, 1720.0, -4310.0, 33.0, 0.0, 5, 100, 4, 'ValleyOfStrength', 0 FROM `city_life_hub` h WHERE h.name='Orgrimmar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'ORG_Trainer_Spirits', 'trainer', 1, 1450.0, -4185.0, 44.0, 0.0, 5, 80, 4, 'ValleyOfSpirits', 0 FROM `city_life_hub` h WHERE h.name='Orgrimmar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'ORG_Trainer_Wisdom', 'trainer', 1, 1938.0, -4675.0, 32.0, 0.0, 5, 80, 4, 'ValleyOfWisdom', 0 FROM `city_life_hub` h WHERE h.name='Orgrimmar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'ORG_Trainer_Shadow', 'trainer', 1, 1835.0, -4370.0, -16.0, 0.0, 5, 80, 4, 'CleftOfShadow', 0 FROM `city_life_hub` h WHERE h.name='Orgrimmar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'ORG_Dock_Spirits', 'dock', 1, 1960.0, -4180.0, 25.0, 0.0, 4, 80, 5, 'ValleyOfSpirits', 0 FROM `city_life_hub` h WHERE h.name='Orgrimmar';

-- Darnassus
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Trainer_Warriors', 'trainer', 1, 10040.0, 2530.0, 1316.0, 0.0, 5, 80, 4, 'WarriorTerrace', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Trainer_Temple', 'trainer', 1, 9705.0, 2510.0, 1335.0, 0.0, 5, 80, 4, 'TempleMoon', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Trainer_Enclave', 'trainer', 1, 10080.0, 2205.0, 1328.0, 0.0, 5, 80, 4, 'CenarionEnclave', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Dock_Ruttheran', 'dock', 1, 9870.0, 2580.0, 1316.0, 0.0, 4, 80, 5, 'RuttheranVillage', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

-- Thunder Bluff
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Trainer_Hunter', 'trainer', 1, -1050.0, 250.0, 135.0, 0.0, 5, 80, 4, 'HunterRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Trainer_Spirit', 'trainer', 1, -1425.0, -55.0, 158.0, 0.0, 5, 80, 4, 'SpiritRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Trainer_Central', 'trainer', 1, -1110.0, -35.0, 132.0, 0.0, 5, 80, 4, 'CentralMesa', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Dock_Lower', 'dock', 1, -1300.0, -20.0, 125.0, 0.0, 4, 80, 5, 'LowerRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

-- Silvermoon
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Trainer_Royal', 'trainer', 1, 9605.0, -7380.0, 14.0, 0.0, 5, 80, 4, 'RoyalExchange', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Trainer_Murder', 'trainer', 1, 9800.0, -7335.0, 14.0, 0.0, 5, 80, 4, 'MurderRow', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Trainer_Farstriders', 'trainer', 1, 9900.0, -7420.0, 14.0, 0.0, 5, 80, 4, 'FarstridersSquare', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

-- Shattrath
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Trainer_Scryer', 'trainer', 1, -2000.0, 5350.0, -8.0, 0.0, 5, 80, 4, 'ScryersTier', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Trainer_Aldor', 'trainer', 1, -1745.0, 5760.0, 130.0, 0.0, 5, 80, 4, 'AldorRise', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

-- Dalaran
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Trainer_Krasus', 'trainer', 1, 5775.0, 620.0, 650.0, 0.0, 5, 80, 4, 'KrasusLanding', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Trainer_Violet', 'trainer', 1, 5810.0, 760.0, 640.0, 0.0, 5, 80, 4, 'VioletHold', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';
