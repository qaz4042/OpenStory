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
#include "CashShopHandlers.h"
#include <algorithm>

#include "Helpers/CashShopParser.h"
#include "Helpers/ItemParser.h"

#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/UIChatBar.h"
#include "../../IO/UITypes/UIStatusMessenger.h"
#include "../../IO/UITypes/UIMTS.h"
#include "../../IO/Window.h"

namespace ms
{
	static std::vector<CashInventoryItem> cash_inventory;

	void SetCashShopHandler::handle(InPacket& recv) const
	{
		cash_inventory.clear();
		CashShopParser::parseCharacterInfo(recv);

		recv.skip_byte();	// Not MTS
		recv.skip_string();	// account_name
		recv.skip_int();

		int16_t specialcashitem_size = recv.read_short();

		for (size_t i = 0; i < specialcashitem_size; i++)
		{
			recv.skip_int();	// sn
			recv.skip_int();	// mod
			recv.skip_byte();	// info
		}

		recv.skip(121);

		for (size_t cat = 1; cat <= 8; cat++)
		{
			for (size_t gender = 0; gender < 2; gender++)
			{
				for (size_t in = 0; in < 5; in++)
				{
					recv.skip_int(); // category
					recv.skip_int(); // gender
					recv.skip_int(); // commoditysn
				}
			}
		}

		recv.skip_int();
		recv.skip_short();
		recv.skip_byte();
		recv.skip_int();

		transition();
	}


	const std::vector<CashInventoryItem>& get_cash_inventory() { return cash_inventory; }

	static CashInventoryItem read_cash_item(InPacket& recv)
	{
		CashInventoryItem item;
		item.cash_id = recv.read_long();
		recv.skip(8); // account ID and reserved int
		item.item_id = recv.read_int();
		item.sn = recv.read_int();
		item.count = static_cast<uint16_t>(recv.read_short());
		recv.skip(29); // gift sender (13), expiration (8), reserved long (8)
		return item;
	}

	static void add_cash_item(const CashInventoryItem& item)
	{
		for (auto& entry : cash_inventory)
			if (entry.cash_id == item.cash_id) { entry = item; return; }
		cash_inventory.push_back(item);
	}

	void CashShopOperationHandler::handle(InPacket& recv) const
	{
		uint8_t operation = static_cast<uint8_t>(recv.read_byte());

		switch (operation)
		{
		case 0x4B: // Cosmic showCashInventory: complete account locker snapshot
		{
			const uint16_t count = static_cast<uint16_t>(recv.read_short());
			std::vector<CashInventoryItem> snapshot;
			for (uint16_t i = 0; i < count; ++i) snapshot.push_back(read_cash_item(recv));
			recv.skip(4); // storage and character slot counts
			cash_inventory = std::move(snapshot);
			break;
		}
		case 0x57: // Cosmic showBoughtCashItem (not 0x4A)
		case 0x6A: // putIntoCashInventory
			add_cash_item(read_cash_item(recv));
			break;
		case 0x89: // package purchase
		{
			const uint8_t count = static_cast<uint8_t>(recv.read_byte());
			std::vector<CashInventoryItem> purchased;
			for (uint8_t i = 0; i < count; ++i) purchased.push_back(read_cash_item(recv));
			recv.skip(2);
			for (const auto& item : purchased) add_cash_item(item);
			break;
		}
		case 0x68: // Cosmic takeFromCashInventory: target slot + full item info
		{
			const int16_t slot = recv.read_short();
			InPacket header = recv;
			header.read_byte();
			const int32_t item_id = header.read_int();
			const bool cash = header.read_bool();
			const int64_t cash_id = cash ? header.read_long() : 0;
			// Validate the complete item before changing either inventory.
			InPacket validate = recv;
			ItemParser::skim_item(validate);
			ItemParser::parse_item(recv, InventoryType::by_item_id(item_id), slot,
				Stage::get().get_player().get_inventory());
			cash_inventory.erase(std::remove_if(cash_inventory.begin(), cash_inventory.end(),
				[cash_id](const auto& item) { return item.cash_id == cash_id; }), cash_inventory.end());
			break;
		}

		case 0x6C: // deleted cash item
		{
			const auto id = recv.read_long();
			cash_inventory.erase(std::remove_if(cash_inventory.begin(), cash_inventory.end(),
				[id](const auto& item) { return item.cash_id == id; }), cash_inventory.end());
			break;
		}

		case 0x4A:
		{
			// Buy item success
			chat::log("[Cash Shop] Item purchased successfully!", chat::LineType::YELLOW);
			break;
		}
		case 0x5C: // Cosmic showCashShopMessage
		case 0x4C:
		{
			const uint8_t reason = static_cast<uint8_t>(recv.read_byte());
			std::string msg;
			switch (reason)
			{
			case 0xA5: msg = "Not enough NX."; break;
			case 0xAA: msg = "This item has a gender restriction."; break;
			case 0xAC: case 0xBB: msg = "Cash inventory is full."; break;
			case 0xB0: msg = "Invalid coupon code."; break;
			case 0xB2: msg = "This coupon has expired."; break;
			case 0xB3: msg = "This coupon has already been used."; break;
			case 0xC0: msg = "Item is out of stock."; break;
			default: msg = "Cash shop operation failed (code " + std::to_string(reason) + ")."; break;
			}
			chat::log("[Cash Shop] " + msg, chat::LineType::RED);
			break;
		}

		case 0x59:
		{
			chat::log("[Cash Shop] Coupon redeemed successfully!", chat::LineType::YELLOW);
			break;
		}
		default:
			break;
		}
	}

	void SetITCHandler::handle(InPacket& recv) const
	{
		// SET_ITC — MTS transition
		// Same as SET_CASH_SHOP but with MTS trailing data instead of CS items
		CashShopParser::parseCharacterInfo(recv);

		recv.skip_string();	// account_name

		// MTS config bytes (hardcoded by Cosmic)
		recv.skip(28);

		// Open the MTS UI
		UI::get().emplace<UIMTS>();
	}

	// Holds the UI scale that was active immediately before the cash shop
	// transition forced scale=1.0. UICashShop reads this back via
	// get_pre_cashshop_ui_scale() when the player leaves the shop.
	static float s_pre_cashshop_ui_scale = 1.0f;

	float get_pre_cashshop_ui_scale()
	{
		return s_pre_cashshop_ui_scale;
	}

	void SetCashShopHandler::transition() const
	{
		// The cash shop UI is authored at 1:1 for a 1024x768 physical window.
		// Setting UI_SCALE to 1.0 makes the logical view match the physical
		// window so the hard-coded button positions line up with the cursor
		// (cursor_callback divides physical coords by get_viewwidth() /
		// get_viewheight(), which at scale=1.0 equal the physical size).
		s_pre_cashshop_ui_scale = Constants::Constants::get().get_ui_scale();
		Constants::Constants::get().set_ui_scale(1.0f);

		// Drop any focused textfield (e.g. the chat bar) so the arrow
		// keys reach the cash shop stage character
		UI::get().remove_textfield();

		Constants::Constants::get().set_viewwidth(1024);
		Constants::Constants::get().set_viewheight(768);

		float fadestep = 0.025f;

		Window::get().fadeout(
			fadestep,
			[]()
			{
				GraphicsGL::get().clear();

				Stage::get().load(-1, 0);

				GraphicsGL::get().unlock();

				UI::get().change_state(UI::State::CASHSHOP);
				UI::get().enable();
				Timer::get().start();
			}
		);

		GraphicsGL::get().lock();
		Stage::get().clear();
		Timer::get().start();
	}

	static int32_t s_cash_balances[3] = { 0, 0, 0 };

	int32_t get_cash_balance(int which)
	{
		return (which >= 0 && which < 3) ? s_cash_balances[which] : 0;
	}

	void QueryCashResultHandler::handle(InPacket& recv) const
	{
		// Cash query result — the balances render in the cash shop's
		// left panel next to the baked NX labels
		if (recv.available())
		{
			s_cash_balances[0] = recv.read_int(); // NX credit
			s_cash_balances[1] = recv.read_int(); // maple points
			s_cash_balances[2] = recv.read_int(); // NX prepaid
		}
	}

	void CashShopNameChangeHandler::handle(InPacket& recv) const
	{
		std::string name = recv.read_string();
		bool in_use = recv.read_bool();

		if (auto messenger = UI::get().get_element<UIStatusMessenger>())
		{
			if (in_use)
				messenger->show_status(Color::Name::RED, "Name '" + name + "' is already in use.");
			else
				messenger->show_status(Color::Name::WHITE, "Name '" + name + "' is available!");
		}
	}

	void CashShopNameChangePossibleHandler::handle(InPacket& recv) const
	{
		recv.read_int(); // 0
		int8_t error = recv.read_byte();
		recv.read_int(); // 0

		if (error == 0)
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::WHITE, "Name change is available.");
		}
		else
		{
			std::string msg;

			switch (error)
			{
			case 1: msg = "A name change has already been submitted."; break;
			case 2: msg = "Must wait at least one month between name changes."; break;
			case 3: msg = "Cannot change name due to a recent ban."; break;
			default: msg = "Name change is not available."; break;
			}

			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, msg);
		}
	}

	void CashShopTransferWorldHandler::handle(InPacket& recv) const
	{
		recv.read_int(); // 0
		int8_t error = recv.read_byte();
		recv.read_int(); // 0
		bool ok = recv.read_bool();

		if (ok)
		{
			int32_t world_count = recv.read_int();
			std::string world_list = "[CashShop] Available worlds: ";

			for (int32_t i = 0; i < world_count; i++)
			{
				std::string world_name = recv.read_string();

				if (i > 0)
					world_list += ", ";

				world_list += world_name;
			}

			chat::log(world_list, chat::LineType::YELLOW);
		}
		else if (error != 0)
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, "World transfer failed (error=" + std::to_string(error) + ").");
		}
	}

	void CashGachaponResultHandler::handle(InPacket& recv) const
	{
		int8_t op = recv.read_byte();

		if (op == (int8_t)0xE4) // Open failed
		{
			if (auto messenger = UI::get().get_element<UIStatusMessenger>())
				messenger->show_status(Color::Name::RED, "Gachapon failed to open.");
		}
		else if (op == (int8_t)0xE5) // Open success
		{
			recv.read_long(); // box cash id
			int32_t remaining = recv.read_int();

			chat::log("[Gachapon] Opened! Remaining: " + std::to_string(remaining), chat::LineType::YELLOW);
			// CashItemInfo + reward data follows
		}
	}
}