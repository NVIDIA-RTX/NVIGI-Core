// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Unit Tests for nvigi.plugin.net
//
// This file contains comprehensive unit tests for the network plugin,
// testing all security features, configurations, and error handling.

#pragma once

#include "source/plugins/nvigi.net/nvigi_net.h"
#include "source/plugins/nvigi.net/net.h"
#include "external/json/source/nlohmann/json.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#ifdef NVIGI_WINDOWS
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

namespace nvigi
{
namespace net
{
namespace tests
{

#ifdef NVIGI_WINDOWS
using TestSocket = SOCKET;
constexpr TestSocket kInvalidTestSocket = INVALID_SOCKET;

inline void closeTestSocket(TestSocket socket)
{
    closesocket(socket);
}
#else
using TestSocket = int;
constexpr TestSocket kInvalidTestSocket = -1;

inline void closeTestSocket(TestSocket socket)
{
    close(socket);
}
#endif

//! Minimal loopback HTTP server for deterministic transport tests.
//! It accepts one request, optionally delays, then returns the configured response.
class LoopbackHttpServer
{
public:
    explicit LoopbackHttpServer(
        std::string responseBody,
        std::chrono::milliseconds responseDelay = std::chrono::milliseconds(0),
        int statusCode = 200)
        : m_responseBody(std::move(responseBody))
        , m_responseDelay(responseDelay)
        , m_statusCode(statusCode)
    {
#ifdef NVIGI_WINDOWS
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            return;
        }
        m_winsockInitialized = true;
#endif

        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == kInvalidTestSocket)
        {
            return;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;

        if (bind(m_listenSocket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
            listen(m_listenSocket, 1) != 0)
        {
            closeTestSocket(m_listenSocket);
            m_listenSocket = kInvalidTestSocket;
            return;
        }

#ifdef NVIGI_WINDOWS
        int addressLength = sizeof(address);
#else
        socklen_t addressLength = sizeof(address);
#endif
        if (getsockname(m_listenSocket, reinterpret_cast<sockaddr*>(&address), &addressLength) != 0)
        {
            closeTestSocket(m_listenSocket);
            m_listenSocket = kInvalidTestSocket;
            return;
        }

        m_port = ntohs(address.sin_port);
        m_thread = std::thread([this]() { run(); });
        m_ready = true;
    }

    ~LoopbackHttpServer()
    {
        m_stopRequested = true;
        m_waitCondition.notify_all();

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        if (m_listenSocket != kInvalidTestSocket)
        {
            closeTestSocket(m_listenSocket);
            m_listenSocket = kInvalidTestSocket;
        }

#ifdef NVIGI_WINDOWS
        if (m_winsockInitialized)
        {
            WSACleanup();
        }
#endif
    }

    LoopbackHttpServer(const LoopbackHttpServer&) = delete;
    LoopbackHttpServer& operator=(const LoopbackHttpServer&) = delete;

    bool isReady() const
    {
        return m_ready;
    }

    std::string url() const
    {
        return "http://127.0.0.1:" + std::to_string(m_port) + "/";
    }

private:
    bool receiveRequestHeaders(TestSocket client)
    {
        constexpr size_t kMaxRequestHeaderSize = 16 * 1024;
        constexpr auto kRequestTimeout = std::chrono::seconds(2);
        const auto deadline = std::chrono::steady_clock::now() + kRequestTimeout;
        std::string request;
        request.reserve(1024);

        while (!m_stopRequested && request.size() < kMaxRequestHeaderSize &&
            std::chrono::steady_clock::now() < deadline)
        {
            fd_set sockets;
            FD_ZERO(&sockets);
            FD_SET(client, &sockets);

            timeval timeout{};
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

#ifdef NVIGI_WINDOWS
            int selectResult = select(0, &sockets, nullptr, nullptr, &timeout);
#else
            int selectResult = select(client + 1, &sockets, nullptr, nullptr, &timeout);
#endif
            if (selectResult == 0)
            {
                continue;
            }
            if (selectResult < 0)
            {
                return false;
            }

            char buffer[1024];
            int received = recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
            if (received <= 0)
            {
                return false;
            }

            request.append(buffer, static_cast<size_t>(received));
            if (request.find("\r\n\r\n") != std::string::npos)
            {
                return true;
            }
        }

        return false;
    }

    void run()
    {
        while (!m_stopRequested)
        {
            fd_set sockets;
            FD_ZERO(&sockets);
            FD_SET(m_listenSocket, &sockets);

            timeval timeout{};
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

#ifdef NVIGI_WINDOWS
            int selectResult = select(0, &sockets, nullptr, nullptr, &timeout);
#else
            int selectResult = select(m_listenSocket + 1, &sockets, nullptr, nullptr, &timeout);
#endif
            if (selectResult == 0)
            {
                continue;
            }
            if (selectResult < 0 || m_stopRequested)
            {
                return;
            }

            TestSocket client = accept(m_listenSocket, nullptr, nullptr);
            if (client == kInvalidTestSocket)
            {
                return;
            }

            // Consume the request before closing the connection. On Windows, closing a
            // socket with unread inbound data can reset the connection and make curl
            // discard an otherwise complete response as CURLE_RECV_ERROR.
            if (!receiveRequestHeaders(client))
            {
                closeTestSocket(client);
                return;
            }

            if (m_responseDelay.count() > 0)
            {
                std::unique_lock<std::mutex> lock(m_waitMutex);
                if (m_waitCondition.wait_for(lock, m_responseDelay, [this]() { return m_stopRequested.load(); }))
                {
                    closeTestSocket(client);
                    return;
                }
            }

            const char* reason = m_statusCode == 200 ? "OK" :
                (m_statusCode == 503 ? "Service Unavailable" : "Error");
            std::string response =
                "HTTP/1.1 " + std::to_string(m_statusCode) + " " + reason + "\r\n" +
                "Content-Type: application/octet-stream\r\n" +
                "Content-Length: " + std::to_string(m_responseBody.size()) + "\r\n" +
                "Connection: close\r\n\r\n" +
                m_responseBody;

            size_t sent = 0;
            while (sent < response.size() && !m_stopRequested)
            {
#ifdef NVIGI_WINDOWS
                constexpr int sendFlags = 0;
#else
                constexpr int sendFlags = MSG_NOSIGNAL;
#endif
                int chunk = send(client, response.data() + sent,
                    static_cast<int>(response.size() - sent), sendFlags);
                if (chunk <= 0)
                {
                    break;
                }
                sent += static_cast<size_t>(chunk);
            }

#ifdef NVIGI_WINDOWS
            shutdown(client, 1); // SD_SEND is not defined by legacy winsock.h.
#else
            shutdown(client, SHUT_WR);
#endif
            closeTestSocket(client);
            return;
        }
    }

    std::string m_responseBody;
    std::chrono::milliseconds m_responseDelay;
    int m_statusCode = 200;
    uint16_t m_port = 0;
    TestSocket m_listenSocket = kInvalidTestSocket;
    std::thread m_thread;
    std::atomic<bool> m_stopRequested = false;
    std::mutex m_waitMutex;
    std::condition_variable m_waitCondition;
    bool m_ready = false;
#ifdef NVIGI_WINDOWS
    bool m_winsockInitialized = false;
#endif
};

//! Helper: Create default secure parameters
inline net::Parameters createSecureParams(const std::string& url)
{
    net::Parameters params;
    params.url = url.c_str();
    params.verifySSLPeer = true;
    params.verifySSLHost = true;
    params.timeoutSeconds = 30;
    params.connectionTimeoutSeconds = 10;
    params.maxResponseSizeBytes = 10485760; // 10 MB
    params.allowHTTP = false;
    params.followRedirects = false;
    params.redactAuthInLogs = true;
    return params;
}

//! Helper: Create JSON body for LLM chat completion
inline json createChatCompletionBody(const std::string& userMessage, bool stream = false)
{
    return json::parse(R"({
        "model": "nvidia/nemotron-3.5-lightning-30b-a3b",
        "messages": [
            {
                "role": "system",
                "content": "You are a helpful AI assistant. Respond concisely."
            },
            {
                "role": "user",
                "content": ")" + userMessage + R"("
            }
        ],
        "stream": )" + (stream ? "true" : "false") + R"(,
        "temperature": 0.5,
        "top_p": 1,
        "max_tokens": 256
    })");
}

//! Helper: Parse JSON response from types::string
inline json parseResponse(const types::string& response)
{
    try
    {
        return json::parse(response.c_str());
    }
    catch (...)
    {
        return json::object();
    }
}

} // namespace tests
} // namespace net

// ============================================================================
// TEST CASES - Network Plugin Security Tests
// ============================================================================

TEST_CASE("net_ssl_verification_enabled", "[net][security][ssl]")
{
    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);
    REQUIRE(inet != nullptr);
    
    // Test that valid HTTPS endpoint works with SSL verification
    auto testParams = nvigi::net::tests::createSecureParams("https://www.google.com");
    testParams.verifySSLPeer = true;
    testParams.verifySSLHost = true;
    
    types::string responseStr;
    Result res = inet->nvcfGet(testParams, responseStr);
    
    // Should succeed with valid SSL certificate
    bool passed = (res == kResultOk);
    NVIGI_LOG_TEST_INFO("[%s] SSL Verification - %s", 
        passed ? "PASS" : "FAIL",
        passed ? "SSL verification working correctly" : "Failed to connect");
    
    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

TEST_CASE("net_http_blocking", "[net][security][http]")
{
    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);
    
    // Test that HTTP is blocked by default
    auto testParams = nvigi::net::tests::createSecureParams("http://127.0.0.1/");
    testParams.allowHTTP = false;
    
    types::string responseStr;
    Result res = inet->nvcfGet(testParams, responseStr);
    
    // Should fail because HTTP is not allowed
    bool passed = (res != kResultOk);
    NVIGI_LOG_TEST_INFO("[%s] HTTP Blocking - %s", 
        passed ? "PASS" : "FAIL",
        passed ? "HTTP correctly blocked" : "HTTP should be blocked");
    
    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

TEST_CASE("net_http_allowed", "[net][http]")
{
    nvigi::net::tests::LoopbackHttpServer server("ok");
    REQUIRE(server.isReady());

    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);

    auto testParams = nvigi::net::tests::createSecureParams(server.url());
    testParams.allowHTTP = true;

    types::string responseStr;
    Result res = inet->nvcfGet(testParams, responseStr);

    bool passed = res == nvigi::kResultOk && responseStr == "ok";
    NVIGI_LOG_TEST_INFO("[%s] HTTP Allowed - loopback response received",
        passed ? "PASS" : "FAIL");

    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

TEST_CASE("net_invalid_url", "[net][http][errors]")
{
    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);

    auto testParams = nvigi::net::tests::createSecureParams("https://[invalid");

    types::string responseStr;
    Result res = inet->nvcfGet(testParams, responseStr);

    bool passed = res == nvigi::net::kResultNetCurlError;
    NVIGI_LOG_TEST_INFO("[%s] Invalid URL Handling - malformed URL rejected",
        passed ? "PASS" : "FAIL");

    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

TEST_CASE("net_http_server_error", "[net][http][errors]")
{
    nvigi::net::tests::LoopbackHttpServer server("service unavailable", std::chrono::milliseconds(0), 503);
    REQUIRE(server.isReady());

    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);

    auto testParams = nvigi::net::tests::createSecureParams(server.url());
    testParams.allowHTTP = true;

    types::string responseStr;
    Result res = inet->nvcfGet(testParams, responseStr);

    bool passed = res == nvigi::net::kResultNetServerError &&
        responseStr == "service unavailable";
    NVIGI_LOG_TEST_INFO("[%s] HTTP Server Error - status and response body propagated",
        passed ? "PASS" : "FAIL");

    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

TEST_CASE("net_response_size_limit", "[net][security][limits]")
{
    nvigi::net::tests::LoopbackHttpServer server(std::string(4096, 'x'));
    REQUIRE(server.isReady());

    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);
    
    // Test that responses exceeding size limit are aborted
    auto testParams = nvigi::net::tests::createSecureParams(server.url());
    testParams.maxResponseSizeBytes = 1024;
    testParams.allowHTTP = true;
    testParams.timeoutSeconds = 15;
    
    types::string responseStr;
    Result res = inet->nvcfGet(testParams, responseStr);
    
    bool passed = res == nvigi::net::kResultNetResponseTooLarge;
    NVIGI_LOG_TEST_INFO("[%s] Response Size Limit - %s", 
        passed ? "PASS" : "FAIL",
        passed ? "Large response aborted with the expected result" : "Unexpected result");
    
    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

TEST_CASE("net_timeout_configuration", "[net][security][timeout]")
{
    nvigi::net::tests::LoopbackHttpServer server(
        "delayed response", std::chrono::seconds(10));
    REQUIRE(server.isReady());

    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);
    
    // Test that timeout is enforced
    auto testParams = nvigi::net::tests::createSecureParams(server.url());
    testParams.timeoutSeconds = 1;
    testParams.allowHTTP = true;
    
    auto start = std::chrono::steady_clock::now();
    types::string responseStr;
    Result res = inet->nvcfGet(testParams, responseStr);
    auto end = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Check the specific timeout result and reject immediate unrelated failures.
    bool passed = res == nvigi::net::kResultNetTimeout && duration >= 500 && duration < 4000;
    NVIGI_LOG_TEST_INFO("[%s] Timeout Configuration - %s (actual: %lldms)",
        passed ? "PASS" : "FAIL",
        passed ? "Timeout enforced correctly" : "Unexpected result or duration",
        duration);
    
    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

TEST_CASE("net_nvidia_chat_api", "[net][api][nvidia]")
{
    // Check if API key is set
    std::string token;
    nvigi::extra::getEnvVar("NVIDIA_INTEGRATE_KEY_NVDEV", token);
    if (token.empty())
    {
        NVIGI_LOG_TEST_WARN("NVIDIA_INTEGRATE_KEY_NVDEV not set, skipping NVIDIA Chat API test");
        return; // Allow local testing if CI does not have the token
    }
    
    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);
    
    // Set token and create parameters
    inet->nvcfSetToken(token.c_str());
    
    auto testParams = nvigi::net::tests::createSecureParams("https://integrate.api.nvidia.com/v1/chat/completions");
    testParams.headers = { "Content-Type: application/json" };
    testParams.maxResponseSizeBytes = 5242880; // 5 MB
    testParams.timeoutSeconds = 60;
    
    // Create request body using your exact JSON format
    json body = R"({
        "model" : "nvidia/nemotron-3.5-lightning-30b-a3b",
        "messages": [
            {
                "role":"system",
                "content":"This is a transcript of a dialog between Kai (male) and ramen shop owner named Jin (male). They are old friends and know each other really well. You are Jin, user will be Kai. Answer with short sentences, everyone takes one turn."
            },
            {
                "role":"user",
                "content":"Hey Jin, what's up?"
            }
        ],
        "stream": false,
        "temperature": 0.5,
        "top_p" : 1,
        "max_tokens": 1024
    })"_json;
    
    std::string jsonStr = body.dump();
    testParams.data.resize(jsonStr.size());
    std::memcpy(testParams.data.data(), jsonStr.c_str(), jsonStr.size());
    
    // Make request
    types::string responseStr;
    Result res = inet->nvcfPost(testParams, responseStr);
    
    bool passed = false;
    std::string message;
    
    if (res == kResultOk)
    {
        json response = nvigi::net::tests::parseResponse(responseStr);
        bool hasChoices = response.contains("choices") && response["choices"].is_array();
        bool hasContent = hasChoices && response["choices"].size() > 0 &&
                        response["choices"][0].contains("message") &&
                        response["choices"][0]["message"].contains("content");
        
        passed = hasContent;
        if (passed)
        {
            std::string content = response["choices"][0]["message"]["content"].get<std::string>();
            message = "Chat completion successful";
            NVIGI_LOG_TEST_INFO("Response: %s", content.c_str());
        }
        else
        {
            message = "Invalid response structure";
        }
    }
    else
    {
        message = "API request failed";
    }
    
    NVIGI_LOG_TEST_INFO("[%s] NVIDIA Chat API - %s", 
        passed ? "PASS" : "FAIL", message.c_str());
    
    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

TEST_CASE("net_nvidia_streaming_api", "[net][api][nvidia][streaming]")
{
    // Check if API key is set
    std::string token;
    nvigi::extra::getEnvVar("NVIDIA_INTEGRATE_KEY_NVDEV", token);
    if (token.empty())
    {
        NVIGI_LOG_TEST_WARN("NVIDIA_INTEGRATE_KEY_NVDEV not set, skipping NVIDIA Streaming API test");
        return; // Allow local testing if CI does not have the token
    }
    
    nvigi::net::INet* inet{};
    auto result = nvigiGetInterfaceDynamic(plugin::net::kId, &inet, params.nvigiLoadInterface);
    REQUIRE(result == nvigi::kResultOk);
    
    // Set token
    inet->nvcfSetToken(token.c_str());
    
    auto testParams = nvigi::net::tests::createSecureParams("https://integrate.api.nvidia.com/v1/chat/completions");
    testParams.headers = { "Content-Type: application/json" };
    testParams.maxStreamingSizeBytes = 5242880; // 5 MB
    testParams.timeoutSeconds = 60;
    
    // Create request body with streaming enabled
    json body = nvigi::net::tests::createChatCompletionBody("Say hello in 3 words.", true);
    std::string jsonStr = body.dump();
    testParams.data.resize(jsonStr.size());
    std::memcpy(testParams.data.data(), jsonStr.c_str(), jsonStr.size());
    
    // Streaming callback
    std::string streamedContent;
    auto callback = [](const char* data, size_t size, void* userdata) -> size_t {
        auto* content = static_cast<std::string*>(userdata);
        content->append(data, size);
        return size;
    };
    
    // Make streaming request
    Result res = inet->nvcfPostStreaming(testParams, callback, &streamedContent);
    
    bool passed = (res == kResultOk && !streamedContent.empty());
    NVIGI_LOG_TEST_INFO("[%s] NVIDIA Streaming API - %s (%zu bytes received)", 
        passed ? "PASS" : "FAIL",
        passed ? "Streaming successful" : "Streaming failed",
        streamedContent.size());
    
    if (passed)
    {
        NVIGI_LOG_TEST_INFO("Streamed content sample: %s", 
            streamedContent.substr(0, std::min<size_t>(200, streamedContent.size())).c_str());
    }
    
    params.nvigiUnloadInterface(plugin::net::kId, inet);
    REQUIRE(passed);
}

} // namespace nvigi
