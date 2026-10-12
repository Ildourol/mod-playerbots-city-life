-- mod-playerbots-city-life
-- Full Capitals Civic POI Expansion Migration
-- Seeds canonical POIs (Banks, Auction Houses, Mailboxes, Taverns, Plazas, Trainers, Vendors, Docks)
-- across Ironforge, Undercity, Darnassus, Exodar, Thunder Bluff, Silvermoon, Shattrath, and Dalaran.
-- Also normalizes legacy category poi_types (mail -> 2, bank -> 11, auction -> 1).
-- Idempotent script for AzerothCore WORLD database.

-- 1. Normalize legacy category poi_types
UPDATE `city_life_spot` SET `poi_type` = 2 WHERE `category` IN ('mail', 'mailbox') AND `poi_type` != 2;
UPDATE `city_life_spot` SET `poi_type` = 11 WHERE `category` = 'bank' AND `poi_type` != 11;
UPDATE `city_life_spot` SET `poi_type` = 1 WHERE `category` = 'auction' AND `poi_type` != 1;
UPDATE `city_life_spot` SET `poi_type` = 3 WHERE `category` = 'tavern' AND `poi_type` = 0;

-- 2. Ironforge POIs
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Inn_GreatForge', 'inn', 1, -4845.0, -880.0, 502.0, 0.0, 4, 100, 0, 'GreatForge', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Auction_GreatForge', 'auction', 1, -4960.0, -910.0, 503.0, 0.0, 5, 100, 1, 'GreatForge', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Tavern_GreatForge', 'tavern', 1, -4850.0, -850.0, 502.0, 0.0, 4, 100, 3, 'GreatForge', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Plaza_MysticWard', 'plaza', 1, -4920.0, -990.0, 501.0, 0.0, 5, 100, 7, 'MysticWard', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Dock_Cavern1', 'dock', 1, -4638.0, -1114.0, 501.0, 0.0, 4, 100, 5, 'ForlornCavern', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Dock_Cavern2', 'dock', 1, -4644.0, -1119.0, 501.0, 0.0, 4, 100, 5, 'ForlornCavern', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Plaza_Explorers', 'plaza', 1, -4675.0, -1230.0, 501.0, 0.0, 5, 80, 7, 'HallOfExplorers', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Trainer_Military', 'trainer', 1, -5010.0, -1250.0, 501.0, 0.0, 5, 80, 4, 'MilitaryWard', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Vendor_CommonsMarket', 'vendor', 1, -4925.0, -955.0, 501.0, 0.0, 4, 100, 8, 'Commons', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Plaza_TinkerTown', 'plaza', 1, -4830.0, -1260.0, 501.0, 0.0, 5, 90, 7, 'TinkerTown', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Trainer_Mystic', 'trainer', 1, -4650.0, -960.0, 501.0, 0.0, 5, 80, 4, 'MysticWard', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Plaza_Cavern', 'plaza', 1, -4625.0, -1100.0, 501.0, 0.0, 5, 80, 7, 'ForlornCavern', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Plaza_GreatForge', 'plaza', 1, -4780.0, -1000.0, 502.0, 0.0, 5, 100, 7, 'GreatForge', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Plaza_Gates', 'plaza', 1, -5035.0, -820.0, 495.0, 0.0, 5, 70, 7, 'Gates', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Vendor_TinkerTown', 'vendor', 1, -4865.0, -1170.0, 501.0, 0.0, 4, 80, 8, 'TinkerTown', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'IF_Bank_Commons', 'bank', 1, -4890.0, -985.0, 501.0, 0.0, 5, 120, 11, 'Commons', 0 FROM `city_life_hub` h WHERE h.name='Ironforge';

-- 3. Undercity POIs
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Inn_MagicQuarter', 'inn', 1, 1633.0, 240.0, -43.0, 0.0, 4, 100, 0, 'MagicQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Auction_MagicQuarter', 'auction', 1, 1600.0, 170.0, -56.0, 0.0, 5, 100, 1, 'MagicQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Tavern_MagicQuarter', 'tavern', 1, 1630.0, 200.0, -43.0, 0.0, 4, 100, 3, 'MagicQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Plaza_MagicQuarter', 'plaza', 1, 1580.0, 260.0, -43.0, 0.0, 5, 100, 7, 'MagicQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Plaza_TradeQuarter', 'plaza', 1, 1590.0, 330.0, -43.0, 0.0, 5, 90, 7, 'TradeQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Vendor_RoguesQuarter', 'vendor', 1, 1560.0, 430.0, -43.0, 0.0, 4, 80, 8, 'RoguesQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Trainer_WarQuarter', 'trainer', 1, 1700.0, 90.0, -62.0, 0.0, 5, 80, 4, 'WarQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Plaza_Apothecarium', 'plaza', 1, 1460.0, 170.0, -62.0, 0.0, 5, 80, 7, 'Apothecarium', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Vendor_MagicQuarter', 'vendor', 1, 1685.0, 300.0, -43.0, 0.0, 4, 80, 8, 'MagicQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Plaza_CanalRing', 'plaza', 1, 1580.0, 410.0, -43.0, 0.0, 5, 80, 7, 'CanalRing', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Trainer_MagicQuarter', 'trainer', 1, 1660.0, 210.0, -43.0, 0.0, 5, 80, 4, 'MagicQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Plaza_CentralRing', 'plaza', 1, 1520.0, 250.0, -43.0, 0.0, 5, 90, 7, 'CentralRing', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Plaza_TradeOuter', 'plaza', 1, 1640.0, 360.0, -43.0, 0.0, 5, 80, 7, 'TradeQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Vendor_Apothecarium', 'vendor', 1, 1510.0, 115.0, -62.0, 0.0, 4, 70, 8, 'Apothecarium', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Plaza_InnerRing', 'plaza', 1, 1715.0, 245.0, -43.0, 0.0, 5, 80, 7, 'InnerRing', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'UC_Bank_MagicQuarter', 'bank', 1, 1595.0, 230.0, -43.0, 0.0, 5, 110, 11, 'MagicQuarter', 0 FROM `city_life_hub` h WHERE h.name='Undercity';

-- 4. Darnassus POIs
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Inn_Craftsmen', 'inn', 1, 9947.0, 2483.0, 1316.0, 0.0, 4, 100, 0, 'Craftsmen', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Tavern_Craftsmen', 'tavern', 1, 9940.0, 2510.0, 1316.0, 0.0, 4, 100, 3, 'Craftsmen', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Plaza_Temple', 'plaza', 1, 9920.0, 2550.0, 1316.0, 0.0, 5, 100, 7, 'Temple', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Auction_Tradesmens', 'auction', 1, 9860.0, 2330.0, 1315.0, 0.0, 5, 100, 1, 'TradesmensTerrace', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Mailbox_Tradesmens', 'mailbox', 1, 9865.0, 2355.0, 1315.0, 0.0, 4, 90, 2, 'TradesmensTerrace', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Trainer_Warriors', 'trainer', 1, 10040.0, 2530.0, 1316.0, 0.0, 5, 80, 4, 'WarriorsTerrace', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Vendor_Warriors', 'vendor', 1, 10110.0, 2570.0, 1316.0, 0.0, 4, 80, 8, 'WarriorsTerrace', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Plaza_TempleGardens', 'plaza', 1, 9650.0, 2520.0, 1336.0, 0.0, 5, 80, 7, 'TempleGardens', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Trainer_TempleGardens', 'trainer', 1, 9705.0, 2510.0, 1335.0, 0.0, 5, 80, 4, 'TempleGardens', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Plaza_Bridge', 'plaza', 1, 9790.0, 2590.0, 1316.0, 0.0, 5, 70, 7, 'Bridge', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Vendor_Cenarion', 'vendor', 1, 9910.0, 2260.0, 1328.0, 0.0, 4, 80, 8, 'CenarionEnclave', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Trainer_Cenarion', 'trainer', 1, 10080.0, 2205.0, 1328.0, 0.0, 5, 80, 4, 'CenarionEnclave', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Plaza_Cenarion', 'plaza', 1, 9945.0, 2185.0, 1328.0, 0.0, 5, 80, 7, 'CenarionEnclave', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Plaza_Pool', 'plaza', 1, 9750.0, 2435.0, 1316.0, 0.0, 5, 70, 7, 'Pool', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Vendor_TerraceMarket', 'vendor', 1, 10010.0, 2425.0, 1316.0, 0.0, 4, 80, 8, 'TerraceMarket', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DAR_Bank_Tradesmens', 'bank', 1, 9865.0, 2325.0, 1315.0, 0.0, 5, 110, 11, 'TradesmensTerrace', 0 FROM `city_life_hub` h WHERE h.name='Darnassus';

-- 5. Exodar POIs
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Inn_Seat', 'inn', 1, -3965.0, -11653.0, -139.0, 0.0, 4, 100, 0, 'Seat', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Tavern_Seat', 'tavern', 1, -3950.0, -11680.0, -139.0, 0.0, 4, 100, 3, 'Seat', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Plaza_Vault', 'plaza', 1, -4000.0, -11800.0, -100.0, 0.0, 5, 100, 7, 'Vault', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Auction_CrystalHall', 'auction', 1, -4020.0, -11720.0, -138.0, 0.0, 5, 100, 1, 'CrystalHall', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Mailbox_CrystalHall', 'mailbox', 1, -4050.0, -11710.0, -138.0, 0.0, 4, 90, 2, 'CrystalHall', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Trainer_TradersTier', 'trainer', 1, -3860.0, -11695.0, -137.0, 0.0, 5, 80, 4, 'TradersTier', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Vendor_Vault', 'vendor', 1, -3905.0, -11870.0, -106.0, 0.0, 4, 80, 8, 'Vault', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Plaza_Naaru', 'plaza', 1, -4200.0, -11580.0, -130.0, 0.0, 5, 80, 7, 'SeatOfNaaru', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Trainer_Hall', 'trainer', 1, -4215.0, -11740.0, -139.0, 0.0, 5, 80, 4, 'Trainers', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Plaza_Mystics', 'plaza', 1, -3840.0, -11590.0, -137.0, 0.0, 5, 80, 7, 'HallOfMystics', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Vendor_CrystalHall', 'vendor', 1, -4090.0, -11620.0, -137.0, 0.0, 4, 80, 8, 'CrystalHall', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Plaza_SeatApproach', 'plaza', 1, -3950.0, -11570.0, -137.0, 0.0, 5, 80, 7, 'SeatApproach', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Trainer_Vault', 'trainer', 1, -4100.0, -11830.0, -106.0, 0.0, 5, 70, 4, 'Vault', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Plaza_TradersTier', 'plaza', 1, -3875.0, -11785.0, -138.0, 0.0, 5, 80, 7, 'TradersTier', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Vendor_SeatMarket', 'vendor', 1, -4015.0, -11505.0, -137.0, 0.0, 4, 80, 8, 'SeatMarket', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'EX_Bank_CrystalHall', 'bank', 1, -3985.0, -11730.0, -138.0, 0.0, 5, 110, 11, 'CrystalHall', 0 FROM `city_life_hub` h WHERE h.name='Exodar';

-- 6. Thunder Bluff POIs
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Inn_ElderRise', 'inn', 1, -1196.0, 28.0, 177.0, 0.0, 4, 100, 0, 'ElderRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Tavern_ElderRise', 'tavern', 1, -1210.0, 50.0, 177.0, 0.0, 4, 100, 3, 'ElderRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Plaza_Pools', 'plaza', 1, -1250.0, 80.0, 177.0, 0.0, 5, 100, 7, 'Pools', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Auction_LowerRise', 'auction', 1, -1265.0, 125.0, 132.0, 0.0, 5, 100, 1, 'LowerRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Mailbox_LowerRise', 'mailbox', 1, -1278.0, 118.0, 132.0, 0.0, 4, 90, 2, 'LowerRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Trainer_HunterRise', 'trainer', 1, -1050.0, 250.0, 135.0, 0.0, 5, 80, 4, 'HunterRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Vendor_HunterRise', 'vendor', 1, -1010.0, 210.0, 134.0, 0.0, 4, 80, 8, 'HunterRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Plaza_SpiritRise', 'plaza', 1, -1390.0, -80.0, 158.0, 0.0, 5, 80, 7, 'SpiritRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Trainer_SpiritRise', 'trainer', 1, -1425.0, -55.0, 158.0, 0.0, 5, 80, 4, 'SpiritRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Plaza_Bridge', 'plaza', 1, -1220.0, -160.0, 132.0, 0.0, 5, 70, 7, 'Bridge', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Vendor_CentralRise', 'vendor', 1, -1180.0, 140.0, 132.0, 0.0, 4, 80, 8, 'CentralRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Plaza_CentralRise', 'plaza', 1, -1295.0, -10.0, 132.0, 0.0, 5, 90, 7, 'CentralRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Trainer_CentralRise', 'trainer', 1, -1110.0, -35.0, 132.0, 0.0, 5, 80, 4, 'CentralRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Plaza_NorthBridge', 'plaza', 1, -1090.0, 95.0, 132.0, 0.0, 5, 70, 7, 'Bridge', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Vendor_Market', 'vendor', 1, -1330.0, 170.0, 132.0, 0.0, 4, 80, 8, 'Market', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'TB_Bank_LowerRise', 'bank', 1, -1255.0, 125.0, 132.0, 0.0, 5, 110, 11, 'LowerRise', 0 FROM `city_life_hub` h WHERE h.name='ThunderBluff';

-- 7. Silvermoon POIs
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Inn_Sunfury', 'inn', 1, 9480.0, -7279.0, 14.0, 0.0, 4, 100, 0, 'Sunfury', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Tavern_Sunfury', 'tavern', 1, 9460.0, -7300.0, 14.0, 0.0, 4, 100, 3, 'Sunfury', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Plaza_Court', 'plaza', 1, 9500.0, -7200.0, 14.0, 0.0, 5, 100, 7, 'Court', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Auction_Bazaar', 'auction', 1, 9680.0, -7500.0, 15.0, 0.0, 5, 100, 1, 'Bazaar', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Mailbox_Bazaar', 'mailbox', 1, 9650.0, -7475.0, 15.0, 0.0, 4, 90, 2, 'Bazaar', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Trainer_RoyalExchange', 'trainer', 1, 9605.0, -7380.0, 14.0, 0.0, 5, 80, 4, 'RoyalExchange', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Vendor_Court', 'vendor', 1, 9580.0, -7210.0, 14.0, 0.0, 4, 80, 8, 'Court', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Plaza_MurderRow', 'plaza', 1, 9850.0, -7285.0, 14.0, 0.0, 5, 80, 7, 'MurderRow', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Trainer_MurderRow', 'trainer', 1, 9800.0, -7335.0, 14.0, 0.0, 5, 80, 4, 'MurderRow', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Plaza_Elders', 'plaza', 1, 9525.0, -6860.0, 16.0, 0.0, 5, 80, 7, 'WalkOfElders', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Vendor_Gate', 'vendor', 1, 9410.0, -7100.0, 15.0, 0.0, 4, 70, 8, 'ShepherdsGate', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Plaza_Exchange', 'plaza', 1, 9750.0, -7050.0, 15.0, 0.0, 5, 90, 7, 'RoyalExchange', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Trainer_Farstriders', 'trainer', 1, 9900.0, -7420.0, 14.0, 0.0, 5, 80, 4, 'FarstridersSquare', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Plaza_Promenade', 'plaza', 1, 9680.0, -7150.0, 14.0, 0.0, 5, 80, 7, 'Promenade', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Vendor_Bazaar', 'vendor', 1, 9550.0, -7440.0, 14.0, 0.0, 4, 80, 8, 'Bazaar', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SMC_Bank_Bazaar', 'bank', 1, 9640.0, -7490.0, 15.0, 0.0, 5, 110, 11, 'Bazaar', 0 FROM `city_life_hub` h WHERE h.name='Silvermoon';

-- 8. Shattrath POIs
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Inn_Aldor', 'inn', 1, -1838.0, 5301.0, -12.0, 0.0, 4, 100, 0, 'Aldor', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Tavern_Scryer', 'tavern', 1, -1900.0, 5450.0, -12.0, 0.0, 4, 100, 3, 'Scryer', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Plaza_Terrace', 'plaza', 1, -1850.0, 5400.0, -12.0, 0.0, 5, 100, 7, 'Terrace', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Mailbox_Terrace', 'mailbox', 1, -1865.0, 5410.0, -12.0, 0.0, 4, 90, 2, 'Terrace', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Vendor_LowerCity', 'vendor', 1, -1710.0, 5490.0, -12.0, 0.0, 4, 90, 8, 'LowerCity', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Plaza_LowerCity', 'plaza', 1, -1600.0, 5260.0, -39.0, 0.0, 5, 80, 7, 'LowerCity', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Trainer_ScryerRise', 'trainer', 1, -2000.0, 5350.0, -8.0, 0.0, 5, 80, 4, 'ScryerRise', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Plaza_ScryerRise', 'plaza', 1, -2100.0, 5295.0, -8.0, 0.0, 5, 80, 7, 'ScryerRise', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Trainer_AldorRise', 'trainer', 1, -1745.0, 5760.0, 130.0, 0.0, 5, 80, 4, 'AldorRise', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Plaza_AldorRise', 'plaza', 1, -1800.0, 5830.0, 130.0, 0.0, 5, 80, 7, 'AldorRise', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Vendor_Terrace', 'vendor', 1, -1885.0, 5200.0, -12.0, 0.0, 4, 80, 8, 'Terrace', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Plaza_Bridge', 'plaza', 1, -1980.0, 5560.0, -12.0, 0.0, 5, 70, 7, 'Bridge', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Plaza_LowerMarket', 'plaza', 1, -1685.0, 5350.0, -12.0, 0.0, 5, 80, 7, 'LowerCityMarket', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Vendor_TerraceMarket', 'vendor', 1, -1815.0, 5515.0, -12.0, 0.0, 4, 80, 8, 'TerraceMarket', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Plaza_Central', 'plaza', 1, -1915.0, 5310.0, -12.0, 0.0, 5, 90, 7, 'CentralTerrace', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'SHAT_Bank_Terrace', 'bank', 1, -1855.0, 5425.0, -12.0, 0.0, 5, 100, 11, 'Terrace', 0 FROM `city_life_hub` h WHERE h.name='Shattrath';

-- 9. Dalaran POIs
INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Inn_Sewers', 'inn', 1, 5807.0, 588.0, 661.0, 0.0, 4, 100, 0, 'Sewers', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Tavern_PurpleParlor', 'tavern', 1, 5850.0, 650.0, 647.0, 0.0, 4, 100, 3, 'PurpleParlor', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Plaza_Krasus', 'plaza', 1, 5750.0, 700.0, 655.0, 0.0, 5, 100, 7, 'Krasus', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Auction_MagusCommerce', 'auction', 1, 5890.0, 620.0, 650.0, 0.0, 5, 100, 1, 'MagusCommerce', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Mailbox_MagusCommerce', 'mailbox', 1, 5880.0, 642.0, 650.0, 0.0, 4, 90, 2, 'MagusCommerce', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Vendor_Runeweaver', 'vendor', 1, 5820.0, 480.0, 658.0, 0.0, 4, 80, 8, 'Runeweaver', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Trainer_Krasus', 'trainer', 1, 5775.0, 620.0, 650.0, 0.0, 5, 80, 4, 'Krasus', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Plaza_AllianceEnclave', 'plaza', 1, 5710.0, 520.0, 652.0, 0.0, 5, 80, 7, 'AllianceQuarter', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Plaza_Sunreavers', 'plaza', 1, 5905.0, 470.0, 652.0, 0.0, 5, 80, 7, 'HordeQuarter', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Trainer_Citadel', 'trainer', 1, 5810.0, 760.0, 640.0, 0.0, 5, 80, 4, 'VioletCitadel', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Vendor_Citadel', 'vendor', 1, 5730.0, 770.0, 640.0, 0.0, 4, 80, 8, 'VioletCitadel', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Plaza_Eventide', 'plaza', 1, 5680.0, 650.0, 647.0, 0.0, 5, 80, 7, 'Eventide', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Plaza_Landing', 'plaza', 1, 5780.0, 830.0, 680.0, 0.0, 5, 70, 7, 'Landing', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Vendor_Market', 'vendor', 1, 5855.0, 735.0, 640.0, 0.0, 4, 80, 8, 'Market', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Plaza_CentralLoop', 'plaza', 1, 5810.0, 690.0, 647.0, 0.0, 5, 90, 7, 'CentralLoop', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';

INSERT IGNORE INTO `city_life_spot` (`hub_id`, `name`, `category`, `enabled`, `x`, `y`, `z`, `o`, `radius`, `weight`, `poi_type`, `district`, `partner_spot_id`)
SELECT h.id, 'DALA_Bank_MagusCommerce', 'bank', 1, 5885.0, 608.0, 650.0, 0.0, 5, 110, 11, 'MagusCommerce', 0 FROM `city_life_hub` h WHERE h.name='Dalaran';
