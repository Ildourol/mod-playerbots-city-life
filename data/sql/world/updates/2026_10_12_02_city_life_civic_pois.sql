-- mod-playerbots-city-life
-- Civic POIs Migration: Seeding Class Trainers, Food Vendors, Taverns, and Inn Beds across capitals
-- Idempotent script for AzerothCore WORLD database.

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SW_Trainer_OldTown', 'trainer', 1, -8765.0, 748.0, 97.5, 0.0, 5, 100, 4, 'OldTown', 0 FROM `city_life_hub` h WHERE h.name='Stormwind';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SW_Trainer_MageQuarter', 'trainer', 1, -9005.0, 865.0, 29.6, 0.0, 5, 100, 4, 'MageQuarter', 0 FROM `city_life_hub` h WHERE h.name='Stormwind';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SW_Vendor_TradeRations', 'vendor', 1, -8845.0, 655.0, 96.0, 0.0, 4, 100, 8, 'TradeDistrict', 0 FROM `city_life_hub` h WHERE h.name='Stormwind';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SW_Inn_TradeBed', 'bed', 1, -8864.0, 678.0, 101.5, 0.0, 3, 100, 10, 'TradeDistrict', 0 FROM `city_life_hub` h WHERE h.name='Stormwind';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'ORG_Trainer_Valley', 'trainer', 1, 1655.0, -4410.0, 16.0, 0.0, 5, 100, 4, 'ValleyOfStrength', 0 FROM `city_life_hub` h WHERE h.name='Orgrimmar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'ORG_Vendor_Rations', 'vendor', 1, 1615.0, -4430.0, 15.0, 0.0, 4, 100, 8, 'ValleyOfStrength', 0 FROM `city_life_hub` h WHERE h.name='Orgrimmar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'ORG_Inn_ValleyBed', 'bed', 1, 1638.0, -4440.0, 16.0, 0.0, 3, 100, 10, 'ValleyOfStrength', 0 FROM `city_life_hub` h WHERE h.name='Orgrimmar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Tavern_Military', 'tavern', 1, -4925.0, -925.0, 502.0, 0.0, 4, 100, 3, 'Commons', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Trainer_Commons', 'trainer', 1, -4890.0, -960.0, 502.0, 0.0, 5, 100, 4, 'Commons', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Vendor_Commons', 'vendor', 1, -4905.0, -930.0, 502.0, 0.0, 4, 100, 8, 'Commons', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Tavern_Center', 'tavern', 1, 1570.0, 250.0, -52.0, 0.0, 4, 100, 3, 'Center', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Trainer_Apothecary', 'trainer', 1, 1590.0, 260.0, -52.0, 0.0, 5, 100, 4, 'Center', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Vendor_Center', 'vendor', 1, 1575.0, 230.0, -52.0, 0.0, 4, 100, 8, 'Center', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'Dala_Tavern_Legerdemain', 'tavern', 1, 5830.0, 625.0, 647.0, 0.0, 4, 100, 3, 'Legerdemain', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'Dala_Trainer_Center', 'trainer', 1, 5850.0, 650.0, 647.0, 0.0, 5, 100, 4, 'Center', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'Dala_Vendor_Center', 'vendor', 1, 5810.0, 600.0, 647.0, 0.0, 4, 100, 8, 'Center', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';
