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
#include "Session.h"

#include <algorithm>
#include <cstring>

#include "../Configuration.h"
#include "../Gameplay/Stage.h"
#include "../IO/UI.h"
#include "../IO/UITypes/UILoginWait.h"
#include "Packets/LoginPackets.h"
#include "Packets/SelectCharPackets.h"

namespace ms
{
	Session::Session()
	{
		connected = false;
		length = 0;
		pos = 0;
	}

	Session::~Session()
	{
		if (connected)
			socket.close();
	}

	bool Session::init(const char* host, const char* port)
	{
		// A new stream cannot continue a packet from the previous connection.
		length = pos = header_pos = 0;
		++connection_id;
		connected = socket.open(host, port);

		if (connected)
		{
			// Read keys necessary for communicating with the server
			cryptography = { socket.get_buffer() };
		}

		return connected;
	}

	Error Session::init()
	{
		std::string HOST = Setting<ServerIP>::get().load();
		std::string PORT = Setting<ServerPort>::get().load();

		if (!init(HOST.c_str(), PORT.c_str()))
			return Error::CONNECTION;

		return Error::NONE;
	}

	void Session::reconnect(const char* address, const char* port)
	{
		// Close the current connection and open a new one
		socket.close();
		init(address, port);
	}

	void Session::process(const int8_t* bytes, size_t available)
	{
		const auto stream = connection_id;
		while (available > 0 && connected && stream == connection_id)
		{
			if (length == 0)
			{
				const auto count = std::min(HEADER_LENGTH - header_pos, available);
				std::memcpy(header + header_pos, bytes, count);
				header_pos += count;
				bytes += count;
				available -= count;
				if (header_pos < HEADER_LENGTH)
					return;

				length = cryptography.check_length(header);
				header_pos = 0;
				// Every body must contain an opcode and fit the receive buffer.
				if (length < MIN_PACKET_LENGTH - HEADER_LENGTH || length > sizeof(buffer))
				{
					socket.close();
					connected = false;
					length = pos = 0;
					return;
				}
			}

			const auto count = std::min(length - pos, available);
			std::memcpy(buffer + pos, bytes, count);
			pos += count;
			bytes += count;
			available -= count;
			if (pos < length)
				return;

			const auto packet_length = length;
			cryptography.decrypt(buffer, packet_length);
			length = pos = 0;
			try
			{
				packetswitch.forward(buffer, packet_length);
			}
			catch (const PacketError&)
			{
			}
			catch (const std::exception&)
			{
				// Keep existing handler error isolation.
			}
			// A handler can reconnect (e.g. channel change). Never feed the old
			// stream's remaining bytes into that connection's fresh crypto state.
		}
	}

	void Session::write(int8_t* packet_bytes, size_t packet_length)
	{
		if (!connected)
			return;

		int8_t header[HEADER_LENGTH];
		cryptography.create_header(header, packet_length);
		cryptography.encrypt(packet_bytes, packet_length);

		connected = socket.dispatch(header, HEADER_LENGTH)
			&& socket.dispatch(packet_bytes, packet_length);
	}

	void Session::read()
	{
		const auto now = std::chrono::steady_clock::now();
		if (!connected && in_game && !restore_password.empty())
		{
			in_game = false;
			restoring = true;
			restore_world = Configuration::get().get_worldid();
			restore_channel = Configuration::get().get_channelid();
			Stage::get().clear();
			UI::get().change_state(UI::State::LOGIN);
			UI::get().enable();
			UI::get().emplace<UILoginWait>([] { Session::get().cancel_restore(); });
			retry_at = now;
		}
		if (restoring && now >= retry_at)
		{
			socket.close();
			connected = false;
			if (!init())
			{
				UI::get().emplace<UILoginWait>([] { Session::get().cancel_restore(); });
				LoginPacket(restore_account, restore_password).dispatch();
				// A connected server can still be starting its channels.
				retry_at = now + std::chrono::seconds(20);
			}
			else
				retry_at = now + std::chrono::seconds(5);
		}
		if (!connected)
			return;
		// TCP reads may split either the header or the body at any byte.
		size_t result = socket.receive(&connected);

		if (result > 0)
		{
			// Retrieve buffer from the socket and process it
			const int8_t* bytes = socket.get_buffer();
			process(bytes, result);
		}
	}

	void Session::reconnect()
	{
		if (!restoring) in_game = false;
		std::string HOST = Setting<ServerIP>::get().load();
		std::string PORT = Setting<ServerPort>::get().load();

		reconnect(HOST.c_str(), PORT.c_str());
	}

	void Session::remember_login(const std::string& account, const std::string& password)
	{
		cancel_restore();
		restore_account = account;
		restore_password = password;
	}

	void Session::remember_character(int32_t id, const std::string& pic)
	{
		restore_cid = id;
		restore_pic = pic;
	}

	void Session::restore_world_selection()
	{
		Configuration::get().set_worldid(restore_world);
		Configuration::get().set_channelid(restore_channel);
		CharlistRequestPacket(restore_world, restore_channel).dispatch();
	}

	void Session::entered_game()
	{
		in_game = true;
		restoring = false;
	}

	void Session::cancel_restore()
	{
		restoring = in_game = false;
	}

	bool Session::restore_character(const std::vector<CharEntry>& characters, int8_t pic_mode)
	{
		if (!restoring)
			return false;
		const auto found = std::find_if(characters.begin(), characters.end(),
			[this](const CharEntry& c) { return c.id == restore_cid; });
		if (found == characters.end() || pic_mode == 0 || (pic_mode != 2 && restore_pic.empty()))
		{
			cancel_restore();
			return false;
		}
		if (pic_mode == 2)
			SelectCharPacket(restore_cid).dispatch();
		else
			SelectCharPicPacket(restore_pic, restore_cid).dispatch();
		return true;
	}

	bool Session::is_connected() const
	{
		return connected;
	}
}