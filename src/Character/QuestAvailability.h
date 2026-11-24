#pragma once

#include "QuestLog.h"
#include "Inventory/Inventory.h"
#include <nlnx/node.hpp>

namespace ms
{
	// Mirror Cosmic's JobRequirement, QuestRequirement and ItemRequirement.
	// Level/location filtering stays with the caller (quest-log filters differ).
	inline bool quest_start_requirements_met(nl::node check, int32_t job,
		const QuestLog& quests, const Inventory& inventory)
	{
		if (auto jobs = check["job"]; jobs && jobs.size() > 0)
		{
			bool matches = false;
			for (auto required : jobs)
				if (required.get_integer() == job) matches = true;
			if (!matches) return false; // Job 0 means Beginner, not all jobs.
		}
		for (auto required : check["quest"])
		{
			const int32_t id = required["id"].get_integer();
			if (id <= 0 || id > 32767) return false;
			const int state = quests.get_completed().count(id) ? 2
				: quests.get_started().count(id) ? 1 : 0;
			if (state != required["state"].get_integer()) return false;
		}
		for (auto required : check["item"])
		{
			const int32_t id = required["id"].get_integer();
			if (id <= 0 || InventoryType::by_item_id(id) == InventoryType::NONE) return false;
			const int count = inventory.get_total_item_count(id);
			const int needed = required["count"].get_integer();
			// Cosmic counts the backpack, not worn equipment. Zero means absent.
			if (count < needed || (needed <= 0 && count > 0)) return false;
		}
		return true;
	}
}
