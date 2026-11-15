// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <chrono>
#include <future>
#include <string>
#include <thread>

#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>
#include <gtest/gtest.h>

#include "Common/Event.h"
#include "Common/HttpRequest.h"

namespace
{
using namespace std::chrono_literals;

// Only loopback traffic; normally withhold the response until teardown.
class TestHttpServer
{
public:
  ~TestHttpServer() { Stop(); }

  bool Start(bool respond = false)
  {
    if (m_listener.listen(0, sf::IpAddress::LocalHost) != sf::Socket::Status::Done)
      return false;
    m_listener.setBlocking(false);
    m_thread = std::jthread([this, respond](std::stop_token stop) {
      sf::TcpSocket peer;
      while (!stop.stop_requested())
      {
        if (m_listener.accept(peer) == sf::Socket::Status::Done)
          break;
        std::this_thread::sleep_for(1ms);
      }
      peer.setBlocking(false);
      while (!stop.stop_requested())
      {
        std::array<char, 4096> bytes;
        std::size_t received = 0;
        const auto status = peer.receive(bytes.data(), bytes.size(), received);
        if (received != 0)
        {
          m_received.Set();
          if (respond)
          {
            const std::string response =
                "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK";
            peer.setBlocking(true);
            (void)peer.send(response.data(), response.size());
            break;
          }
        }
        if (status == sf::Socket::Status::Disconnected || status == sf::Socket::Status::Error)
          break;
        std::this_thread::sleep_for(1ms);
      }
    });
    return true;
  }

  std::string Url() const
  {
    return "http://127.0.0.1:" + std::to_string(m_listener.getLocalPort()) + "/stall";
  }
  bool WaitForRequest() { return m_received.WaitFor(5s); }
  void Stop()
  {
    if (m_thread.joinable())
    {
      m_thread.request_stop();
      m_thread.join();
    }
    m_listener.close();
  }

private:
  sf::TcpListener m_listener;
  Common::Event m_received;
  std::jthread m_thread;
};

TEST(HttpRequestTest, CancellingAnActiveTransferDoesNotWaitForTheNetworkTimeout)
{
  TestHttpServer server;
  ASSERT_TRUE(server.Start());
  Common::HttpRequest request(60s);
  ASSERT_TRUE(request.IsValid());
  auto result = std::async(std::launch::async, [&] { return request.Get(server.Url()); });
  const bool received = server.WaitForRequest();
  request.Cancel();
  const auto completion = result.wait_for(5s);
  // Always release the connection before asserting, including if cancellation regresses.
  server.Stop();
  EXPECT_TRUE(received);
  EXPECT_EQ(completion, std::future_status::ready);
  EXPECT_FALSE(result.get());
}

TEST(HttpRequestTest, CancellationRejectsFutureRequestsWithoutInvokingProgressCallbacks)
{
  int callbacks = 0;
  Common::HttpRequest request(60s, [&](s64, s64, s64, s64) {
    ++callbacks;
    return true;
  });
  request.Cancel();
  request.Cancel();
  EXPECT_FALSE(request.Get("http://127.0.0.1:1/cancelled"));
  EXPECT_FALSE(request.Post("http://127.0.0.1:1/cancelled", "payload"));
  EXPECT_EQ(callbacks, 0);
}

TEST(HttpRequestTest, CallerProgressCallbackCanStillAbortATransfer)
{
  TestHttpServer server;
  ASSERT_TRUE(server.Start());
  int callbacks = 0;
  Common::HttpRequest request(5s, [&](s64, s64, s64, s64) {
    ++callbacks;
    return false;
  });
  EXPECT_FALSE(request.Get(server.Url()));
  EXPECT_GT(callbacks, 0);
}

TEST(HttpRequestTest, RequestsWithoutAUserProgressCallbackStillSucceed)
{
  TestHttpServer server;
  ASSERT_TRUE(server.Start(true));
  Common::HttpRequest request(5s);
  const auto result = request.Get(server.Url());
  ASSERT_TRUE(result);
  EXPECT_EQ(std::string(result->begin(), result->end()), "OK");
}
}  // namespace
