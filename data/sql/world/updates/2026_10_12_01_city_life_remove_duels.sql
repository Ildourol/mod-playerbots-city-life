-- City Life
-- Remove duel hubs and duel ring locations from world database

DELETE FROM `city_life_spot` WHERE `category` IN ('duel', 'duelring') OR `poi_type` = 9;
DELETE FROM `city_life_hub` WHERE `name` IN ('StormwindGate', 'OrgrimmarGate', 'GoldshireDuel');
