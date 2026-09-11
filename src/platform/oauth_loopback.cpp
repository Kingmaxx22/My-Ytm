#include "platform/oauth_loopback.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

namespace myytm::platform {

OAuthLoopbackServer::~OAuthLoopbackServer() { stop(); }

bool OAuthLoopbackServer::start() {
#ifdef _WIN32
    if (running_) return true;
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return false;
    wsaInit_ = true;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { WSACleanup(); wsaInit_=false; return false; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    addr.sin_port = 0; // ephemeral
    addr.sin_port = htons(0);

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s); WSACleanup(); wsaInit_=false; return false;
    }
    if (listen(s, 1) == SOCKET_ERROR) {
        closesocket(s); WSACleanup(); wsaInit_=false; return false;
    }
    // Retrieve assigned port
    sockaddr_in out{};
    int len = sizeof(out);
    if (getsockname(s, reinterpret_cast<sockaddr*>(&out), &len) == 0) {
        port_ = ntohs(out.sin_port);
    } else {
        port_ = 0;
    }
    listenSock_ = reinterpret_cast<void*>(s);
    running_ = true;
    return true;
#else
    return false;
#endif
}

void OAuthLoopbackServer::stop() {
#ifdef _WIN32
    if (listenSock_) {
        closesocket(reinterpret_cast<SOCKET>(listenSock_));
        listenSock_ = nullptr;
    }
    if (wsaInit_) { WSACleanup(); wsaInit_=false; }
    running_ = false;
    port_ = 0;
#endif
}

std::string OAuthLoopbackServer::redirectUri() const {
    if (!running_ || port_==0) return "";
    return "http://127.0.0.1:" + std::to_string(port_) + "/callback";
}

static std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i=0;i<s.size();++i) {
        if (s[i]=='%' && i+2<s.size()) {
            char h = s[i+1], l = s[i+2];
            auto hex = [](char c)->int {
                if (c>='0'&&c<='9') return c-'0';
                if (c>='a'&&c<='f') return c-'a'+10;
                if (c>='A'&&c<='F') return c-'A'+10;
                return 0;
            };
            out.push_back(static_cast<char>(hex(h)*16+hex(l)));
            i+=2;
        } else if (s[i]=='+') out.push_back(' ');
        else out.push_back(s[i]);
    }
    return out;
}

static std::string getQueryParam(const std::string& query, const std::string& key) {
    std::string pat = key + "=";
    size_t pos = 0;
    while (true) {
        pos = query.find(pat, pos);
        if (pos==std::string::npos) return "";
        // ensure start or &
        if (pos!=0 && query[pos-1]!='&' && query[pos-1]!='?') { ++pos; continue; }
        size_t start = pos + pat.size();
        size_t end = query.find('&', start);
        std::string v = query.substr(start, end==std::string::npos? std::string::npos : end-start);
        return urlDecode(v);
    }
}

OAuthLoopbackServer::Result OAuthLoopbackServer::waitForCode(std::chrono::milliseconds timeout) {
    Result res;
#ifdef _WIN32
    if (!running_ || !listenSock_) return res;
    SOCKET ls = reinterpret_cast<SOCKET>(listenSock_);

    // Use select with timeout
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(ls, &rfds);
        timeval tv{0, 200000}; // 200ms poll
        int sel = select(0, &rfds, nullptr, nullptr, &tv);
        if (sel > 0 && FD_ISSET(ls, &rfds)) {
            sockaddr_in client{};
            int clen = sizeof(client);
            SOCKET cs = accept(ls, reinterpret_cast<sockaddr*>(&client), &clen);
            if (cs == INVALID_SOCKET) continue;

            // Read request (up to 8KB)
            char buf[8192]{};
            int rec = recv(cs, buf, sizeof(buf)-1, 0);
            std::string req = rec>0 ? std::string(buf, rec) : "";

            // Parse first line: GET /callback?code=... HTTP/1.1
            std::string path;
            size_t sp1 = req.find(' ');
            size_t sp2 = req.find(' ', sp1+1);
            if (sp1!=std::string::npos && sp2!=std::string::npos) {
                path = req.substr(sp1+1, sp2-sp1-1);
            }

            std::string query;
            size_t qm = path.find('?');
            if (qm != std::string::npos) query = path.substr(qm+1);

            res.code = getQueryParam(query, "code");
            res.state = getQueryParam(query, "state");
            res.error = getQueryParam(query, "error");
            res.errorDescription = getQueryParam(query, "error_description");
            if (!res.error.empty()) {
                res.ok = false;
            } else if (!res.code.empty()) {
                res.ok = true;
            } else {
                // No code — treat as error but still respond
                res.ok = false;
                if (res.error.empty()) res.error = "missing_code";
            }

            // Send HTML response — never include code in logs
            std::string body;
            if (res.ok) body = "<html><body><h1>My-Ytm: Signed in</h1><p>You can close this window and return to the app.</p></body></html>";
            else body = "<html><body><h1>My-Ytm: Authentication failed</h1><p>" + (res.error.empty()? "Missing code" : res.error) + "</p><p>You can close this window.</p></body></html>";
            std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            send(cs, resp.c_str(), (int)resp.size(), 0);
            closesocket(cs);
            return res;
        }
        // timeout check continues
    }
#else
    (void)timeout;
#endif
    return res; // timeout -> ok=false
}

} // namespace myytm::platform
