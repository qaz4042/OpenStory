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
#include "UIEquipInventory.h"

#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../UITypes/UIItemInventory.h"

#include "../../Audio/Audio.h"
#include "../../Data/ItemData.h"

#include "../../Net/Packets/InventoryPackets.h"
#include "../../Net/Packets/PlayerPackets.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Graphics/Geometry.h"
#include "../../Gameplay/PetSettings.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#include <nlnx/bitmap.hpp>
#endif

namespace ms
{
    namespace {
        // Preserve the game's authored edges and gradient, extending only the fill.
        Texture pet_skin(nl::node source, const std::string& key, int width, int height,
                         int left, int right, int top, int bottom, int fill_x, int fill_y) {
            nl::bitmap bitmap=source;
            if (!bitmap || !bitmap.data()) return {};
            const auto* pixels=static_cast<const uint8_t*>(bitmap.data());
            std::vector<uint8_t> output(width*height*4);
            for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
                int sx=x<left ? x : x>=width-right ? bitmap.width()-(width-x) : fill_x;
                int sy=y<top ? y : y>=height-bottom ? bitmap.height()-(height-y)
                    : fill_y+(y-top)*(bitmap.height()-bottom-fill_y)/(height-top-bottom);
                for (int c=0;c<4;++c) output[(y*width+x)*4+c]=pixels[(sy*bitmap.width()+sx)*4+c];
            }
            return Texture::from_pixels("pet/skin/"+key,width,height,std::move(output),{});
        }
    }

	UIEquipInventory::UIEquipInventory(const Inventory& invent) : UIDragElement<PosEQINV>(Point<int16_t>(184, 20)), inventory(invent), tab(Buttons::BT_TAB1), hasPendantSlot(false), hasPocketSlot(false)
	{
		// UIWindow2 doll cells: cols x=10,43,76,109,142; rows y=27..225 (step 33)
		iconpositions[EquipSlot::Id::BOOK]     = Point<int16_t>(10, 27);
		iconpositions[EquipSlot::Id::HAT]      = Point<int16_t>(43, 27);
		iconpositions[EquipSlot::Id::BADGE]    = Point<int16_t>(76, 27);
		iconpositions[EquipSlot::Id::ANDROID]  = Point<int16_t>(109, 27);
		iconpositions[EquipSlot::Id::HEART]    = Point<int16_t>(142, 27);

		iconpositions[EquipSlot::Id::MEDAL]    = Point<int16_t>(10, 60);
		iconpositions[EquipSlot::Id::FACE]     = Point<int16_t>(43, 60);
		iconpositions[EquipSlot::Id::RING1]    = Point<int16_t>(109, 60);
		iconpositions[EquipSlot::Id::RING2]    = Point<int16_t>(142, 60);

		iconpositions[EquipSlot::Id::EYEACC]   = Point<int16_t>(43, 93);
		iconpositions[EquipSlot::Id::EARACC]   = Point<int16_t>(109, 93);
		iconpositions[EquipSlot::Id::SHOULDER] = Point<int16_t>(142, 93);

		iconpositions[EquipSlot::Id::CAPE]     = Point<int16_t>(10, 126);
		iconpositions[EquipSlot::Id::TOP]      = Point<int16_t>(43, 126);
		iconpositions[EquipSlot::Id::PENDANT1] = Point<int16_t>(76, 126);
		iconpositions[EquipSlot::Id::WEAPON]   = Point<int16_t>(109, 126);
		iconpositions[EquipSlot::Id::SHIELD]   = Point<int16_t>(142, 126);

		iconpositions[EquipSlot::Id::GLOVES]   = Point<int16_t>(10, 159);
		iconpositions[EquipSlot::Id::BOTTOM]   = Point<int16_t>(43, 159);
		iconpositions[EquipSlot::Id::BELT]     = Point<int16_t>(76, 159);
		iconpositions[EquipSlot::Id::RING3]    = Point<int16_t>(109, 159);
		iconpositions[EquipSlot::Id::RING4]    = Point<int16_t>(142, 159);

		iconpositions[EquipSlot::Id::SHOES]    = Point<int16_t>(76, 192);

		iconpositions[EquipSlot::Id::TAMEDMOB] = Point<int16_t>(10, 225);
		iconpositions[EquipSlot::Id::SADDLE]   = Point<int16_t>(43, 225);

		tab_source[Buttons::BT_TAB0] = "Equip";
		tab_source[Buttons::BT_TAB1] = "Cash";
		tab_source[Buttons::BT_TAB2] = "Pet";
		tab_source[Buttons::BT_TAB3] = "Android";

		nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
		nl::node character = nl::nx::ui["UIWindow2.img"]["Equip"]["character"];

		nl::node backgrnd = character["backgrnd"];
		Point<int16_t> bg_dimensions = Texture(backgrnd).get_dimensions();

		translated_frame = LocalizedArt(backgrnd, {{23, 5, 137, 14, localization::Key::EquipmentInventory, Color::Name::YELLOW, 20}});
		sprites.emplace_back(backgrnd);
		sprites.emplace_back(character["backgrnd2"]);
		sprites.emplace_back(character["backgrnd3"]);
		translated_slots = LocalizedArt(character["backgrnd3"], {
			{0, 12, 48, 14, localization::Key::SlotBook, Color::Name::DARKGREY, 0},
			{33, 0, 31, 10, localization::Key::SlotHat, Color::Name::DARKGREY, 33},
			{66, 12, 37, 10, localization::Key::SlotBadge, Color::Name::DARKGREY, 66},
			{99, 0, 49, 10, localization::Key::SlotAndroid, Color::Name::DARKGREY, 99},
			{132, 12, 31, 10, localization::Key::SlotHeart, Color::Name::DARKGREY, 132},
			{0, 33, 31, 10, localization::Key::SlotMedal, Color::Name::DARKGREY, 0},
			{33, 33, 32, 10, localization::Key::SlotFace, Color::Name::DARKGREY, 33},
			{99, 33, 31, 10, localization::Key::SlotRing, Color::Name::DARKGREY, 99},
			{132, 33, 31, 10, localization::Key::SlotRing, Color::Name::DARKGREY, 132},
			{33, 66, 31, 17, localization::Key::SlotEye, Color::Name::DARKGREY, 33},
			{99, 66, 31, 17, localization::Key::SlotEar, Color::Name::DARKGREY, 99},
			{117, 84, 46, 12, localization::Key::SlotShoulder, Color::Name::DARKGREY, 117},
			{0, 99, 38, 10, localization::Key::SlotCape, Color::Name::DARKGREY, 0},
			{33, 111, 53, 12, localization::Key::SlotTop, Color::Name::DARKGREY, 33},
			{66, 99, 51, 10, localization::Key::SlotPendant, Color::Name::DARKGREY, 66},
			{99, 111, 49, 12, localization::Key::SlotWeapon, Color::Name::DARKGREY, 99},
			{132, 99, 31, 10, localization::Key::SlotShield, Color::Name::DARKGREY, 132},
			{0, 132, 50, 10, localization::Key::SlotGloves, Color::Name::DARKGREY, 0},
			{33, 156, 35, 10, localization::Key::SlotBottom, Color::Name::DARKGREY, 33},
			{66, 132, 31, 10, localization::Key::SlotBelt, Color::Name::DARKGREY, 66},
			{99, 132, 31, 10, localization::Key::SlotRing, Color::Name::DARKGREY, 99},
			{132, 132, 31, 10, localization::Key::SlotRing, Color::Name::DARKGREY, 132},
			{66, 165, 34, 10, localization::Key::SlotShoes, Color::Name::DARKGREY, 66},
			{0, 198, 40, 16, localization::Key::SlotMount, Color::Name::DARKGREY, 0},
			{33, 219, 50, 11, localization::Key::SlotSaddle, Color::Name::DARKGREY, 33},
			{66, 198, 31, 16, localization::Key::SlotMountEquip, Color::Name::DARKGREY, 66},
		});

		totem_dimensions = Point<int16_t>(0, 0);
		totem_adj = Point<int16_t>(0, 0);

		disabled = character["disabled"];

		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close, Point<int16_t>(bg_dimensions.x() - 19, 5));

		tab = Buttons::BT_TAB0;

		dimension = bg_dimensions;
		dragarea = Point<int16_t>(bg_dimensions.x(), 20);

        auto basic=nl::nx::ui["Basic.img"];
        auto interior=nl::nx::ui["UIWindow2.img"]["UserList"]["Group"]["backgrnd2"];
        pet_panel=pet_skin(interior,"panel",dimension.x()-16,dimension.y()-58,6,6,8,6,100,100);
        pet_card=pet_skin(interior,"card",dimension.x()-28,72,6,6,8,6,100,100);
        pet_slot=pet_skin(interior,"slot",36,36,6,6,8,6,100,100);
        for (int i=0;i<2;++i) pet_checks[i]=Texture(basic["CheckBox3"][i]);
        const char* states[]={"normal","mouseOver","disabled"};
        for (int i=0;i<3;++i)
            pet_adjust[i]=pet_skin(basic["BtOK"][states[i]]["0"],std::string("adjust/")+states[i],24,18,7,7,3,3,7,3);
        for (int i=0;i<2;++i)
            pet_tabs[i]=pet_skin(basic["BtOK"][i ? "normal" : "disabled"]["0"],std::string("tab/")+std::to_string(i),(dimension.x()-20)/2,20,7,7,3,3,7,3);

		load_icons();
	}

	void UIEquipInventory::draw(float alpha) const
	{
		if (Setting<UILanguage>::get().load() == "zh-CN")
		{
			// LocalizedArt takes opacity; alpha here is frame interpolation.
			translated_frame.draw(position);
			// The frame must precede the paper-doll backing and captions.
			sprites[1].draw(position, alpha);
			translated_slots.draw(position);
			draw_buttons(alpha);
		}
		else UIElement::draw(alpha);

		for (auto iter : icons)
		{
			if (tab == Buttons::BT_TAB2) break;
			// Unmapped slots (emblem/pocket/etc — not on this doll) default
			// to (0,0); skip them so no icon stacks in the corner.
			Point<int16_t> ipos = iconpositions[iter.first];

			if (iter.second && (ipos.x() != 0 || ipos.y() != 0))
				iter.second->draw(position + ipos + Point<int16_t>(1, 1));
		}

        const bool cn = Setting<UILanguage>::get().load() == "zh-CN";
        auto label = [&](const std::string& text, int x, int y, Color::Name color=Color::Name::PRUSSIANBLUE, int width=0) {
            Text(Text::A11M, Text::LEFT, color, text, width, false).draw(position+Point<int16_t>(x,y));
        };
        if (tab == Buttons::BT_TAB2) {
            pet_panel.draw(position+Point<int16_t>(8,26));
            label(cn ? "自动拾取" : "Auto pickup",16,31);
            for (int i=0;i<2;++i) {
                int x=i ? 96 : 16;
                bool checked=i ? Setting<PetLootItems>::get().load() : Setting<PetLootMesos>::get().load();
                const auto& check=pet_checks[checked ? 1 : 0];
                check.draw(position+Point<int16_t>(x,52)+check.get_origin());
                label(i ? (cn ? "物品" : "Items") : (cn ? "金币" : "Mesos"),x+19,51);
            }
            for (int slot=0;slot<2;++slot) {
                int y=92+slot*72;
                pet_card.draw(position+Point<int16_t>(14,y-18));
                label(slot ? (cn ? "MP 自动恢复" : "MP recovery") : (cn ? "HP 自动恢复" : "HP recovery"),20,y-17,
                    slot ? Color::Name::MEDIUMBLUE : Color::Name::DARKRED);
                pet_slot.draw(position+Point<int16_t>(16,y-1));
                int id=slot ? Configuration::get().get_auto_mp_pot() : Configuration::get().get_auto_hp_pot();
                if (id) {
                    const auto& icon=ItemData::get(id).get_icon(false);
                    icon.draw(position+Point<int16_t>(18,y+1)+icon.get_origin());
                }
                label(id ? ItemData::get(id).get_name() : (cn ? "拖入药水" : "Drop potion"),57,y+4,Color::Name::PRUSSIANBLUE,104);
                if (!id) label(cn ? "消耗背包 → 此处" : "From USE tab",57,y+18,Color::Name::DUSTYGRAY);
                int threshold=pet_potion_threshold(slot ? Setting<MPWarning>::get().load() : Setting<HPWarning>::get().load());
                label(threshold ? (cn ? "不高于 " : "At most ")+std::to_string(threshold)+"%" : (cn ? "已关闭" : "Off"),20,y+31);
                for (int sign=0;sign<2;++sign) {
                    int x=sign ? 142 : 110;
                    bool disabled=sign ? threshold==95 : threshold==0;
                    bool hover=Rectangle<int16_t>(x,x+24,y+36,y+54).contains(pet_hover);
                    pet_adjust[disabled ? 2 : hover ? 1 : 0].draw(position+Point<int16_t>(x,y+36));
                    Text(Text::A11M,Text::CENTER,Color::Name::WHITE,sign ? "+" : "−")
                        .draw(position+Point<int16_t>(x+12,y+31));
                }
            }
            for (int i=0;i<2;++i) {
                int id=1812000+i;
                int equipped=inventory.find_item(InventoryType::Id::EQUIPPED,id);
                int owned=inventory.find_item(InventoryType::Id::EQUIP,id);
                int x=i ? 96 : 16;
                label(i ? (cn ? "道具袋" : "Pouch") : (cn ? "磁铁" : "Magnet"),x,214);
                pet_tabs[equipped ? 0 : 1].draw(DrawArgument(position+Point<int16_t>(x,235),0.85f,0.85f));
                Text(Text::A11M,Text::CENTER,equipped ? Color::Name::DARKGREY : Color::Name::WHITE,
                    equipped ? (cn ? "卸下" : "Remove") : owned ? (cn ? "穿戴" : "Equip") : (cn ? "商城获取" : "Cash shop"))
                    .draw(position+Point<int16_t>(x+34,230));
            }

        }
        for (int t=0;t<2;++t) {
            bool selected=(t==1)==(tab==Buttons::BT_TAB2);
            int x=10+t*(dimension.x()-20)/2;
            pet_tabs[selected ? 1 : 0].draw(position+Point<int16_t>(x,dimension.y()-26));
            Text(Text::A11M,Text::CENTER,selected ? Color::Name::WHITE : Color::Name::DARKGREY,
                t ? (cn ? "宠物" : "Pet") : (cn ? "装备" : "Equipment"))
                .draw(position+Point<int16_t>(x+(dimension.x()-20)/4,dimension.y()-30));
        }

	}

	Button::State UIEquipInventory::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::BT_CLOSE:
			toggle_active();
			break;
		case Buttons::BT_TAB0:
		case Buttons::BT_TAB1:
		case Buttons::BT_TAB2:
		case Buttons::BT_TAB3:
			change_tab(id);

			return Button::State::IDENTITY;
		default:
			break;
		}

		return Button::State::NORMAL;
	}

	void UIEquipInventory::update_slot(EquipSlot::Id slot)
	{
		// Forget a dangling drag reference before replacing/destroying the
		// icon (also lets the empty-slot branch actually free it — release()
		// leaked the icon to dodge exactly this dangling problem)
		if (icons[slot])
			UI::get().purge_icon(icons[slot].get());

		if (int32_t item_id = inventory.get_item_id(InventoryType::Id::EQUIPPED, slot))
		{
			const Texture& texture = ItemData::get(item_id).get_icon(false);

			icons[slot] = std::make_unique<Icon>(
				std::make_unique<EquipIcon>(slot),
				texture,
				-1
				);
		}
		else if (icons[slot])
		{
			icons[slot].reset();
		}

		clear_tooltip();
	}

	void UIEquipInventory::load_icons()
	{
		for (auto iter : EquipSlot::values)
			if (icons[iter])
				UI::get().purge_icon(icons[iter].get());

		icons.clear();

		for (auto iter : EquipSlot::values)
			update_slot(iter);
	}

	Cursor::State UIEquipInventory::send_cursor(bool pressed, Point<int16_t> cursorpos)
	{
		auto local = cursorpos - position;
        pet_hover=local;
        if (tab == Buttons::BT_TAB2) {
            if (local.y()>=235 && local.y()<252 && local.x()>=16 && local.x()<166) {
                if (pressed) use_pet_accessory(local.x()>=96 ? 1 : 0);
                return Cursor::State::CANGRAB;
            }
            if (local.y() >= 49 && local.y() < 66 && local.x() >= 12 && local.x() < 174) {
                if (pressed) {
                    if (local.x() < 92) Setting<PetLootMesos>::get().save(!Setting<PetLootMesos>::get().load());
                    else Setting<PetLootItems>::get().save(!Setting<PetLootItems>::get().load());
                    Configuration::get().save();
                }
                return Cursor::State::CANGRAB;
            }
            for (int i=0;i<2;++i) {
                int y=130+i*72;
                if (local.y()>=y && local.y()<y+16 && local.x()>=108 && local.x()<170) {
                    if (pressed) {
                        int percentage=pet_potion_threshold(i ? Setting<MPWarning>::get().load() : Setting<HPWarning>::get().load());
                        percentage=pet_potion_threshold(percentage+(local.x()<138 ? -5 : 5));
                        if (i) Setting<MPWarning>::get().save(percentage);
                        else Setting<HPWarning>::get().save(percentage);
                        Configuration::get().save();
                    }
                    return Cursor::State::CANGRAB;
                }
            }
        }
        if (local.y() >= dimension.y()-26 && local.y() < dimension.y()-6 && local.x() >= 10 && local.x() < dimension.x()-10) {
            if (pressed) { clear_tooltip(); change_tab(local.x() < dimension.x()/2 ? Buttons::BT_TAB0 : Buttons::BT_TAB2); }
            return Cursor::State::CANGRAB;
        }
        Cursor::State dstate = UIDragElement::send_cursor(pressed, cursorpos);
        if (tab == Buttons::BT_TAB2) { clear_tooltip(); return dstate; }

		if (dragged)
		{
			clear_tooltip();

			return dstate;
		}

		EquipSlot::Id slot = slot_by_position(cursorpos);

		if (auto icon = icons[slot].get())
		{
			if (pressed)
			{
				icon->start_drag(cursorpos - position - iconpositions[slot]);

				UI::get().drag_icon(icon);

				clear_tooltip();

				return Cursor::State::GRABBING;
			}
			else
			{
				show_equip(slot);

				return Cursor::State::CANGRAB;
			}
		}
		else
		{
			clear_tooltip();

			return Cursor::State::IDLE;
		}
	}

	void UIEquipInventory::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				toggle_active();
			}
		}
	}

	UIElement::Type UIEquipInventory::get_type() const
	{
		return TYPE;
	}

	void UIEquipInventory::doubleclick(Point<int16_t> cursorpos)
	{
        if (tab == Buttons::BT_TAB2) {
            int slot = potion_slot(cursorpos);
            if (slot >= 0) set_potion(slot == 1, 0);
            return;
        }
		EquipSlot::Id slot = slot_by_position(cursorpos);

		if (icons[slot])
			if (int16_t freeslot = inventory.find_free_slot(InventoryType::Id::EQUIP))
				UnequipItemPacket(slot, freeslot).dispatch();
	}

	bool UIEquipInventory::is_in_range(Point<int16_t> cursorpos) const
	{
		Rectangle<int16_t> bounds = Rectangle<int16_t>(position, position + dimension);

		Rectangle<int16_t> totem_bounds = Rectangle<int16_t>(position, position + totem_dimensions);
		totem_bounds.shift(totem_adj);

		return bounds.contains(cursorpos) || totem_bounds.contains(cursorpos);
	}

	bool UIEquipInventory::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		if (tab == Buttons::BT_TAB2) {
            int slot = potion_slot(cursorpos);
            int id = icon.get_action_id();
            if (slot < 0 || icon.get_source_tab() != InventoryType::Id::USE ||
                !inventory.find_item(InventoryType::Id::USE, id)) return true;
            std::string key = "0" + std::to_string(id);
            auto spec = nl::nx::item["Consume"][key.substr(0,4)+".img"][key]["spec"];
            const char* flat = slot ? "mp" : "hp";
            const char* ratio = slot ? "mpR" : "hpR";
            if (spec[flat].get_integer() > 0 || spec[ratio].get_integer() > 0)
                set_potion(slot == 1, id);
        } else if (EquipSlot::Id slot = slot_by_position(cursorpos))
            icon.drop_on_equips(slot);

		return true;
	}

    void UIEquipInventory::use_pet_accessory(int index) {
        const int id=1812000+index;
        if (int slot=inventory.find_item(InventoryType::Id::EQUIPPED,id)) {
            if (int free=inventory.find_free_slot(InventoryType::Id::EQUIP))
                UnequipItemPacket(slot,free).dispatch();
        } else if (int slot=inventory.find_item(InventoryType::Id::EQUIP,id)) {
            // Dedicated v83 pet accessory positions; never replace character cash gear.
            const int destination=181+index;
            if (!inventory.get_item_id(InventoryType::Id::EQUIPPED,destination))
                MoveItemPacket(InventoryType::Id::EQUIP,slot,-destination,1).dispatch();
        } else {
            EnterCashShopPacket().dispatch();
        }
    }

    int UIEquipInventory::potion_slot(Point<int16_t> cursorpos) const {
        auto p = cursorpos-position;
        for (int i=0;i<2;++i)
            if (Rectangle<int16_t>(16,50,92+i*72,126+i*72).contains(p)) return i;
        return -1;
    }

    void UIEquipInventory::set_potion(bool mp, int32_t itemid) {
        PetAutoPotionPacket(mp,itemid).dispatch();
        if (mp) Configuration::get().set_auto_mp_pot(itemid);
        else Configuration::get().set_auto_hp_pot(itemid);
    }

	void UIEquipInventory::toggle_active()
	{
		clear_tooltip();

		UIElement::toggle_active();
	}

	void UIEquipInventory::modify(int16_t pos, int8_t mode, int16_t arg)
	{
		EquipSlot::Id eqpos = EquipSlot::by_id(pos);
		EquipSlot::Id eqarg = EquipSlot::by_id(arg);

		switch (mode)
		{
		case 0:
		case 3:
			update_slot(eqpos);
			break;
		case 2:
			update_slot(eqpos);
			update_slot(eqarg);
			break;
		}
	}

	void UIEquipInventory::show_equip(EquipSlot::Id slot)
	{
		UI::get().show_equip(Tooltip::Parent::EQUIPINVENTORY, slot);
	}

	void UIEquipInventory::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::Parent::EQUIPINVENTORY);
	}

	EquipSlot::Id UIEquipInventory::slot_by_position(Point<int16_t> cursorpos) const
	{
		if (tab != Buttons::BT_TAB0)
			return EquipSlot::Id::NONE;

		for (auto iter : iconpositions)
		{
			Rectangle<int16_t> iconrect = Rectangle<int16_t>(
				position + iter.second,
				position + iter.second + Point<int16_t>(32, 32)
				);

			if (iconrect.contains(cursorpos))
				return iter.first;
		}

		return EquipSlot::Id::NONE;
	}

	void UIEquipInventory::change_tab(uint16_t tabid)
	{
		// Single-page v83 window: tab buttons are never created, so only
		// remember the id (guards the old multi-tab code's null derefs).
		tab = tabid;
	}

	UIEquipInventory::EquipIcon::EquipIcon(int16_t s)
	{
		source = s;
	}

	void UIEquipInventory::EquipIcon::drop_on_stage() const
	{
		Sound(Sound::Name::DRAGEND).play();
	}

	void UIEquipInventory::EquipIcon::drop_on_equips(EquipSlot::Id slot) const
	{
		if (source == slot)
			Sound(Sound::Name::DRAGEND).play();
	}

	bool UIEquipInventory::EquipIcon::drop_on_items(InventoryType::Id tab, EquipSlot::Id eqslot, int16_t slot, bool equip) const
	{
		if (tab != InventoryType::Id::EQUIP)
		{
			if (auto iteminventory = UI::get().get_element<UIItemInventory>())
			{
				if (iteminventory->is_active())
				{
					iteminventory->change_tab(InventoryType::Id::EQUIP);
					return false;
				}
			}
		}

		if (equip)
		{
			if (eqslot == source)
				EquipItemPacket(slot, eqslot).dispatch();
		}
		else
		{
			UnequipItemPacket(source, slot).dispatch();
		}

		return true;
	}

	Icon::IconType UIEquipInventory::EquipIcon::get_type()
	{
		return Icon::IconType::EQUIP;
	}
}
