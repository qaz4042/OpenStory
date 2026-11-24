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
#include "Stage.h"
#include "../Data/SkillData.h"

#include "MapleTVBroadcast.h"
#include "HiredMerchants.h"
#include "MiniRooms.h"

#include "../Configuration.h"

#include "../IO/UI.h"
#include "../IO/Window.h"
#include "FlightMotion.h"
#include "../IO/UITypes/UITrainingPanel.h"
#include "../IO/UITypes/UIChatBar.h"
#include "../Net/Packets/MessagingPackets.h"
#include "../Net/Session.h"
#include "../IO/UITypes/UISkillBook.h"

#include "../IO/UITypes/UIStatusBar.h"
#include "../Net/Packets/AttackAndSkillPackets.h"
#include "../Net/Packets/TradePackets.h"
#include "../Net/Packets/GameplayPackets.h"
#include "../Net/Packets/InventoryPackets.h"
#include "../Graphics/Geometry.h"
#include "../Character/MapleStat.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	Stage::Stage() : combat(player, chars, mobs, reactors, physics)
	{
		state = State::INACTIVE;
	}

	void Stage::init()
	{
		drops.init();
	}

	void Stage::load(int32_t mapid, int8_t portalid)
	{
		switch (state)
		{
			case State::INACTIVE:
				load_map(mapid);
				respawn(portalid);
				break;
			case State::TRANSITION:
				respawn(portalid);
				break;
		}

		state = State::ACTIVE;
        const int resume_flight = transfer_flight_mode;
        transfer_flight_mode = 0;
        if (resume_flight) set_flight_mode(resume_flight);
	}

	void Stage::loadplayer(const CharEntry& entry)
	{
		flight_mode = transfer_flight_mode = 0;
		player = entry;
		playable = player;

		start = ContinuousTimer::get().start();

		CharStats stats = player.get_stats();
		levelBefore = stats.get_stat(MapleStat::Id::LEVEL);
		expBefore = stats.get_exp();
	}

	void Stage::clear(bool map_transfer)
	{
        transfer_flight_mode = map_transfer ? (flight_mode ? flight_mode : transfer_flight_mode) : 0;
		set_flight_mode(0);
		held_attack_skill = 0;
		dash_tap_direction = -1;
		stop_training_auto("Map changed / disconnected", false);
		training_status.clear();
		player.set_training_invulnerable(false);
		MiniRooms::get().clear_all();
		HiredMerchants::get().clear_all();
		state = State::INACTIVE;

		chars.clear();
		npcs.clear();
		mobs.clear();
		summons.clear();
		dragons.clear();
		drops.clear();
		petai.reset();
		for (uint8_t i = 0; i < 3; ++i)
			player.get_pet(i).clear_loot_target();
		doors.clear();
		mists.clear();
		point_effects.clear();
		reactors.clear();

		// Weather is a property of the map we are leaving. The server only
		// ever sends BLOW_WEATHER for a map that has it, never a clear on the
		// way out, so without this the old map's snow/rain kept falling on
		// the new one for the rest of the session.
		weather.clear();
	}

	void Stage::load_map(int32_t mapid)
	{
		Stage::mapid = mapid;

		std::string strid = string_format::extend_id(mapid, 9);
		std::string prefix = std::to_string(mapid / 100000000);

		nl::node src = mapid == -1 ? nl::nx::ui["CashShopPreview.img"] : nl::nx::map["Map"]["Map" + prefix][strid + ".img"];

		tilesobjs = MapTilesObjs(src);
		backgrounds = MapBackgrounds(src["back"]);
		physics = Physics(src["foothold"]);
		mapinfo = MapInfo(src, physics.get_fht().get_walls(), physics.get_fht().get_borders());
		portals = MapPortals(src["portal"], mapid);
		environments = MapEnvironments(src["env"]);
	}

	void Stage::respawn(int8_t portalid)
	{
		Music(mapinfo.get_bgm()).play();

		Point<int16_t> spawnpoint = portals.get_portal_by_id(portalid);
		Point<int16_t> startpos = physics.get_y_below(spawnpoint);
		player.respawn(startpos, mapinfo.is_underwater());
		camera.set_position(startpos);
		camera.set_view(mapinfo.get_walls(), mapinfo.get_borders());
	}

	void Stage::draw(float alpha) const
	{
		if (state != State::ACTIVE)
			return;

		Point<int16_t> viewpos = camera.position(alpha);
		Point<double> viewrpos = camera.realposition(alpha);
		double viewx = viewrpos.x();
		double viewy = viewrpos.y();

		uint8_t gfx_quality = Setting<GraphicsQuality>::get().load();

		backgrounds.drawbackgrounds(viewx, viewy, alpha);

		for (auto id : Layer::IDs)
		{
			tilesobjs.draw(id, viewpos, alpha);

			reactors.draw(id, viewx, viewy, alpha);
			npcs.draw(id, viewx, viewy, alpha);
			npcs.draw_names(id, viewx, viewy, alpha);
			mobs.draw(id, viewx, viewy, alpha);
			summons.draw(id, viewx, viewy, alpha);
			dragons.draw(id, viewx, viewy, alpha);
			drops.draw(id, viewx, viewy, alpha);
			doors.draw(id, viewx, viewy, alpha);

			if (gfx_quality > 25)
				mists.draw(id, viewx, viewy, alpha);

			point_effects.draw(id, viewx, viewy, alpha);

			chars.draw(id, viewx, viewy, alpha);
			player.draw(id, viewx, viewy, alpha);
		}

		HiredMerchants::get().draw(viewpos, alpha);

		portals.draw(viewpos, alpha);

		combat.draw(viewx, viewy, alpha);
        if (flight_mode == 1) {
            Point<int16_t> at = Point<int16_t>(flight_position.x(), flight_position.y()) + viewpos;
            ColorBox(17, 3, Color::Name::YELLOW, 1).draw(at - Point<int16_t>(8,1));
            ColorBox(3, 17, Color::Name::YELLOW, 1).draw(at - Point<int16_t>(1,8));
        }

		if (gfx_quality > 10)
			backgrounds.drawforegrounds(viewx, viewy, alpha);

		if (gfx_quality > 25)
			environments.draw(viewx, viewy, alpha);

		effect.draw();

		if (gfx_quality > 50)
			weather.draw(alpha);

		// Low HP/MP warnings are drawn on their status-bar gauges by UIStatusBar.

	}

	bool Stage::training_auto_notice(const std::string& notice)
	{
        if (training_status.accept_rapid(notice, get_mapid())) {
            training_auto.accept(notice, get_mapid()); return true;
        }
		if (training_status.accept_pet_vacuum(notice, get_mapid())) return true;
		if (training_status.accept_vacuum_count(notice, get_mapid())) return true;
		const std::string pin_prefix = "[TrainingAssist:pin:v1] ";
		if (notice.compare(0, pin_prefix.size(), pin_prefix) == 0) {
			std::istringstream input(notice.substr(pin_prefix.size()));
			int32_t map, oid, enabled, x, y;
			std::string extra;
			if ((input >> map >> oid >> enabled >> x >> y) && !(input >> extra)
				&& map == get_mapid() && (enabled == 0 || enabled == 1)
				&& x >= -32768 && x <= 32767 && y >= -32768 && y <= 32767)
				mobs.set_training_pin(oid, enabled != 0, {static_cast<int16_t>(x), static_cast<int16_t>(y)});
			return true;
		}

		if (training_status.accept(notice, get_mapid())) {
			// The confirmed flag lasts until an off acknowledgement or map clear,
			// independent of the panel freshness timer when the panel is collapsed.
			player.set_training_invulnerable(training_status.known && training_status.allowed && training_status.invincible);
			// A status poll may stop a stale local session, but never starts one.
			if (training_status.fresh() && training_status.auto_skill < 0)
				stop_training_auto("Server stopped auto attack", false);
			return true;
		}
        if (notice.rfind("[TrainingAssist:rotation:v1] ",0)==0)
            return training_auto.accept(notice,get_mapid());
		const bool handled = training_auto.accept(notice, get_mapid());
		if (handled && training_auto.active()) training_status.stop_reason.clear();
		if (handled)
			if (auto chat = UI::get().get_element<UIChatBar>())
				chat->send_chatline(training_auto.active()
					? "Auto attack enabled (skill " + std::to_string(training_auto.skill()) + "). Use F8 to stop it."
					: "Auto attack disabled.", UIChatBar::LineType::YELLOW);
		return handled;
	}

	void Stage::stop_training_auto(const std::string& reason, bool notify_server)
	{
		if (!training_auto.active()) return;
		training_auto.stop();
		training_status.stop_reason = reason == "Disconnected / dead" ? "断线或死亡，已停止挂机"
			: reason == "No weapon equipped" ? "未装备武器，已停止挂机"
            : reason == "No ammunition" ? "弹药不足，请补充子弹或箭矢"
            : reason == "Not enough MP" ? "MP 不足，请补充蓝量"
            : reason == "Wrong weapon type" ? "武器类型不符，请更换武器"
			: reason == "Skill unavailable (MP, ammunition, weapon or skill requirements)" ? "技能条件不足，请检查蓝量、弹药和武器"
			: reason == "Server stopped auto attack" ? "服务端已停止挂机" : "挂机已停止";
		if (notify_server && Session::get().is_connected())
			GeneralChatPacket("@assist auto off", false).dispatch();
		if (auto chat = UI::get().get_element<UIChatBar>())
			chat->send_chatline("Auto attack stopped: " + reason, UIChatBar::LineType::YELLOW);
	}

	void Stage::update()
	{
		if (state != State::ACTIVE)
			return;

		combat.update();
		HiredMerchants::get().update();

		petai.update(player, drops);
		MapleTVBroadcast::get().tick();
		backgrounds.update();
		environments.update();
		effect.update();
		weather.update();
		tilesobjs.update();

		reactors.update(physics);
		npcs.update(physics);
		mobs.update(physics);
		summons.update(physics);
		dragons.update(physics);
		chars.update(physics);
		drops.update(physics);
		doors.update(physics);
		mists.update(physics);
		point_effects.update();
		update_flight();
		player.update(physics);

		if (training_auto.active())
		{
			if (!Session::get().is_connected() || player.get_stats().get_stat(MapleStat::Id::HP) == 0)
				stop_training_auto("Disconnected / dead");
			else if (!flight_mode && !UI::get().blocks_training_attack() && !player.is_climbing() && !player.is_sitting()
				&& training_auto.ready(TrainingAutoAttack::Clock::now()))
			{
				std::string reason;
				if (!combat.auto_use_move(training_auto.skill(), reason, training_auto.rapid_rate()>0))
                    {
                        if (training_auto.rapid_rate()>0 && reason.rfind("等待",0)!=0) {
                            stop_training_auto(reason); training_status.stop_reason="高频已停止："+reason;
                        } else { training_auto.retry(TrainingAutoAttack::Clock::now()); training_status.stop_reason="暂停："+reason; }
                    }
                else training_status.stop_reason.clear();
			}
		}

		if (held_attack_skill && !UI::get().is_skill_key_down(held_attack_skill))
			held_attack_skill = 0;
		if (held_attack_skill && !UI::get().blocks_training_attack()
			&& !player.is_climbing() && !player.is_sitting()
			&& player.get_stats().get_stat(MapleStat::Id::HP) > 0)
			combat.repeat_attack(held_attack_skill);

		portals.update(flight_mode == 1 ? Point<int16_t>(flight_position.x(), flight_position.y()) : player.get_position());
		camera.update(flight_mode == 1 ? Point<int16_t>(flight_position.x(), flight_position.y()) : player.get_position());

		// The local player is the audio listener; world sounds attenuate by
		// distance from here.
		Sound::set_listener_position(player.get_position());

		if (!flight_mode && !player.is_climbing() && !player.is_sitting() && !player.is_attacking())
		{
			if (player.is_key_down(KeyAction::Id::UP) && !player.is_key_down(KeyAction::Id::DOWN))
				check_ladders(true);

			if (player.is_key_down(KeyAction::Id::UP))
				check_portals();

			if (player.is_key_down(KeyAction::Id::DOWN))
				check_ladders(false);

			if (player.is_key_down(KeyAction::Id::SIT))
				check_seats();

			if (player.is_key_down(KeyAction::Id::ATTACK))
				combat.use_move(0);

			if (player.is_key_down(KeyAction::Id::PICKUP))
				check_drops();
		}

		// A server-approved hidden GM takes no touch damage or knockback.
		if (player.is_invincible() || player.is_hidden())
			return;

		if (int32_t oid_id = mobs.find_colliding(player.get_phobj()))
		{
			if (MobAttack attack = mobs.create_attack(oid_id))
			{
				MobAttackResult result = player.damage(attack);
				TakeDamagePacket(result, TakeDamagePacket::From::TOUCH).dispatch();
			}
		}
	}

    void Stage::set_flight_mode(int mode)
    {
        if (mode < 0 || mode > 2 || (mode && (state != ACTIVE || !player.get_stats().get_stat(MapleStat::Id::HP)))) return;
        if (mode == flight_mode) return;
        held_attack_skill = 0;
        dash_tap_direction = -1;
        // Clear ladder/chair/input state without changing the actual position.
        if (player.get_stats().get_stat(MapleStat::Id::HP) > 0)
            player.respawn(player.get_position(), mapinfo.is_underwater());
        player.set_assist_flight(false, false);
        flight_position = {double(player.get_position().x()), double(player.get_position().y())};
        flight_up = false;
        flight_mode = mode;
        camera.set_position(player.get_position());
    }

    void Stage::update_flight()
    {
        if (!flight_mode) return;
        if (!Session::get().is_connected() || !player.get_stats().get_stat(MapleStat::Id::HP))
        { set_flight_mode(0); return; }
        auto panel = UI::get().get_element<UITrainingPanel>();
        const bool paused = UI::get().blocks_training_attack() || (panel && panel->expanded());
        const auto actual = player.get_position();
        if (flight_mode == 2) flight_position = {player.get_phobj().crnt_x(), player.get_phobj().crnt_y()};
        const auto origin = flight_mode == 1
            ? Point<int16_t>(flight_position.x(), flight_position.y()) + camera.position()
            : actual + camera.position();
        auto next = flight_position;
        if (!paused && Window::get().flight_pointer_active())
        {
            auto pointer = UI::get().get_cursor_position();
            next = flight_next(flight_position, double(pointer.x())-origin.x(), double(pointer.y())-origin.y(),
                               mapinfo.get_walls(), mapinfo.get_borders(),
                               flight_speed(flight_mode, pointer, Constants::Constants::get().get_viewwidth(),
                                            Constants::Constants::get().get_viewheight()));
        }
        player.set_assist_flight(true, flight_mode == 2,
            {next.x()-flight_position.x(), next.y()-flight_position.y()});
        flight_position = next;
    }

	void Stage::show_character_effect(int32_t cid, CharEffect::Id effect)
	{
		if (auto character = get_character(cid))
			character->show_effect_id(effect);
	}

	void Stage::check_portals()
	{
		if (player.is_attacking())
			return;

		Point<int16_t> playerpos = flight_mode == 1 ? Point<int16_t>(flight_position.x(), flight_position.y()) : player.get_position();
		Portal::WarpInfo warpinfo = portals.find_warp_at(playerpos, flight_mode != 0);

        if (flight_mode == 1)
        {
            if (!warpinfo.name.empty())
                GeneralChatPacket("@flyportal " + std::to_string(mapid) + " " + warpinfo.name, false).dispatch();
            return;
        }
		if (warpinfo.intramap)
		{
			Point<int16_t> spawnpoint = portals.get_portal_by_name(warpinfo.toname);
			Point<int16_t> startpos = physics.get_y_below(spawnpoint);

			player.respawn(startpos, mapinfo.is_underwater());
		}
		else if (warpinfo.valid)
		{
			ChangeMapPacket(false, -1, warpinfo.name, false).dispatch();

			CharStats& stats = Stage::get().get_player().get_stats();

			stats.set_mapid(warpinfo.mapid);

			Sound(Sound::Name::PORTAL).play();
		}
	}

	void Stage::check_seats()
	{
		if (player.is_sitting() || player.is_attacking())
			return;

		Optional<const Seat> seat = mapinfo.findseat(player.get_position());
		player.set_seat(seat);
	}

	void Stage::check_ladders(bool up)
	{
		if (!player.can_climb() || player.is_climbing() || player.is_attacking())
			return;

		Optional<const Ladder> ladder = mapinfo.findladder(player.get_position(), up);
		player.set_ladder(ladder);
	}

	void Stage::check_drops()
	{
		Point<int16_t> playerpos = player.get_position();
		MapDrops::Loot loot = drops.find_loot_at(playerpos);

		if (loot.first)
			PickupItemPacket(loot.first, loot.second).dispatch();
	}

	void Stage::send_key(KeyType::Id type, int32_t action, bool down)
	{
		if (state != State::ACTIVE || !playable)
			return;

        if (flight_mode) {
            if (type == KeyType::Id::ACTION && down && KeyAction::actionbyid(action) == KeyAction::Id::INTERACT_HARVEST) {
                npcs.talk_nearest(flight_mode == 1 ? Point<int16_t>(flight_position.x(), flight_position.y()) : player.get_position());
                return;
            }
            if (type == KeyType::Id::ACTION && KeyAction::actionbyid(action) == KeyAction::Id::UP) {
                if (down && !flight_up) check_portals();
                flight_up = down;
            }
            return;
        }
		switch (type)
		{
			case KeyType::Id::ACTION:
				{
					const auto key = KeyAction::actionbyid(action);
					// Instant interaction: do not latch this key in Player. Its release
					// may be consumed by the dialog opened by the server response.
					if (key == KeyAction::Id::INTERACT_HARVEST)
					{
						if (down) npcs.talk_nearest(player.get_position());
						break;
					}
					const bool fresh = down && !player.is_key_down(key);
					playable->send_action(key, down);
					if (fresh && (key == KeyAction::Id::LEFT || key == KeyAction::Id::RIGHT))
					{
						const auto now = std::chrono::steady_clock::now();
						const bool twice = dash_tap_direction == action
							&& now - dash_tap_time <= std::chrono::milliseconds(300);
						dash_tap_direction = twice ? -1 : action;
						dash_tap_time = now;
						if (twice && !player.is_key_down(key == KeyAction::Id::LEFT
							? KeyAction::Id::RIGHT : KeyAction::Id::LEFT))
							for (int32_t skill : {5001005, 15001003})
								if (player.get_skilllevel(skill) > 0)
								{
									combat.use_move(skill);
									break;
								}
					}
				}
				break;
			case KeyType::Id::SKILL:
				if (!down)
				{
					if (held_attack_skill == action) held_attack_skill = 0;
				}
				else if (held_attack_skill != action)
				{
					// Attack continuation follows game frames, never OS key repeat.
					if (SkillData::get(action).is_attack()) held_attack_skill = action;
					combat.use_move(action);
				}
				break;
			case KeyType::Id::MACRO:
				if (down)
					if (auto sb = UI::get().get_element<UISkillBook>())
						sb->trigger_macro(action);
				break;
			case KeyType::Id::ITEM:
				if (down)
					player.use_item(action);
				break;
			case KeyType::Id::FACE:
				player.set_expression(action);
				break;
		}
	}

	Cursor::State Stage::send_cursor(bool pressed, Point<int16_t> position)
	{
		auto statusbar = UI::get().get_element<UIStatusBar>();

		if (statusbar && statusbar->is_menu_active())
		{
			if (pressed)
				statusbar->remove_menus();

			if (statusbar->is_in_range(position))
				return statusbar->send_cursor(pressed, position);
		}

		Point<int16_t> mappos = position - camera.position();

		if (const auto* m = HiredMerchants::get().find_at(mappos))
		{
			if (pressed)
			{
				MiniRoomVisitPacket(m->oid).dispatch();
				return Cursor::State::CLICKING;
			}

			return Cursor::State::CANCLICK;
		}

		// Check characters first, then NPCs
		Cursor::State char_state = chars.send_cursor(pressed, position, camera.position());
		if (char_state != Cursor::State::IDLE)
			return char_state;

		return npcs.send_cursor(pressed, position, camera.position());
	}

	bool Stage::is_player(int32_t cid) const
	{
		return cid == player.get_oid();
	}

	MapNpcs& Stage::get_npcs()
	{
		return npcs;
	}

	MapChars& Stage::get_chars()
	{
		return chars;
	}

	MapMobs& Stage::get_mobs()
	{
		return mobs;
	}

	MapReactors& Stage::get_reactors()
	{
		return reactors;
	}

	MapDrops& Stage::get_drops()
	{
		return drops;
	}

	MapDoors& Stage::get_doors()
	{
		return doors;
	}

	MapMists& Stage::get_mists()
	{
		return mists;
	}

	MapPointEffects& Stage::get_point_effects()
	{
		return point_effects;
	}

	MapSummons& Stage::get_summons()
	{
		return summons;
	}

	MapDragons& Stage::get_dragons()
	{
		return dragons;
	}

	Player& Stage::get_player()
	{
		return player;
	}

	Combat& Stage::get_combat()
	{
		return combat;
	}

	Optional<Char> Stage::get_character(int32_t cid)
	{
		if (is_player(cid))
			return player;
		else
			return chars.get_char(cid);
	}

	int Stage::get_mapid()
	{
		return mapid;
	}

	void Stage::add_effect(std::string path)
	{
		effect = MapEffect(path);
	}

	void Stage::set_weather(const std::string& path, const std::string& message)
	{
		nl::node src = nl::nx::map.resolve(path);

		weather.set(src, path, message);
	}

	void Stage::clear_weather()
	{
		weather.clear();
	}

	void Stage::toggle_environment(const std::string& name, int32_t mode)
	{
		environments.toggle(name, mode);
	}

	int64_t Stage::get_uptime()
	{
		return ContinuousTimer::get().stop(start);
	}

	uint16_t Stage::get_uplevel()
	{
		return levelBefore;
	}

	int64_t Stage::get_upexp()
	{
		return expBefore;
	}

	void Stage::transfer_player()
	{
		PlayerMapTransferPacket().dispatch();

		if (Configuration::get().get_admin())
			AdminEnterMapPacket(AdminEnterMapPacket::Operation::ALERT_ADMINS).dispatch();
	}

	void Stage::set_clock(int8_t hour, int8_t min, int8_t sec)
	{
		clock_hour = hour;
		clock_min = min;
		clock_sec = sec;
		clock_active = true;
		countdown_active = false;
	}

	void Stage::set_countdown(int32_t seconds)
	{
		countdown_seconds = seconds;
		countdown_active = true;
		clock_active = false;
	}

	void Stage::clear_clock()
	{
		clock_active = false;
		countdown_active = false;
	}

	bool Stage::is_clock_active() const
	{
		return clock_active;
	}

	bool Stage::is_countdown_active() const
	{
		return countdown_active;
	}

	int8_t Stage::get_clock_hour() const
	{
		return clock_hour;
	}

	int8_t Stage::get_clock_min() const
	{
		return clock_min;
	}

	int8_t Stage::get_clock_sec() const
	{
		return clock_sec;
	}

	int32_t Stage::get_countdown_seconds() const
	{
		return countdown_seconds;
	}

	void Stage::set_combo(int32_t count)
	{
		combo_count = count;
	}

	int32_t Stage::get_combo() const
	{
		return combo_count;
	}

	bool Stage::is_combo_active() const
	{
		return combo_count > 0;
	}

	void Stage::set_energy(int32_t amount)
	{
		energy_amount = std::min(std::max(amount, 0), 10000);
	}

	int32_t Stage::get_energy() const
	{
		return energy_amount;
	}

	bool Stage::is_energy_active() const
	{
		return energy_amount > 0;
	}

}
