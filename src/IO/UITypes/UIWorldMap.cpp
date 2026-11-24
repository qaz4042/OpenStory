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
#include "UIWorldMap.h"

#include "../UI.h"
#include "../Components/LocalizedMapleButton.h"
#include "UINotice.h"
#include "../../Net/Packets/MessagingPackets.h"

#include "../../Gameplay/Stage.h"

#include "../../IO/Components/MapleButton.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIWorldMap::UIWorldMap() : UIDragElement<PosMAP>(Point<int16_t>(500, 20))
	{
		nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
		nl::node WorldMap = nl::nx::ui["UIWindow2.img"]["WorldMap"];
		nl::node WorldMapSearch = WorldMap["WorldMapSearch"];
		nl::node Border = WorldMap["Border"]["0"];
		nl::node backgrnd = WorldMapSearch["backgrnd"];
		nl::node MapHelper = nl::nx::map["MapHelper.img"]["worldMap"];

		cur_pos = MapHelper["curPos"];

		for (size_t i = 0; i < MAPSPOT_TYPE_MAX; i++)
			npc_pos[i] = MapHelper["npcPos" + std::to_string(i)];

		sprites.emplace_back(Border);
		translated_border = LocalizedArt(Border, {{250, 3, 150, 12, localization::Key::WorldMapTitle, Color::Name::YELLOW, 200}});
		translated_search = LocalizedArt(backgrnd, {{35, 3, 75, 12, localization::Key::SearchLabel, Color::Name::YELLOW, 20}});
		translated_search_notice = LocalizedArt(WorldMapSearch["notice"], {{0, 0, 88, 59, localization::Key::MapSearchHint, Color::Name::DARKGREY}});

		search_background = backgrnd;
		search_notice = WorldMapSearch["notice"];

		bg_dimensions = Texture(Border).get_dimensions();
		bg_search_dimensions = search_background.get_dimensions();

		int16_t bg_dimension_x = bg_dimensions.x();
		background_dimensions = Point<int16_t>(bg_dimension_x, 0);

		int16_t base_x = bg_dimension_x / 2;
		int16_t base_y = bg_dimensions.y() / 2;
		base_position = Point<int16_t>(base_x, base_y + 15);

		Point<int16_t> close_dimensions = Point<int16_t>(bg_dimension_x - 22, 4);

		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(close, close_dimensions);
		buttons[Buttons::BT_SEARCH] = std::make_unique<LocalizedMapleButton>(WorldMap["BtSearch"], localization::Key::OpenMapSearch);
		buttons[Buttons::BT_AUTOFLY] = std::make_unique<LocalizedMapleButton>(WorldMap["BtAutoFly_1"], localization::Key::AutoPilotReady, Point<int16_t>(), 20, 4);
		buttons[Buttons::BT_NAVIREG] = std::make_unique<LocalizedMapleButton>(WorldMap["BtNaviRegister"], localization::Key::EnableNavigation, Point<int16_t>(), 20, 4);
		buttons[Buttons::BT_SEARCH_CLOSE] = std::make_unique<MapleButton>(close, close_dimensions + Point<int16_t>(bg_search_dimensions.x(), 0));
		buttons[Buttons::BT_ALLSEARCH] = std::make_unique<LocalizedMapleButton>(WorldMapSearch["BtAllsearch"], localization::Key::SearchLabel, background_dimensions);

		Point<int16_t> search_text_pos = Point<int16_t>(bg_dimension_x + 14, 25);
		Point<int16_t> search_box_dim = Point<int16_t>(83, 15);
		Rectangle<int16_t> search_text_dim = Rectangle<int16_t>(search_text_pos, search_text_pos + search_box_dim);

		search_text = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, search_text_dim, 8);

		set_search(true);

		dragarea = Point<int16_t>(bg_dimension_x, 20);
	}

	void UIWorldMap::draw(float alpha) const
	{
		// alpha is animation interpolation, not window opacity.
		if (Setting<UILanguage>::get().load() == "zh-CN") translated_border.draw(position);
		else UIElement::draw_sprites(alpha);

		if (search)
		{
			if (Setting<UILanguage>::get().load() == "zh-CN")
			{
				translated_search.draw(position + background_dimensions);
				translated_search_notice.draw(position + background_dimensions);
			}
			else
			{
				search_background.draw(position + background_dimensions);
				search_notice.draw(position + background_dimensions);
			}
			search_text.draw(position + Point<int16_t>(1, -5));
		}

		base_img.draw(position + base_position);

		if (link_images.size() > 0)
		{
			for (auto& iter : buttons)
			{
				if (const auto button = iter.second.get())
				{
					if (iter.first >= Buttons::BT_LINK0 && button->get_state() == Button::State::MOUSEOVER)
					{
						if (link_images.find(iter.first) != link_images.end())
						{
							link_images.at(iter.first).draw(position + base_position);
							break;
						}
					}
				}
			}
		}

		if (show_path_img)
			path_img.draw(position + base_position);

		for (auto spot : map_spots)
			spot.second.marker.draw(spot.first + position + base_position);

		bool found = false;

		if (!found)
		{
			for (auto spot : map_spots)
			{
				for (auto map_id : spot.second.map_ids)
				{
					if (map_id == mapid)
					{
						found = true;
						if (spot.second.type < MAPSPOT_TYPE_MAX)
							npc_pos[spot.second.type].draw(spot.first + position + base_position, alpha);
						cur_pos.draw(spot.first + position + base_position, alpha);
						break;
					}
				}

				if (found)
					break;
			}
		}

		UIElement::draw_buttons(alpha);
	}

	void UIWorldMap::update()
	{
		// Defer replacement until the destination-number callback has returned.
		if (pending_confirmation >= 0)
		{
			const auto id = pending_confirmation;
			pending_confirmation = -1;
			const auto source = Stage::get().get_mapid();
			const auto name = NxHelper::Map::get_map_info_by_id(id).full_name;
			const bool chinese = Setting<UILanguage>::get().load() == "zh-CN";
			UI::get().emplace<UIYesNo>(chinese ? "确定免费传送至：\n" + name + "？"
				: "Travel for free to:\n" + name + "?", [this, id, source](bool yes)
			{
				const auto now = std::chrono::steady_clock::now();
				if (!yes || Stage::get().get_mapid() != source || id == source || now < next_travel) return;
				next_travel = now + std::chrono::seconds(3);
				GeneralChatPacket("@maptravel " + std::to_string(id), false).dispatch();
			});
		}
		// Replace the number notice after its callback returns.
		if (!pending_destinations.empty())
		{
			auto ids = std::move(pending_destinations);
			pending_destinations.clear();
			choose_destination(std::move(ids), pending_page);
		}
		const auto language = Setting<UILanguage>::get().load();
		if (display_language != language)
		{
			display_language = language;
			if (!user_map.empty()) update_world(user_map);
		}
		int32_t mid = Stage::get().get_mapid();

		if (mid != mapid)
		{
			mapid = mid;
			auto prefix = mapid / 10000000;
			auto parent_map = "WorldMap0" + std::to_string(prefix);
			user_map = parent_map;

			update_world(parent_map);
		}

		if (search)
			search_text.update(position);

		for (size_t i = 0; i < MAPSPOT_TYPE_MAX; i++)
			npc_pos[i].update(1);

		cur_pos.update();

		UIElement::update();
	}

	void UIWorldMap::toggle_active()
	{
		UIElement::toggle_active();

		if (!active)
		{
			set_search(true);
			update_world(user_map);
		}
	}

	void UIWorldMap::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			if (search)
			{
				set_search(false);
			}
			else
			{
				if (parent_map == "")
				{
					toggle_active();

					update_world(user_map);
				}
				else
				{
					Sound(Sound::Name::SELECTMAP).play();

					update_world(parent_map);
				}
			}
		}
	}

	UIElement::Type UIWorldMap::get_type() const
	{
		return TYPE;
	}

	Button::State UIWorldMap::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BT_CLOSE:
			deactivate();
			break;
		case Buttons::BT_SEARCH:
			set_search(!search);
			break;
		case Buttons::BT_SEARCH_CLOSE:
			set_search(false);
			break;
		default:
			break;
		}

		if (buttonid >= Buttons::BT_LINK0)
		{
			auto link = link_maps.find(buttonid);
			if (link != link_maps.end())
			{
				region_click_consumed = true;
				update_world(link->second);
			}

			return Button::State::IDENTITY;
		}

		return Button::State::NORMAL;
	}

	void UIWorldMap::remove_cursor()
	{
		if (!UI::get().is_mouse_held())
			region_click_consumed = false;
		UIDragElement::remove_cursor();

		UI::get().clear_tooltip(Tooltip::Parent::WORLDMAP);

		show_path_img = false;
	}

	Cursor::State UIWorldMap::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Region links rebuild the controls on mouse-down. Held cursor moves
		// must not activate a link in the newly displayed map.
		if (region_click_consumed)
		{
			if (!clicked && !UI::get().is_mouse_held())
				region_click_consumed = false;
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;
		}

		if (Cursor::State new_state = search_text.send_cursor(cursorpos, clicked))
			return new_state;

		show_path_img = false;

		for (auto path : map_spots)
		{
			Point<int16_t> p = path.first + position + base_position - 10;
			Point<int16_t> d = p + path.second.marker.get_dimensions();
			Rectangle<int16_t> abs_bounds = Rectangle<int16_t>(p, d);

			if (abs_bounds.contains(cursorpos))
			{
				path_img = path.second.path;
				show_path_img = path_img.is_valid();

				UI::get().show_map(Tooltip::Parent::WORLDMAP, path.second.title, path.second.description, path.second.map_ids.empty() ? 0 : path.second.map_ids[0], path.second.bolded);
				break;
			}
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIWorldMap::doubleclick(Point<int16_t> cursorpos)
	{
		if (region_click_consumed) return;
		for (const auto& spot : map_spots)
		{
			auto top = spot.first + position + base_position - 10;
			if (!Rectangle<int16_t>(top, top + spot.second.marker.get_dimensions()).contains(cursorpos)
				|| spot.second.map_ids.empty())
				continue;

			const auto ids = spot.second.map_ids;
			if (ids.size() == 1)
			{
				request_travel(ids.front());
				return;
			}

			choose_destination(ids, 0);
			return;
		}
	}

	void UIWorldMap::choose_destination(std::vector<int32_t> ids, size_t page)
	{
		if (ids.empty() || page > (ids.size() - 1) / 8) return;
		const size_t first = page * 8;
		const size_t last = std::min(first + 8, ids.size());
		std::string message = "选择目的地 / Destination (" + std::to_string(page + 1)
			+ "/" + std::to_string((ids.size() + 7) / 8) + "):\n";
		for (size_t i = first; i < last; ++i)
			message += std::to_string(i - first + 1) + ". "
				+ NxHelper::Map::get_map_info_by_id(ids[i]).full_name + "\n";
		if (last < ids.size()) message += "9. 下一页 / Next\n";
		if (page > 0) message += "10. 上一页 / Previous\n";
		UI::get().emplace<UIEnterNumber>(message, [this, ids, page, first, last](int32_t choice)
		{
			if (choice == 9 && last < ids.size()) { pending_destinations = ids; pending_page = page + 1; }
			else if (choice == 10 && page > 0) { pending_destinations = ids; pending_page = page - 1; }
			else if (choice > 0 && static_cast<size_t>(choice) <= last - first)
				request_travel(ids[first + choice - 1]);
		}, 10, 1);
	}

	void UIWorldMap::request_travel(int32_t id)
	{
		const auto now = std::chrono::steady_clock::now();
		if (id == Stage::get().get_mapid() || now < next_travel) return;
		pending_confirmation = id;
	}

	void UIWorldMap::set_search(bool enable)
	{
		search = enable;

		buttons[Buttons::BT_SEARCH_CLOSE]->set_active(enable);
		buttons[Buttons::BT_ALLSEARCH]->set_active(enable);

		if (enable)
		{
			search_text.set_state(Textfield::State::NORMAL);
			dimension = bg_dimensions + Point<int16_t>(bg_search_dimensions.x(), 0);
		}
		else
		{
			search_text.set_state(Textfield::State::DISABLED);
			dimension = bg_dimensions;
		}
	}

	void UIWorldMap::update_world(std::string map)
	{
		show_path_img = false;
		UI::get().clear_tooltip(Tooltip::Parent::WORLDMAP);
		nl::node WorldMap = nl::nx::map["WorldMap"][map + ".img"];

		if (!WorldMap)
			WorldMap = nl::nx::map["WorldMap"]["WorldMap.img"];

		base_img = WorldMap["BaseImg"][0];
		parent_map = (std::string)WorldMap["info"]["parentMap"];

		link_images.clear();
		link_maps.clear();

		for (auto& iter : buttons)
			if (const auto button = iter.second.get())
				if (iter.first >= Buttons::BT_LINK0)
					button->set_active(false);

		size_t i = Buttons::BT_LINK0;

		for (nl::node link : WorldMap["MapLink"])
		{
			nl::node l = link["link"];
			Texture link_image = l["linkImg"];

			link_images[i] = link_image;
			link_maps[i] = (std::string)l["linkMap"];

			buttons[i] = std::make_unique<AreaButton>(base_position - link_image.get_origin(), link_image.get_dimensions());
			buttons[i]->set_active(true);

			i++;
		}

		nl::node mapImage = nl::nx::map["MapHelper.img"]["worldMap"]["mapImage"];

		map_spots.clear();

		for (nl::node list : WorldMap["MapList"])
		{
			nl::node desc = list["desc"];
			nl::node mapNo = list["mapNo"];
			nl::node path = list["path"];
			nl::node spot = list["spot"];
			nl::node title = list["title"];
			nl::node type = list["type"];
			nl::node marker = mapImage[type];

			std::vector<int32_t> map_ids;

			for (nl::node map_no : mapNo)
				map_ids.push_back(map_no);

			if (!desc && !title)
			{
				NxHelper::Map::MapInfo map_info = NxHelper::Map::get_map_info_by_id(mapNo[0]);

				map_spots.emplace_back(std::make_pair<Point<int16_t>, MapSpot>(spot, { map_info.description, path, map_info.full_name, type, marker, true, map_ids }));
			}
			else
			{
				map_spots.emplace_back(std::make_pair<Point<int16_t>, MapSpot>(spot, { desc, path, title, type, marker, false, map_ids }));
			}
		}
	}
}
