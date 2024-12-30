// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#ifdef _WIN32

#define _UNICODE 1
#define UNICODE 1

#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <Softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <inttypes.h>

#define GetProc(hModule, procName, proc) (((NULL == proc) && (NULL == (*((FARPROC*)&proc) = GetProcAddress(hModule, procName)))) ? FALSE : TRUE)
namespace nvigi
{

namespace security
{

typedef BOOL(WINAPI* PfnCryptMsgClose)(IN HCRYPTMSG hCryptMsg);
static PfnCryptMsgClose pfnCryptMsgClose = NULL;

typedef BOOL(WINAPI* PfnCertCloseStore)(IN HCERTSTORE hCertStore, DWORD dwFlags);
static PfnCertCloseStore pfnCertCloseStore = NULL;

typedef HCERTSTORE (WINAPI* PfnCertOpenStore)(
    _In_ LPCSTR lpszStoreProvider,
    _In_ DWORD dwEncodingType,
    _In_opt_ HCRYPTPROV_LEGACY hCryptProv,
    _In_ DWORD dwFlags,
    _In_opt_ const void* pvPara
);
static PfnCertOpenStore pfnCertOpenStore = NULL;

typedef BOOL(WINAPI* PfnCertFreeCertificateContext)(IN PCCERT_CONTEXT pCertContext);
static PfnCertFreeCertificateContext pfnCertFreeCertificateContext = NULL;

typedef PCCERT_CONTEXT(WINAPI* PfnCertFindCertificateInStore)(
    IN HCERTSTORE hCertStore,
    IN DWORD dwCertEncodingType,
    IN DWORD dwFindFlags,
    IN DWORD dwFindType,
    IN const void* pvFindPara,
    IN PCCERT_CONTEXT pPrevCertContext
    );
static PfnCertFindCertificateInStore pfnCertFindCertificateInStore = NULL;

typedef BOOL(WINAPI* PfnCryptMsgGetParam)(
    IN HCRYPTMSG hCryptMsg,
    IN DWORD dwParamType,
    IN DWORD dwIndex,
    OUT void* pvData,
    IN OUT DWORD* pcbData
    );
static PfnCryptMsgGetParam pfnCryptMsgGetParam = NULL;

typedef HCRYPTMSG (WINAPI* PfnCryptMsgOpenToDecode)(
    _In_ DWORD dwMsgEncodingType,
    _In_ DWORD dwFlags,
    _In_ DWORD dwMsgType,
    _In_opt_ HCRYPTPROV_LEGACY hCryptProv,
    _Reserved_ PCERT_INFO pRecipientInfo,
    _In_opt_ PCMSG_STREAM_INFO pStreamInfo
);
PfnCryptMsgOpenToDecode pfnCryptMsgOpenToDecode = {};

typedef BOOL (WINAPI* PfnCryptMsgUpdate)(
    _In_ HCRYPTMSG hCryptMsg,
    _In_reads_bytes_opt_(cbData) const BYTE* pbData,
    _In_ DWORD cbData,
    _In_ BOOL fFinal
);
PfnCryptMsgUpdate pfnCryptMsgUpdate = {};

typedef BOOL(WINAPI* PfnCryptQueryObject)(
    DWORD            dwObjectType,
    const void* pvObject,
    DWORD            dwExpectedContentTypeFlags,
    DWORD            dwExpectedFormatTypeFlags,
    DWORD            dwFlags,
    DWORD* pdwMsgAndCertEncodingType,
    DWORD* pdwContentType,
    DWORD* pdwFormatType,
    HCERTSTORE* phCertStore,
    HCRYPTMSG* phMsg,
    const void** ppvContext
    );
static PfnCryptQueryObject pfnCryptQueryObject = NULL;

typedef BOOL(WINAPI* PfnCryptDecodeObjectEx)(
    IN DWORD              dwCertEncodingType,
    IN LPCSTR             lpszStructType,
    IN const BYTE* pbEncoded,
    IN DWORD              cbEncoded,
    IN DWORD              dwFlags,
    IN PCRYPT_DECODE_PARA pDecodePara,
    OUT void* pvStructInfo,
    IN OUT DWORD* pcbStructInfo
    );
static PfnCryptDecodeObjectEx pfnCryptDecodeObjectEx = NULL;

typedef LONG(WINAPI* PfnWinVerifyTrust)(
    IN HWND   hwnd,
    IN GUID* pgActionID,
    IN LPVOID pWVTData
    );
static PfnWinVerifyTrust pfnWinVerifyTrust = NULL;

typedef CRYPT_PROVIDER_DATA* (WINAPI* PfnWTHelperProvDataFromStateData)(IN HANDLE hd);
static PfnWTHelperProvDataFromStateData pfnWTHelperProvDataFromStateData = NULL;

typedef CRYPT_PROVIDER_SGNR* (WINAPI* PfnWTHelperGetProvSignerFromChain)(IN CRYPT_PROVIDER_DATA*, DWORD, BOOL, DWORD);
static PfnWTHelperGetProvSignerFromChain pfnWTHelperGetProvSignerFromChain = NULL;

typedef CRYPT_PROVIDER_CERT* (WINAPI* PfnWTHelperGetProvCertFromChain)(IN CRYPT_PROVIDER_SGNR*, DWORD);
static PfnWTHelperGetProvCertFromChain pfnWTHelperGetProvCertFromChain = NULL;

typedef DWORD(WINAPI* PfnCertGetNameStringA)(
    IN PCCERT_CONTEXT pCertContext,
    IN DWORD dwType,
    IN DWORD dwFlags,
    IN void* pvTypePara,
    OUT OPTIONAL LPSTR pszNameString,
    IN DWORD cchNameString
    );
static PfnCertGetNameStringA pfnCertGetNameStringA = NULL;

#define NV_NVIDIA_CERT_NAME "NVIDIA "

//! See https://docs.microsoft.com/en-us/windows/win32/seccrypto/example-c-program--verifying-the-signature-of-a-pe-file
//! 
//! IMPORTANT: Always pass in the FULL PATH to the file, relative paths are NOT allowed!
bool verifyEmbeddedSignature(const wchar_t* pathToFile)
{
    bool valid = true;

    LONG lStatus = {};

    // Initialize the WINTRUST_FILE_INFO structure.

    WINTRUST_FILE_INFO FileData;
    memset(&FileData, 0, sizeof(FileData));
    FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
    FileData.pcwszFilePath = pathToFile;
    FileData.hFile = NULL;
    FileData.pgKnownSubject = NULL;

    if (!pfnWinVerifyTrust)
    {
        // We only support Win10+ so we can search for module in system32 directly
        auto hModWintrust = LoadLibraryExW(L"wintrust.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!hModWintrust || !GetProc(hModWintrust, "WinVerifyTrust", pfnWinVerifyTrust)
            || !GetProc(hModWintrust, "WTHelperProvDataFromStateData", pfnWTHelperProvDataFromStateData)
            || !GetProc(hModWintrust, "WTHelperGetProvCertFromChain", pfnWTHelperGetProvCertFromChain)
            || !GetProc(hModWintrust, "WTHelperGetProvSignerFromChain", pfnWTHelperGetProvSignerFromChain))
        {
            return false;
        }
        auto hModCrypt32 = LoadLibraryExW(L"crypt32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!hModCrypt32 || !GetProc(hModCrypt32, "CertGetNameStringA", pfnCertGetNameStringA))
        {
            return false;
        }
    }



    /*
    WVTPolicyGUID specifies the policy to apply on the file
    WINTRUST_ACTION_GENERIC_VERIFY_V2 policy checks:

    1) The certificate used to sign the file chains up to a root
    certificate located in the trusted root certificate store. This
    implies that the identity of the publisher has been verified by
    a certification authority.

    2) In cases where user interface is displayed (which this example
    does not do), WinVerifyTrust will check for whether the
    end entity certificate is stored in the trusted publisher store,
    implying that the user trusts content from this publisher.

    3) The end entity certificate has sufficient permission to sign
    code, as indicated by the presence of a code signing EKU or no
    EKU.
    */

    GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA WinTrustData;

    // Initialize the WinVerifyTrust input data structure.

    // Default all fields to 0.
    memset(&WinTrustData, 0, sizeof(WinTrustData));

    WinTrustData.cbStruct = sizeof(WinTrustData);
    // Use default code signing EKU.
    WinTrustData.pPolicyCallbackData = NULL;
    // No data to pass to SIP.
    WinTrustData.pSIPClientData = NULL;
    // Disable WVT UI.
    WinTrustData.dwUIChoice = WTD_UI_NONE;
    // No revocation checking.
    WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    // Verify an embedded signature on a file.
    WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    // Verify action.
    WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    // Verification sets this value.
    WinTrustData.hWVTStateData = NULL;
    // Not used.
    WinTrustData.pwszURLReference = NULL;
    // This is not applicable if there is no UI because it changes 
    // the UI to accommodate running applications instead of 
    // installing applications.
    WinTrustData.dwUIContext = 0;
    // Set pFile.
    WinTrustData.pFile = &FileData;

    // First verify the primary signature (index 0) to determine how many secondary signatures
    // are present. We use WSS_VERIFY_SPECIFIC and dwIndex to do this, also setting 
    // WSS_GET_SECONDARY_SIG_COUNT to have the number of secondary signatures returned.
    WINTRUST_SIGNATURE_SETTINGS SignatureSettings = {};
    CERT_STRONG_SIGN_PARA StrongSigPolicy = {};
    SignatureSettings.cbStruct = sizeof(WINTRUST_SIGNATURE_SETTINGS);
    SignatureSettings.dwFlags = WSS_GET_SECONDARY_SIG_COUNT | WSS_VERIFY_SPECIFIC;
    SignatureSettings.dwIndex = 0;
    WinTrustData.pSignatureSettings = &SignatureSettings;

    StrongSigPolicy.cbSize = sizeof(CERT_STRONG_SIGN_PARA);
    StrongSigPolicy.dwInfoChoice = CERT_STRONG_SIGN_OID_INFO_CHOICE;
    StrongSigPolicy.pszOID = (LPSTR)szOID_CERT_STRONG_SIGN_OS_CURRENT;
    WinTrustData.pSignatureSettings->pCryptoPolicy = &StrongSigPolicy;

    // WinVerifyTrust verifies signatures as specified by the GUID  and Wintrust_Data.
    lStatus = pfnWinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);
    
    // First signature must be validated by the OS
    valid = lStatus == ERROR_SUCCESS;

    // Check for NVIDIA as the signer
    if(valid)
    {
        CRYPT_PROVIDER_DATA* pCryptProvData = pfnWTHelperProvDataFromStateData(WinTrustData.hWVTStateData);
        if (pCryptProvData == NULL)
        {
            valid = false;
            goto verifySingleSignatureDone;
        }

        CRYPT_PROVIDER_SGNR* pSigner = pfnWTHelperGetProvSignerFromChain(pCryptProvData, 0, FALSE, 0);
        if (pSigner == NULL)
        {
            valid = false;
            goto verifySingleSignatureDone;
        }

        CRYPT_PROVIDER_CERT* pProvCertInfo = pfnWTHelperGetProvCertFromChain(pSigner, 0);
        if (pProvCertInfo == NULL)
        {
            valid = false;
            goto verifySingleSignatureDone;
        }

        // The function will set szName to "" if the name is not found, but
        // static code analysis does not know that, so terminate manually.
        char szName[256];
        szName[0] = 0;
        pfnCertGetNameStringA(pProvCertInfo->pCert,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            0,
            NULL,
            szName,
            sizeof(szName));

        valid &= (_strnicmp(szName, NV_NVIDIA_CERT_NAME, strlen(NV_NVIDIA_CERT_NAME)) == 0);
    }

verifySingleSignatureDone:
    // Any hWVTStateData must be released by a call with close.
    WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    lStatus = pfnWinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

    return valid;
}

}
}

#endif