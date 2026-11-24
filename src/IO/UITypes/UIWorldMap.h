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

#include "../UIDragElement.h"
#include "../Components/LocalizedArt.h"

#include "../Components/AreaButton.h"
#include "../Components/Textfield.h"
#include <chrono>

namespace ms
{
	class UIWorldMap : public UIDragElement<PosMAP>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::WORLDMAP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIWorldMap();

		void draw(float inter) const override;
		void update() override;

		void toggle_active() override;

		void remove_cursor() override;
		void doubleclick(Point<int16_t> cursorpos) override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursor_pos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		static constexpr uint8_t MAPSPOT_TYPE_MAX = 4u;

		void set_search(bool enable);
		void choose_destination(std::vector<int32_t> ids, size_t page);
		void request_travel(int32_t id);
		void update_world(std::string parent_map);

		enum Buttons
		{
			BT_CLOSE,
			BT_SEARCH,
			BT_AUTOFLY,
			BT_NAVIREG,
			BT_ALLSEARCH,
			BT_SEARCH_CLOSE,
			BT_LINK0,
			BT_LINK1,
			BT_LINK2,
			BT_LINK3,
			BT_LINK4,
			BT_LINK5,
			BT_LINK6,
			BT_LINK7,
			BT_LINK8,
			BT_LINK9
		};

		struct MapSpot
		{
			std::string description;
			Texture path;
			std::string title;
			uint8_t type;
			Texture marker;
			bool bolded;
			std::vector<int32_t> map_ids;
		};

		std::vector<int32_t> pending_destinations;
		int32_t pending_confirmation = -1;
		size_t pending_page = 0;
		bool search = false;
		bool show_path_img = false;
		bool region_click_consumed = false;
		std::chrono::steady_clock::time_point next_travel{};

		int32_t mapid = -1;

		std::string display_language;
		std::string parent_map;
		std::string user_map;

		LocalizedArt translated_border, translated_search, translated_search_notice;
		Texture search_background;
		Texture search_notice;
		Texture base_img;
		Texture path_img;

		Animation cur_pos;
		Animation npc_pos[MAPSPOT_TYPE_MAX];

		Textfield search_text;

		std::map<uint16_t, Texture> link_images;
		std::map<uint16_t, std::string> link_maps;

		std::vector<std::pair<Point<int16_t>, MapSpot>> map_spots;

		Point<int16_t> bg_dimensions;
		Point<int16_t> bg_search_dimensions;
		Point<int16_t> background_dimensions;
		Point<int16_t> base_position;
	};
}
