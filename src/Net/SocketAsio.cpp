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
#include "SocketAsio.h"

#ifdef USE_ASIO
namespace ms
{
	SocketAsio::SocketAsio() : resolver(iocontext), socket(iocontext) {}

	SocketAsio::~SocketAsio()
	{
		if (socket.is_open())
		{
			error_code error;
			socket.close(error);
		}
	}

	bool SocketAsio::open(const char* address, const char* port)
	{
		close();
		iocontext.restart();
		bool success = false;
		asio::steady_timer deadline(iocontext, std::chrono::milliseconds(1500));
		deadline.async_wait([&](const error_code& ec) {
			if (!ec) { resolver.cancel(); error_code ignored; socket.close(ignored); }
		});
		resolver.async_resolve(address, port, [&](const error_code& ec, tcp::resolver::results_type endpoints) {
			if (ec) { deadline.cancel(); return; }
			asio::async_connect(socket, endpoints, [&](const error_code& ec, const tcp::endpoint&) {
				if (ec) { deadline.cancel(); return; }
				asio::async_read(socket, asio::buffer(buffer, HANDSHAKE_LEN),
					[&](const error_code& ec, size_t count) {
						success = !ec && count == HANDSHAKE_LEN;
						deadline.cancel();
					});
			});
		});
		iocontext.run();
		if (!success) close();
		return success;
	}

	bool SocketAsio::close()
	{
		error_code error;
		socket.shutdown(tcp::socket::shutdown_both, error);
		socket.close(error);

		return !error;
	}

	size_t SocketAsio::receive(bool* recvok)
	{
		if (!socket.is_open()) { *recvok = false; return 0; }
		error_code error;
		socket.non_blocking(true, error);
		if (error) { *recvok = false; return 0; }
		size_t result = socket.read_some(asio::buffer(buffer), error);
		error_code ignored;
		socket.non_blocking(false, ignored);
		if (error == asio::error::would_block || error == asio::error::try_again)
			return 0;
		*recvok = !error && !ignored;
		return error ? 0 : result;
	}

	const int8_t* SocketAsio::get_buffer() const
	{
		return buffer;
	}

	bool SocketAsio::dispatch(const int8_t* bytes, size_t length)
	{
		error_code error;
		size_t result = asio::write(socket, asio::buffer(bytes, length), error);

		return !error && (result == length);
	}
}
#endif