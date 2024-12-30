// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <chrono>
#include <string>

#include "source/core/nvigi.file/file.h"
#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>

// Function to split the input string by commas
std::unordered_map<std::string, std::string> parseAuthorizationString(const std::string& authString) {
    std::unordered_map<std::string, std::string> result;
    std::istringstream stream(authString);
    std::string token;

    while (std::getline(stream, token, ',')) {
        if (token == "authorization") {
            // Get the TOKEN
            std::getline(stream, token, ',');
            result["TOKEN"] = token.substr(token.find(" "));
        }
        else if (token == "function-id") {
            // Get the FUNC
            std::getline(stream, token, ',');
            result["FUNC"] = token;
        }
        else if (token == "function-version-id") {
            // Get the VERS
            std::getline(stream, token, ',');
            result["VERS"] = token;
        }
    }

    return result;
}

using grpc::Status;
using grpc::StatusCode;

namespace nvigi::net {

inline std::vector<std::string> split(std::string& input_string, const char delim = ' ')
{
    std::stringstream ss(input_string);
    std::string s;
    std::vector<std::string> splat;
    while (getline(ss, s, delim)) {
        splat.push_back(s);
    }

    return splat;
}

class CustomAuthenticator : public ::grpc::MetadataCredentialsPlugin {
public:
    CustomAuthenticator(const std::string& metadata) : metadata_(metadata) {}

    ::grpc::Status GetMetadata(
        ::grpc::string_ref service_url, ::grpc::string_ref method_name,
        const ::grpc::AuthContext& channel_auth_context,
        std::multimap<::grpc::string, ::grpc::string>* metadata) override
    {
        auto key_value_pairs = split(metadata_, ',');
        if (key_value_pairs.size() % 2) {
            throw std::runtime_error("Error: metadata must contain key value pairs.");
        }
        for (size_t i = 0; i < key_value_pairs.size(); i += 2) {
            metadata->insert(std::make_pair(key_value_pairs.at(i), key_value_pairs.at(i + 1)));
        }
        return ::grpc::Status::OK;
    }

private:
    std::string metadata_;
};

/// Utility function to create a GRPC channel
/// This will only return when the channel has been created, thus making sure that the subsequent
/// GRPC call won't have additional latency due to channel creation
///
/// @param uri URI of server
/// @param credentials GRPC credentials
/// @param timeout_ms The maximum time (in milliseconds) to wait for channel creation. Throws
/// exception if time is exceeded

inline std::shared_ptr<::grpc::Channel> CreateChannelBlocking(
    const std::string& uri, const std::shared_ptr<::grpc::ChannelCredentials> credentials,
    uint64_t timeout_ms = 10000)
{
    auto channel = ::grpc::CreateChannel(uri, credentials);

    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(timeout_ms);
    auto reached_required_state = channel->WaitForConnected(deadline);
    auto state = channel->GetState(true);

    if (!reached_required_state) 
    {
        throw std::runtime_error("Unable to establish connection to server. Current state: " + std::to_string((int)state));
    }

    NVIGI_LOG_VERBOSE("Connected to '%s'", uri.c_str());

    return channel;
}

/// Utility function to create GRPC credentials
/// Returns shared ptr to GrpcChannelCredentials
/// @param use_ssl Boolean flag that controls if ssl encryption should be used
/// @param ssl_cert Path to the certificate file
inline std::shared_ptr<::grpc::ChannelCredentials> CreateChannelCredentials(bool use_ssl, const std::string& cacert, const std::string& metadata)
{
    std::shared_ptr<::grpc::ChannelCredentials> creds;

    if (use_ssl) 
    {
        ::grpc::SslCredentialsOptions ssl_opts;
        if (!cacert.empty()) 
        {
            ssl_opts.pem_root_certs = cacert;
        }
        else
        {
            throw std::runtime_error("When SSL is required CA certificate must be provided");
        }
        creds = ::grpc::SslCredentials(ssl_opts);
    }
    else 
    {
        creds = ::grpc::InsecureChannelCredentials();
    }

    if (!metadata.empty()) {
        auto call_creds =
            ::grpc::MetadataCredentialsFromPlugin(std::unique_ptr<::grpc::MetadataCredentialsPlugin>(
                new net::CustomAuthenticator(metadata)));
        creds = ::grpc::CompositeChannelCredentials(creds, call_creds);
        auto values = parseAuthorizationString(metadata);
        NVIGI_LOG_VERBOSE("Opening %s gRPC channel", use_ssl ? "secure" : "insecure");
        NVIGI_LOG_VERBOSE("# Function: %s", values["FUNC"].c_str());
        NVIGI_LOG_VERBOSE("# Version: %s", values["VERS"].c_str());
        NVIGI_LOG_VERBOSE("# Token: %s", values["TOKEN"].c_str());
    }

    return creds;
}

}  // namespace nvigi::net