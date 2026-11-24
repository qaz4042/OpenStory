//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "PetAI.h"

#include "MapleMap/MapDrops.h"
#include "MapleMap/Drop.h"
#include "../Configuration.h"

#include "../Character/Player.h"
#include "../Net/Packets/InventoryPackets.h"

#include <cmath>

namespace ms
{
	void PetAI::update(Player& player, MapDrops& drops)
	{
		const auto& inventory = player.get_inventory();
		const bool mesos = Setting<PetLootMesos>::get().load()
			&& inventory.find_item(InventoryType::Id::EQUIPPED, 1812000);
		const bool items = Setting<PetLootItems>::get().load()
			&& inventory.find_item(InventoryType::Id::EQUIPPED, 1812001);
		if (!mesos && !items)
		{
			for (uint8_t i = 0; i < 3; ++i)
				player.get_pet(i).clear_loot_target();
			reset();
			return;
		}
		if (--loot_cd > 0)
			return;
		loot_cd = 10; // Scan every 80 ms, including while a request is pending.

		MapObjects& objects = *drops.get_drops();
		for (auto it = retries.begin(); it != retries.end();)
		{
			it->second -= 10;
			if (it->second <= 0 || !objects.contains(it->first))
				it = retries.erase(it);
			else
				++it;
		}

		const auto cpos = player.get_position();
		for (uint8_t pi = 0; pi < 3; ++pi)
		{
			PetLook& pet = player.get_pet(pi);
			if (pet.get_itemid() == 0)
				continue;

			const auto ppos = pet.get_position();
			Drop* pickup = nullptr;
			Drop* target = nullptr;
			int best_pickup = 10000;
			int best_target = 10000;
			for (auto& entry : objects)
			{
				auto* drop = dynamic_cast<Drop*>(entry.second.get());
				if (!drop || !drop->can_pet_loot(mesos, items)
					|| retries.count(drop->get_oid()))
					continue;

				const auto dpos = drop->get_position();
				const int cx = std::abs(int(dpos.x()) - cpos.x());
				const int cy = std::abs(int(dpos.y()) - cpos.y());
				// Stay inside PetLook's 150 px follow leash; avoid other floors.
				if (cx * cx + cy * cy > 140 * 140 || cy > 60)
					continue;
				const int px = std::abs(int(dpos.x()) - ppos.x());
				const int py = std::abs(int(dpos.y()) - ppos.y());
				const int distance = px + py;
				// A small magnet area works even when the character is standing.
				if (px <= 100 && py <= 45 && distance < best_pickup)
				{
					pickup = drop;
					best_pickup = distance;
				}
				if (distance < best_target)
				{
					target = drop;
					best_target = distance;
				}
			}

			if (pickup)
			{
				PetLootPacket(pet.get_uniqueid(), pickup->get_oid()).dispatch();
				// Retry rejected/late requests without blocking other drops or pets.
				retries[pickup->get_oid()] = 60;
				pet.clear_loot_target();
			}
			else if (target)
				pet.set_loot_target(target->get_position());
			else
				pet.clear_loot_target();
		}
	}
}
