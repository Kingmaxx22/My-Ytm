#include "platform/http/IHttpClient.h"
#include "platform/win32_error.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#endif

namespace myytm::platform {

#ifdef _WIN32

static std::wstring toWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}
static std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
struct CrackedUrl {
    std::wstring scheme, host, path;
    INTERNET_PORT port = 0;
    bool secure = false;
};
static bool crackUrl(const std::string& urlStr, CrackedUrl& out, std::string& err) {
    std::wstring wurl = toWide(urlStr);
    URL_COMPONENTS comp{}; comp.dwStructSize = sizeof(comp);
    comp.dwSchemeLength = (DWORD)-1; comp.dwHostNameLength = (DWORD)-1;
    comp.dwUrlPathLength = (DWORD)-1; comp.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &comp)) { err = "Invalid URL: " + lastWin32Error(); return false; }
    out.scheme.assign(comp.lpszScheme, comp.dwSchemeLength);
    out.host.assign(comp.lpszHostName, comp.dwHostNameLength);
    std::wstring path(comp.lpszUrlPath, comp.dwUrlPathLength);
    std::wstring extra(comp.lpszExtraInfo, comp.dwExtraInfoLength);
    out.path = path + extra; if (out.path.empty()) out.path = L"/";
    out.port = comp.nPort; out.secure = (_wcsicmp(comp.lpszScheme, L"https")==0);
    return true;
}

HttpResponse WinHttpClient::execute(const HttpRequest& req) {
        HttpResponse resp;
        if (req.url.empty()) { resp.errorMessage = "Empty URL"; return resp; }
        CrackedUrl cu; std::string crackErr;
        if (!crackUrl(req.url, cu, crackErr)) { resp.errorMessage = crackErr; return resp; }
        HINTERNET hSession = WinHttpOpen(L"MyYtm/0.1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) { resp.errorMessage = "WinHttpOpen failed: " + lastWin32Error(); return resp; }
        int timeoutMs = (int)req.timeout.count(); if (timeoutMs<=0) timeoutMs=10000;
        WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
        HINTERNET hConnect = WinHttpConnect(hSession, cu.host.c_str(), cu.port, 0);
        if (!hConnect) { resp.errorMessage = "WinHttpConnect failed: " + lastWin32Error(); WinHttpCloseHandle(hSession); return resp; }
        DWORD flags = cu.secure ? WINHTTP_FLAG_SECURE : 0;
        std::wstring wmethod = toWide(req.method.empty() ? "GET" : req.method);
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), cu.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) { resp.errorMessage = "WinHttpOpenRequest failed: " + lastWin32Error(); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return resp; }
        std::wstring wheaders;
        for (auto& kv : req.headers) wheaders += toWide(kv.first + ": " + kv.second + "\r\n");
        if (req.headers.find("User-Agent")==req.headers.end()) wheaders += L"User-Agent: MyYtm/0.1.0\r\n";
        if (!wheaders.empty()) WinHttpAddRequestHeaders(hRequest, wheaders.c_str(), (DWORD)wheaders.size(), WINHTTP_ADDREQ_FLAG_ADD);
        std::string body = req.body; DWORD bodyLen=(DWORD)body.size();
        const void* bodyPtr = body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data();
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, const_cast<void*>(bodyPtr), bodyLen, bodyLen, 0)) {
            resp.errorMessage = "WinHttpSendRequest failed: " + lastWin32Error();
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return resp;
        }
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            resp.errorMessage = "WinHttpReceiveResponse failed: " + lastWin32Error();
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return resp;
        }
        DWORD status=0; DWORD statusLen=sizeof(status);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX)) resp.statusCode=(int)status;
        DWORD headerSize=0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &headerSize, WINHTTP_NO_HEADER_INDEX);
        if (GetLastError()==ERROR_INSUFFICIENT_BUFFER && headerSize>0) {
            std::vector<wchar_t> buf(headerSize/sizeof(wchar_t)+1);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, buf.data(), &headerSize, WINHTTP_NO_HEADER_INDEX)) {
                std::wstring wh(buf.data());
                size_t pos=0;
                while(pos<wh.size()){
                    size_t eol=wh.find(L"\r\n",pos); if(eol==std::wstring::npos) break;
                    std::wstring line=wh.substr(pos,eol-pos); pos=eol+2;
                    if(line.empty()) continue;
                    size_t colon=line.find(L':'); if(colon==std::wstring::npos) continue;
                    std::wstring k=line.substr(0,colon), v=line.substr(colon+1);
                    auto trimW=[](std::wstring& s){ size_t a=0; while(a<s.size()&&iswspace(s[a]))++a; size_t b=s.size(); while(b>a&&iswspace(s[b-1]))--b; s=s.substr(a,b-a); };
                    trimW(k); trimW(v);
                    resp.headers[toUtf8(k)]=toUtf8(v);
                }
            }
        }
        std::string out; DWORD avail=0;
        while(WinHttpQueryDataAvailable(hRequest,&avail) && avail>0){
            std::vector<char> chunk(avail); DWORD read=0;
            if(!WinHttpReadData(hRequest,chunk.data(),avail,&read)||read==0) break;
            out.append(chunk.data(),read);
        }
        resp.body=std::move(out);
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return resp;
}

#else
HttpResponse WinHttpClient::execute(const HttpRequest&) {
    HttpResponse r; r.errorMessage="WinHTTP not available on this platform"; return r;
}
#endif

} // namespace myytm::platform
