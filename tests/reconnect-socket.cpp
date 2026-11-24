// Standalone transport regression: split handshake, idle, EOF, retry and timeout.
#include "../src/Net/SocketAsio.h"
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
int main()
{
 using namespace std::chrono_literals;
 asio::io_context io;
 asio::ip::tcp::acceptor listener(io, {asio::ip::tcp::v4(), 0});
 auto port = std::to_string(listener.local_endpoint().port());
 std::thread server([&] {
  asio::ip::tcp::socket peer(io); listener.accept(peer);
  char hello[16]{};
  asio::write(peer, asio::buffer(hello, 3));
  std::this_thread::sleep_for(20ms);
  asio::write(peer, asio::buffer(hello + 3, 13));
  std::this_thread::sleep_for(100ms);
 });
 ms::SocketAsio client;
 assert(client.open("127.0.0.1", port.c_str()));
 bool connected = true;
 assert(client.receive(&connected) == 0 && connected);
 server.join();
 for (int i = 0; i < 100 && connected; ++i) {
  client.receive(&connected); std::this_thread::sleep_for(2ms);
 }
 assert(!connected);
 client.close();
 std::thread silent([&] {
  asio::ip::tcp::socket peer(io); listener.accept(peer);
  std::this_thread::sleep_for(1800ms);
 });
 auto start = std::chrono::steady_clock::now();
 assert(!client.open("127.0.0.1", port.c_str()));
 auto elapsed = std::chrono::steady_clock::now() - start;
 assert(elapsed >= 1400ms && elapsed < 1750ms);
 silent.join();
 std::thread restarted([&] {
  asio::ip::tcp::socket peer(io); listener.accept(peer);
  char hello[16]{}; asio::write(peer, asio::buffer(hello));
 });
 assert(client.open("127.0.0.1", port.c_str()));
 restarted.join();
 client.close(); listener.close();
 assert(!client.open("127.0.0.1", port.c_str()));
 std::cout << "PASS: split handshake, idle, EOF, bounded timeout, restart, refused connection\n";
}
