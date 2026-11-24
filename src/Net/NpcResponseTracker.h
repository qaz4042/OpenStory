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
#pragma once

#include "../Template/Singleton.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <unordered_set>

namespace ms
{
	// Tracks pending TalkToNPC requests and mirrors the server click cooldown.
	// A timeout only expires a request; it must not change quest availability.
	class NpcResponseTracker : public Singleton<NpcResponseTracker>
	{
	public:
		// Record that the client just sent a TalkToNPC packet for this NPC.
		void mark_pending(int32_t npcid);

		// Clear a pending entry because the server responded (dialog/shop).
		void clear_pending(int32_t npcid);

		// Expire stale pending entries without declaring an NPC unavailable.
		// Return value retained for existing callers; timeout alone returns false.
		bool tick();

		bool is_unavailable(int32_t npcid) const;

		const std::unordered_set<int32_t>& get_unavailable() const { return unavailable_; }

		// Monotonic revision that increments every time the unavailable set
		// changes. UI code can compare against a cached value to know when
		// to re-run filters.
		uint32_t revision() const { return revision_; }

		// Called when changing maps / logging out.
		void clear_all();

		// Cosmic-side rate limit: TalkToNPC packets sent within
		// BLOCK_NPC_RACE_CONDT (default 500ms) of the previous one
		// are silently dropped — no dialogue, no enable_actions.
		// We mirror that on the client by suppressing rapid re-clicks.
		// Returns true if enough time has elapsed since the last
		// successful click for a fresh TalkToNPC to be sent.
		bool can_click_now() const;

		// Record the moment we just sent a TalkToNPC. Resets the
		// cooldown clock used by can_click_now().
		void mark_clicked_now();

	private:
		using Clock = std::chrono::steady_clock;
		static constexpr auto TIMEOUT = std::chrono::milliseconds(3000);
		// Mirror Cosmic's canClickNPC (500ms) with a tiny safety
		// margin so server-side jitter doesn't push us inside the
		// drop window.
		static constexpr auto CLICK_COOLDOWN = std::chrono::milliseconds(600);

		std::map<int32_t, Clock::time_point> pending_;
		std::unordered_set<int32_t> unavailable_;
		uint32_t revision_ = 0;
		Clock::time_point last_click_ = Clock::time_point::min();
	};
}
