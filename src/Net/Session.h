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

#include <chrono>
#include "Cryptography.h"
#include "PacketSwitch.h"
#include "Login.h"

#include "../Error.h"
#include "../MapleStory.h"

#include "../Template/Singleton.h"

#ifdef USE_ASIO
#include "SocketAsio.h"
#else
#include "SocketWinsock.h"
#endif

namespace ms
{
	class Session : public Singleton<Session>
	{
	public:
		Session();
		~Session();

		// Connect using host and port from the configuration file
		Error init();
		// Send a packet to the server
		void write(int8_t* bytes, size_t length);
		// Check for incoming packets and handle them
		void read();
		// Closes the current connection and opens a new one with default connection settings
		void reconnect();
		// Closes the current connection and opens a new one
		void reconnect(const char* address, const char* port);
		// Check if the connection is alive
		bool is_connected() const;

		void remember_login(const std::string& account, const std::string& password);
		void remember_character(int32_t id, const std::string& pic);
		void entered_game();
		void restore_world_selection();
		void cancel_restore();
		bool is_restoring() const { return restoring; }
		bool restore_character(const std::vector<CharEntry>& characters, int8_t pic_mode);
	private:
		std::string restore_account, restore_password, restore_pic;
		int32_t restore_cid = 0;
		uint8_t restore_world = 0, restore_channel = 0;
		bool restoring = false, in_game = false;
		std::chrono::steady_clock::time_point retry_at{};
		bool init(const char* host, const char* port);
		void process(const int8_t* bytes, size_t available);

		Cryptography cryptography;
		PacketSwitch packetswitch;

		int8_t buffer[MAX_PACKET_LENGTH];
		int8_t header[HEADER_LENGTH];
		size_t header_pos = 0;
		size_t connection_id = 0;
		size_t length;
		size_t pos;
		bool connected;

#ifdef USE_ASIO
		SocketAsio socket;
#else
		SocketWinsock socket;
#endif
	};
}