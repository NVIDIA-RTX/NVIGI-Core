# nvigi.plugin.net Security Guide

## Overview

The `nvigi.plugin.net` plugin provides networking capabilities with comprehensive security controls. 

> **IMPORTANT**: The host application is responsible for ALL input validation and security decisions. This plugin provides security mechanisms but does NOT validate inputs.

## Security Model

### Host Application Responsibilities

The host application MUST:

1. ✅ Validate and sanitize ALL URLs before passing to the plugin
2. ✅ Use secure protocols (HTTPS) for sensitive data transmission
3. ✅ Validate and sanitize ALL HTTP headers
4. ✅ Securely handle authentication tokens (secure storage, transmission, rotation)
5. ✅ Set appropriate timeout and size limits based on use case
6. ✅ Implement rate limiting and abuse prevention at the application level
7. ✅ Verify the plugin binary's digital signature before loading

### Plugin Responsibilities

The plugin provides:

- ✅ SSL/TLS certificate verification (configurable)
- ✅ Response and streaming size limits
- ✅ Timeout and slow transfer detection
- ✅ Protocol restrictions (HTTPS-only by default)
- ✅ Redirect prevention (disabled by default)
- ✅ Credential redaction in logs
- ✅ Thread-safe initialization
- ✅ Memory safety and error handling

## Security Configuration

All security settings are configured via `RESTParameters` (defined in `nvigi_cloud.h`). Each parameter has secure defaults designed to prevent common attack vectors.

### SSL/TLS Security Settings

#### `verifySSLPeer` (default: `true`)

Verifies the peer's SSL certificate against Certificate Authorities.

**Security Impact**: CRITICAL

- ✅ **Enabled (default)**: Prevents man-in-the-middle (MITM) attacks
- ⚠️ **Disabled**: Allows connections to servers with invalid/self-signed certificates

**When to disable**: ONLY for development/testing with self-signed certificates. NEVER in production.

```cpp
RESTParameters params;
params.verifySSLPeer = true;  // RECOMMENDED for production
```

#### `verifySSLHost` (default: `true`)

Verifies that the certificate's Common Name or Subject Alternative Name matches the host.

**Security Impact**: CRITICAL

- ✅ **Enabled (default)**: Prevents certificate substitution attacks
- ⚠️ **Disabled**: Allows any valid certificate, even for different hosts

**When to disable**: NEVER in production.

```cpp
params.verifySSLHost = true;  // RECOMMENDED for production
```

#### `sslCABundlePath` (default: `nullptr`)

Path to custom Certificate Authority bundle file.

**Security Impact**: HIGH

- Default uses system CA bundle (recommended for public CAs)
- Set to custom path when using private/corporate CAs

```cpp
params.sslCABundlePath = "/path/to/corporate-ca-bundle.crt";
```

### Timeout Controls

Timeouts prevent resource exhaustion from slow or hung connections.

#### `timeoutSeconds` (default: `300`)

Maximum time for entire request to complete (0 = no limit).

**Security Impact**: HIGH - Prevents resource exhaustion

- Default: 5 minutes (reasonable for most API calls)
- Set to 0 to disable (NOT RECOMMENDED - can cause resource exhaustion)

```cpp
params.timeoutSeconds = 60;  // 1 minute for time-sensitive operations
```

#### `connectionTimeoutSeconds` (default: `30`)

Maximum time to establish initial connection (0 = no limit).

**Security Impact**: MEDIUM - Prevents hanging on unreachable hosts

- Default: 30 seconds
- Set to 0 to disable (NOT RECOMMENDED)

```cpp
params.connectionTimeoutSeconds = 10;  // Faster timeout for local services
```

#### `lowSpeedLimit` and `lowSpeedTimeSeconds` (defaults: `1024` bytes/sec, `60` seconds)

Abort transfer if speed drops below `lowSpeedLimit` bytes/sec for `lowSpeedTimeSeconds` seconds.

**Security Impact**: MEDIUM - Protects against slowloris-style attacks

- Default: Abort if speed < 1KB/s for 60 seconds
- Set `lowSpeedLimit=0` to disable (NOT RECOMMENDED)

```cpp
params.lowSpeedLimit = 5120;        // 5 KB/s minimum
params.lowSpeedTimeSeconds = 30;     // for 30 seconds
```

### Response Size Limits

Size limits prevent memory exhaustion attacks.

#### `maxResponseSizeBytes` (default: `104857600` = 100MB)

Maximum allowed response size for non-streaming requests (0 = no limit).

**Security Impact**: HIGH - Prevents out-of-memory crashes

- Default: 100 MB (suitable for large API responses)
- Set to 0 to disable (NOT RECOMMENDED - can cause OOM crashes)
- Requests exceeding this limit are immediately aborted

```cpp
params.maxResponseSizeBytes = 10485760;  // 10 MB for typical JSON responses
```

#### `maxStreamingSizeBytes` (default: `104857600` = 100MB)

Maximum total bytes for streaming responses (0 = no limit).

**Security Impact**: HIGH - Prevents unlimited data consumption

- Only applies when `useStreaming=true`
- Default: 100 MB
- Set to 0 to disable (NOT RECOMMENDED for untrusted sources)

```cpp
params.maxStreamingSizeBytes = 524288000;  // 500 MB for large file downloads
```

### Protocol Security

#### `allowHTTP` (default: `false`)

Allow non-encrypted HTTP protocol.

**Security Impact**: CRITICAL

- ✅ **Disabled (default)**: Only HTTPS allowed (encrypted)
- ⚠️ **Enabled**: Allows HTTP (clear text transmission)

**When to enable**: ONLY for trusted local/internal services. NEVER for internet traffic.

⚠️ **WARNING**: HTTP transmits ALL data in clear text, including authentication tokens!

```cpp
params.allowHTTP = true;   // ONLY for localhost/internal services
params.url = "http://localhost:8080/api";
```

#### `followRedirects` (default: `false`)

Allow following HTTP redirects (3xx responses).

**Security Impact**: MEDIUM - Prevents redirect-based attacks

- ✅ **Disabled (default)**: Request fails on redirect
- ⚠️ **Enabled**: Automatically follows redirects (up to `maxRedirects`)

**When to enable**: Only if redirects are expected and the target service is trusted.

**Risks**:

- Open redirect vulnerabilities
- Redirection to malicious sites
- Protocol downgrade attacks (HTTPS → HTTP)

```cpp
params.followRedirects = true;  // Only if redirects are expected
params.maxRedirects = 3;        // Limit redirect chains
```

### Logging Security

#### `redactAuthInLogs` (default: `true`)

Redact authentication tokens from verbose logs.

**Security Impact**: MEDIUM - Prevents credential leakage in logs

- ✅ **Enabled (default)**: Authorization headers logged as `[REDACTED]`
- ⚠️ **Disabled**: Full headers including tokens logged

**When to disable**: ONLY for debugging in secure, isolated environments. NEVER in production.

```cpp
params.redactAuthInLogs = false;  // ONLY for secure debugging
params.verboseMode = true;         // Enable verbose logging
```

## Usage Examples

### Example 1: Secure Production Configuration

```cpp
#include "source/core/nvigi.api/nvigi_cloud.h"

// Production-grade secure configuration
nvigi::RESTParameters params;
params.url = "https://api.example.com/v1/resource";
params.authenticationToken = getSecureToken();  // From secure storage

// Use all secure defaults (recommended)
params.verifySSLPeer = true;              // Verify SSL certificates
params.verifySSLHost = true;              // Verify hostname
params.timeoutSeconds = 60;               // 1 minute timeout
params.connectionTimeoutSeconds = 10;     // 10 second connection timeout
params.maxResponseSizeBytes = 5242880;    // 5 MB max response
params.allowHTTP = false;                 // HTTPS only
params.followRedirects = false;           // No redirects
params.redactAuthInLogs = true;           // Redact credentials

// Make request
Result result = inet->nvcfPost(params, response);
```

### Example 2: Internal Service (Relaxed Security)

```cpp
// Internal service on localhost (relaxed SSL, HTTP allowed)
nvigi::RESTParameters params;
params.url = "http://localhost:8080/internal/api";

// Relaxed settings for local development
params.allowHTTP = true;                  // Allow HTTP for localhost
params.verifySSLPeer = false;             // Self-signed cert
params.verifySSLHost = false;             // Self-signed cert
params.timeoutSeconds = 300;              // 5 minute timeout for slow ops
params.maxResponseSizeBytes = 0;          // Unlimited (trusted service)

Result result = inet->nvcfGet(params, response);
```

### Example 3: Large File Download with Streaming

```cpp
// Stream large file download
size_t streamCallback(const char* data, size_t size, void* userdata) {
    // Process streaming data chunk
    processChunk(data, size);
    return size;
}

nvigi::RESTParameters params;
params.url = "https://cdn.example.com/large-file.bin";
params.useStreaming = true;
params.maxStreamingSizeBytes = 1073741824;  // 1 GB limit
params.timeoutSeconds = 3600;               // 1 hour for large download
params.lowSpeedLimit = 10240;               // 10 KB/s minimum

Result result = inet->nvcfPostStreaming(params, streamCallback, userData);
```

### Example 4: Corporate Network with Custom CA

```cpp
// Corporate network with internal CA
nvigi::RESTParameters params;
params.url = "https://internal.corp.example.com/api";
params.sslCABundlePath = "C:\\certs\\corporate-ca-bundle.crt";
params.verifySSLPeer = true;
params.verifySSLHost = true;

Result result = inet->nvcfGet(params, response);
```

## Common Security Pitfalls

### ❌ DON'T: Disable SSL verification in production

```cpp
// NEVER do this in production!
params.verifySSLPeer = false;
params.verifySSLHost = false;
```

### ❌ DON'T: Use unlimited response sizes for untrusted sources

```cpp
// Risky - can cause OOM crash
params.maxResponseSizeBytes = 0;  // Unlimited
```

### ❌ DON'T: Allow HTTP for sensitive data

```cpp
// NEVER transmit sensitive data over HTTP!
params.allowHTTP = true;
params.url = "http://api.example.com/user/password";  // CRITICAL VULNERABILITY
```

### ❌ DON'T: Log credentials in production

```cpp
// NEVER do this in production!
params.redactAuthInLogs = false;
params.verboseMode = true;
```

### ✅ DO: Use secure defaults

```cpp
// Let secure defaults protect you
nvigi::RESTParameters params;  // All secure defaults applied
params.url = "https://api.example.com";
params.authenticationToken = token;
// Done! Secure by default.
```

### ✅ DO: Set appropriate limits for your use case

```cpp
// Tailor limits to your specific needs
params.maxResponseSizeBytes = 1048576;      // 1 MB for typical API
params.timeoutSeconds = 30;                  // 30s for fast API
params.lowSpeedLimit = 5120;                // 5 KB/s minimum
```

## Error Handling

The plugin returns specific error codes for security-related failures:

- `kResultNetCurlError`: CURL operation failed (check logs for details)
- `kResultNetServerError`: Server returned error or non-JSON response
- `kResultNetTimeout`: Request timed out
- `kResultNetResponseTooLarge`: Response exceeded size limit
- `kResultNetFailedToInitializeCurl`: CURL initialization failed

Always check return values and handle errors appropriately:

```cpp
Result result = inet->nvcfGet(params, response);
if (result != kResultOk) {
    switch (result) {
        case kResultNetResponseTooLarge:
            // Response was too large - increase limit or investigate
            NVIGI_LOG_ERROR("Response exceeded size limit");
            break;
        case kResultNetTimeout:
            // Request timed out - increase timeout or check network
            NVIGI_LOG_ERROR("Request timed out");
            break;
        case kResultNetCurlError:
            // CURL error - check logs for specific error
            NVIGI_LOG_ERROR("CURL request failed");
            break;
        default:
            NVIGI_LOG_ERROR("Request failed with error %d", result);
    }
}
```

## Security Checklist for Host Applications

Before deploying to production, ensure:

- All URLs are validated and sanitized
- HTTPS is used for all sensitive data transmission
- SSL certificate verification is enabled (`verifySSLPeer=true`, `verifySSLHost=true`)
- HTTP is disabled unless explicitly needed (`allowHTTP=false`)
- Appropriate timeout limits are set based on expected operation duration
- Response size limits are set based on expected response sizes
- Redirects are disabled unless expected (`followRedirects=false`)
- Authentication tokens are stored and transmitted securely
- Credential redaction is enabled in logs (`redactAuthInLogs=true`)
- Rate limiting is implemented at the application level
- Plugin binary digital signature is verified before loading
- Error handling is implemented for all network operations
- Logging does not expose sensitive data
- Regular security audits are performed
- Dependencies (CURL, OpenSSL) are kept up-to-date



---

**Last Updated**: January 2026
**Plugin Version**: See `versions.h`