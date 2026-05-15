// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_struct.h"

namespace nvigi
{

//! Interface RPCParameters
//!
//! {908F10BF-9051-494D-BB50-F302BEC11D9A}
struct alignas(8) RPCParameters {
    RPCParameters() {}; 
    NVIGI_UID(UID({ 0x908f10bf, 0x9051, 0x494d,{ 0xbb, 0x50, 0xf3, 0x02, 0xbe, 0xc1, 0x1d, 0x9a } }), kStructVersion1)
    bool useSSL = true;
    const char* url{};
    const char* sslCert{};
    const char* metaData{};

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(RPCParameters)

//! Interface RESTParameters
//!
//! IMPORTANT SECURITY NOTICE:
//! ===========================
//! The host application is responsible for ALL security aspects including:
//! - Validating and sanitizing URLs before passing them to the plugin
//! - Using secure protocols (HTTPS) for sensitive data
//! - Validating and sanitizing HTTP headers
//! - Ensuring authentication tokens are handled securely
//! - Setting appropriate timeout and size limits for their use case
//!
//! The plugin provides security controls below but does NOT validate input.
//! Secure defaults are provided, but the host MUST review and configure
//! these settings based on their security requirements and threat model.
//!
//! {AA88BBA7-2217-4569-9C95-0A6EA6BE189C}
struct alignas(8) RESTParameters {
    RESTParameters() {}; 
    NVIGI_UID(UID({ 0xaa88bba7, 0x2217, 0x4569,{ 0x9c, 0x95, 0xa, 0x6e, 0xa6, 0xbe, 0x18, 0x9c } }), kStructVersion3)
    
    //! v1 - Basic parameters
    const char* url{};
    const char* authenticationToken{};
    bool verboseMode{}; // if true, log detailed information about the request/response

    //! v2 - Streaming support
    bool useStreaming{}; // if true, instruct REST protocol to operate in streaming mode if supported by the server

    //! v3 - Security and resource management controls
    //! 
    //! SSL/TLS Security (CRITICAL for production)
    //! ------------------------------------------
    //! verifySSLPeer: Verify the peer's SSL certificate. 
    //!   - Default: true (RECOMMENDED for production)
    //!   - Set to false ONLY for development/testing with self-signed certs
    bool verifySSLPeer = true;
    
    //! verifySSLHost: Verify that the certificate matches the host.
    //!   - Default: true (RECOMMENDED for production)  
    //!   - Prevents man-in-the-middle attacks
    bool verifySSLHost = true;
    
    //! sslCABundlePath: Path to Certificate Authority bundle file.
    //!   - Default: nullptr (uses system default CA bundle)
    //!   - Set to custom path if using private/corporate CAs
    const char* sslCABundlePath{};
    
    //! Timeout Controls (Defense against slow/hung connections)
    //! ---------------------------------------------------------
    //! timeoutSeconds: Maximum time for entire request to complete (0 = no limit).
    //!   - Default: 300 seconds (5 minutes)
    //!   - Set to 0 to disable (NOT RECOMMENDED - can cause resource exhaustion)
    uint32_t timeoutSeconds = 300;
    
    //! connectionTimeoutSeconds: Maximum time to establish connection (0 = no limit).
    //!   - Default: 30 seconds
    //!   - Set to 0 to disable (NOT RECOMMENDED)
    uint32_t connectionTimeoutSeconds = 30;
    
    //! lowSpeedLimit: Minimum transfer speed in bytes/sec (0 = no limit).
    //!   - Default: 1024 bytes/sec (1 KB/s)
    //!   - Abort if speed is below this for lowSpeedTimeSeconds
    //!   - Protects against slowloris-style attacks
    uint32_t lowSpeedLimit = 1024;
    
    //! lowSpeedTimeSeconds: Duration in seconds to tolerate low speed (0 = no limit).
    //!   - Default: 60 seconds
    //!   - Works with lowSpeedLimit to detect stalled transfers
    uint32_t lowSpeedTimeSeconds = 60;
    
    //! Response Size Limits (Defense against memory exhaustion)
    //! ---------------------------------------------------------
    //! maxResponseSizeBytes: Maximum allowed response size (0 = no limit).
    //!   - Default: 104857600 (100 MB)
    //!   - Set to 0 to disable (NOT RECOMMENDED - can cause OOM crashes)
    //!   - Requests exceeding this will be aborted
    uint64_t maxResponseSizeBytes = 104857600;
    
    //! maxStreamingSizeBytes: Maximum total bytes for streaming responses (0 = no limit).
    //!   - Default: 104857600 (100 MB)
    //!   - Only applies when useStreaming=true
    //!   - Set to 0 to disable (NOT RECOMMENDED for untrusted sources)
    uint64_t maxStreamingSizeBytes = 104857600;
    
    //! Protocol Security (Defense against protocol-level attacks)
    //! -----------------------------------------------------------
    //! allowHTTP: Allow non-encrypted HTTP protocol.
    //!   - Default: false (HTTPS only)
    //!   - Set to true ONLY if connecting to trusted local/internal services
    //!   - WARNING: HTTP transmits data in clear text (including auth tokens!)
    bool allowHTTP = false;
    
    //! followRedirects: Allow following HTTP redirects.
    //!   - Default: false (prevents redirect-based attacks)
    //!   - Set to true if redirects are expected and trusted
    //!   - WARNING: Can be exploited to redirect to malicious sites
    bool followRedirects = false;
    
    //! maxRedirects: Maximum number of redirects to follow (if enabled).
    //!   - Default: 5
    //!   - Only applies if followRedirects=true
    //!   - Prevents infinite redirect loops
    uint32_t maxRedirects = 5;
    
    //! Logging Security (Defense against credential leakage)
    //! ------------------------------------------------------
    //! redactAuthInLogs: Redact authentication tokens from verbose logs.
    //!   - Default: true (RECOMMENDED for production)
    //!   - When true, Authorization headers are logged as [REDACTED]
    //!   - Set to false ONLY for debugging in secure environments
    bool redactAuthInLogs = true;

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(RESTParameters)
}
