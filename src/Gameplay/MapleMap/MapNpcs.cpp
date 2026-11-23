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
#include "MapNpcs.h"

#include "Npc.h"
#include <cmath>

#include "../../Net/NpcResponseTracker.h"
#include "../../Net/Packets/NpcInteractionPackets.h"
#include "../../IO/UI.h"

namespace ms
{
	void MapNpcs::draw(Layer::Id layer, double viewx, double viewy, float alpha) const
	{
		npcs.draw(layer, viewx, viewy, alpha);
	}

	void MapNpcs::draw_names(Layer::Id layer, double viewx, double viewy, float alpha) const
	{
		for (auto it = npcs.begin(); it != npcs.end(); ++it)
		{
			auto* npc = static_cast<const Npc*>(it->second.get());

			if (npc && npc->is_active() && npc->get_layer() == layer)
				npc->draw_name(viewx, viewy, alpha);
		}
	}

	void MapNpcs::update(const Physics& physics)
	{
		for (; !spawns.empty(); spawns.pop())
		{
			const NpcSpawn& spawn = spawns.front();

			int32_t oid = spawn.get_oid();
			Optional<MapObject> npc = npcs.get(oid);

			if (npc)
				npc->makeactive();
			else
				npcs.add(spawn.instantiate(physics));
		}

		npcs.update(physics);

		// Expire stale TalkToNPC requests. Refresh indicators whenever the
		// tracker's revision changes — covers both new unavailability (tick
		// flagged a timeout) and recovery (a response cleared an NPC that
		// was previously flagged unavailable, restoring its bulb).
		NpcResponseTracker::get().tick();

		uint32_t rev = NpcResponseTracker::get().revision();
		if (rev != last_tracker_rev)
		{
			last_tracker_rev = rev;
			refresh_quest_marks();
		}
	}

	void MapNpcs::spawn(NpcSpawn&& spawn)
	{
		spawns.emplace(std::move(spawn));
	}

	void MapNpcs::remove(int32_t oid)
	{
		if (auto npc = npcs.get(oid))
			npc->deactivate();
	}

	void MapNpcs::clear()
	{
		npcs.clear();

		// A queued spawn belongs to the map it arrived on. clear() runs on the
		// map change *before* the new map loads, so anything still queued here
		// is stale -- left in place it materialised on the next map.
		spawns = {};
	}

	MapObjects * MapNpcs::get_npcs()
	{
		return &npcs;
	}

	void MapNpcs::refresh_quest_marks()
	{
		for (auto& map_object : npcs)
		{
			Npc* npc = static_cast<Npc*>(map_object.second.get());
			if (npc && npc->is_active())
				npc->update_quest_mark();
		}
	}

	int32_t MapNpcs::find_talk_target(Point<int16_t> playerpos) const
	{
		int32_t target = 0;
		int32_t best = 150 * 150 + 80 * 80 + 1;
		for (const auto& entry : npcs)
		{
			const auto* npc = static_cast<const Npc*>(entry.second.get());
			if (!npc || !npc->is_active() || !npc->allows_keyboard_talk()) continue;
			const auto pos = npc->get_position();
			const int32_t dx = int32_t(pos.x()) - playerpos.x();
			const int32_t dy = int32_t(pos.y()) - playerpos.y();
			if (std::abs(dx) > 150 || std::abs(dy) > 80) continue;
			const int32_t distance = dx * dx + dy * dy;
			if (distance < best || (distance == best && npc->get_oid() < target))
			{
				best = distance;
				target = npc->get_oid();
			}
		}
		return target;
	}

	void MapNpcs::talk_nearest(Point<int16_t> playerpos)
	{
		if (const int32_t oid = find_talk_target(playerpos)) request_talk(oid);
	}

	void MapNpcs::request_talk(int32_t oid)
	{
		auto npc = npcs.get(oid);
		if (!npc || !npc->is_active() || !NpcResponseTracker::get().can_click_now()) return;
		// Share the Cosmic cooldown and response tracking with mouse clicks.
		NpcResponseTracker::get().mark_clicked_now();
		NpcResponseTracker::get().mark_pending(static_cast<Npc*>(npc.get())->get_npcid());
		TalkToNPCPacket(oid).dispatch();
	}

	Cursor::State MapNpcs::send_cursor(bool pressed, Point<int16_t> position, Point<int16_t> viewpos)
	{
		for (auto& map_object : npcs)
		{
			Npc* npc = static_cast<Npc*>(map_object.second.get());

			if (npc && npc->is_active() && npc->inrange(position, viewpos))
			{
				if (pressed)
				{
					request_talk(npc->get_oid());

					return Cursor::State::IDLE;
				}
				else
				{
					return Cursor::State::CANCLICK;
				}
			}
		}

		return Cursor::State::IDLE;
	}
}