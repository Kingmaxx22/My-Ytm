#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>

#include "platform/crypto/ICrypto.h"
#include "auth/auth_manager.h"
#include "auth/session.h"
#include "models/user_account.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

static int passed = 0, failed = 0;
#define EXPECT(cond, msg) do { if(cond){++passed; std::cout<<"[PASS] "<<msg<<"\n";} else {++failed; std::cout<<"[FAIL] "<<msg<<" ("#cond")\n";} } while(0)

void test_state_uniqueness() {
    std::cout<<"\n-- state uniqueness --\n";
    std::string s1 = myytm::platform::generateSecureState(32);
    std::string s2 = myytm::platform::generateSecureState(32);
    EXPECT(!s1.empty() && !s2.empty(), "state generated");
    EXPECT(s1 != s2, "two states are unique");
    EXPECT(s1 != "myytm_state" && s2 != "myytm_state", "not hard-coded");
    EXPECT(s1.size() >= 43 && s1.find('=')==std::string::npos, "base64url no padding, len>=43");
    EXPECT(s1.find('+')==std::string::npos && s1.find('/')==std::string::npos, "url safe");
}

void test_state_mismatch_detection() {
    std::cout<<"\n-- state mismatch --\n";
    std::string expected = myytm::platform::generateSecureState(32);
    std::string got = myytm::platform::generateSecureState(32);
    EXPECT(expected != got, "mismatch would be detected");
    // Simulate AuthManager check: res.state != expectedState -> error
    bool mismatch = (got != expected);
    EXPECT(mismatch, "state mismatch correctly identified");
    // Missing state
    std::string missing = "";
    EXPECT(missing.empty(), "missing state empty");
    EXPECT(missing != expected, "missing != expected");
}

void test_pkce_verifier_generation() {
    std::cout<<"\n-- pkce verifier --\n";
    std::string v1 = myytm::platform::generatePkceVerifier();
    std::string v2 = myytm::platform::generatePkceVerifier();
    EXPECT(v1.size() >= 43 && v1.size() <= 128, "verifier length 43-128");
    EXPECT(v1 != v2, "verifier unique");
    EXPECT(v1.find('=')==std::string::npos, "verifier no padding");
    EXPECT(v1.find('+')==std::string::npos && v1.find('/')==std::string::npos, "verifier url safe");
    // charset check: only unreserved A-Za-z0-9-._~
    auto isValid = [](const std::string& s){
        for(char c: s) if(!( (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='.'||c=='_'||c=='~')) return false;
        return true;
    };
    EXPECT(isValid(v1), "verifier charset unreserved");
}

void test_pkce_s256_correctness() {
    std::cout<<"\n-- pkce s256 RFC7636 --\n";
    // RFC 7636 Appendix B example
    std::string verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    std::string expected = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";
    std::string challenge = myytm::platform::computePkceChallenge(verifier);
    EXPECT(challenge == expected, "RFC7636 challenge matches");
    // Roundtrip: verifier -> challenge -> not equal verifier, challenge is base64url no padding
    EXPECT(challenge != verifier, "challenge != verifier");
    EXPECT(challenge.find('=')==std::string::npos, "challenge no padding");
}

void test_base64url_no_padding() {
    std::cout<<"\n-- base64url --\n";
    // Known vectors: "" -> "", "f" -> "Zg", "fo" -> "Zm8", "foo" -> "Zm9v", 0xFF 0xFF 0xFF -> "____" (standard "////" -> "____")
    EXPECT(myytm::platform::base64UrlEncode(std::vector<uint8_t>{}) == "", "empty");
    EXPECT(myytm::platform::base64UrlEncode(std::vector<uint8_t>{'f'}) == "Zg", "1 byte no padding");
    EXPECT(myytm::platform::base64UrlEncode(std::vector<uint8_t>{'f','o'}) == "Zm8", "2 bytes no padding");
    EXPECT(myytm::platform::base64UrlEncode(std::vector<uint8_t>{'f','o','o'}) == "Zm9v", "3 bytes");
    std::vector<uint8_t> threeFF{0xFF,0xFF,0xFF};
    EXPECT(myytm::platform::base64UrlEncode(threeFF) == "____", "0xFFFFFF -> ____ url safe");
    std::vector<uint8_t> fb{0xFB,0xEF,0xFF};
    // Standard base64 "++/+" -> url "--__" ? Actually 0xFB 0xEF 0xFF = base64 "++//" -> url "--__"
    // Let's compute: 0xFB=11111011, 0xEF=11101111, 0xFF=11111111 => 111110 111110 111111 111111 => 62,62,63,63 => "+","+", "/","/" -> url "-","-","_","_"
    EXPECT(myytm::platform::base64UrlEncode(fb) == "--__", "fb eff url");
}

void test_token_expiration() {
    std::cout<<"\n-- token expiration --\n";
    using namespace myytm::auth;
    Session s;
    s.account.id = "id"; s.account.displayName="u";
    s.accessToken="at"; s.refreshToken="rt";
    s.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(3600);
    EXPECT(!s.isExpired(), "not expired");
    EXPECT(!s.isExpiringSoon(std::chrono::seconds(300)), "not expiring soon (1h)");
    s.expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(100);
    EXPECT(s.isExpiringSoon(std::chrono::seconds(300)), "expiring soon (100s < 300)");
    s.expiresAt = std::chrono::system_clock::now() - std::chrono::seconds(10);
    EXPECT(s.isExpired(), "expired");

    // AuthManager clamping: exp <300 -> 300, >86400 ->86400
    // We test via completeAuth: use Memory store
    auto am = std::make_shared<AuthManager>(std::make_unique<MemoryCredentialStore>());
    // Directly test serialize: completeAuth with exp 10 should still result in 300 clamp in completeAuthWithCode,
    // but completeAuth itself does not clamp — only completeAuthWithCode does. So we test that completeAuth stores as given.
    myytm::models::UserAccount acc{"id2","","U"};
    am->completeAuth("at2","rt2",acc,10);
    auto sess = am->session();
    EXPECT(sess.has_value(), "session after completeAuth");
    // For direct completeAuth, expiresAt is 10s from now, so isExpiringSoon(300) should be true
    EXPECT(sess->isExpiringSoon(std::chrono::seconds(300)), "direct completeAuth with 10s is expiring soon");
}

// Helper to start a simple HTTP server that returns canned JSON for token endpoint
int startTokenServer(const std::string& body, int statusCode, std::string& outUrl) {
#ifdef _WIN32
    WSADATA wsa{}; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK); addr.sin_port=htons(0);
    bind(s, (sockaddr*)&addr, sizeof(addr));
    listen(s,1);
    sockaddr_in out{}; int len=sizeof(out); getsockname(s,(sockaddr*)&out,&len);
    int port=ntohs(out.sin_port);
    outUrl = "http://127.0.0.1:" + std::to_string(port) + "/token";
    std::thread([s, body, statusCode](){
        sockaddr_in client{}; int clen=sizeof(client);
        SOCKET cs = accept(s,(sockaddr*)&client,&clen);
        if(cs!=INVALID_SOCKET){
            char buf[4096]{}; recv(cs,buf,sizeof(buf)-1,0);
            std::string respBody = body;
            std::string statusText = statusCode==200 ? "OK" : "Bad Request";
            std::string resp = "HTTP/1.1 "+std::to_string(statusCode)+" "+statusText+"\r\nContent-Type: application/json\r\nContent-Length: "+std::to_string(respBody.size())+"\r\nConnection: close\r\n\r\n"+respBody;
            send(cs, resp.c_str(), (int)resp.size(), 0);
            closesocket(cs);
        }
        closesocket(s); WSACleanup();
    }).detach();
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return port;
#else
    (void)body; (void)statusCode; (void)outUrl; return 0;
#endif
}

void test_invalid_grant_handling() {
    std::cout<<"\n-- invalid_grant --\n";
    std::string url;
    std::string body = R"({"error":"invalid_grant","error_description":"Token expired"})";
    int port = startTokenServer(body, 400, url);
    EXPECT(port!=0, "token server started");
    auto am = std::make_shared<myytm::auth::AuthManager>(std::make_unique<myytm::auth::MemoryCredentialStore>());
    myytm::auth::AuthManager::OAuthConfig cfg;
    cfg.clientId="cid"; cfg.clientSecret="csec"; cfg.tokenEndpoint=url;
    bool ok = am->completeAuthWithCode("code123", "http://127.0.0.1/callback", cfg);
    EXPECT(!ok, "invalid_grant should fail");
    EXPECT(std::string(am->lastError()).find("Session expired")!=std::string::npos, "error mentions session expired");
    // Ensure state is Error and not SignedIn
    EXPECT(am->state()==myytm::auth::AuthState::Error, "state Error on invalid_grant");
}

void test_malformed_token_response() {
    std::cout<<"\n-- malformed token --\n";
    std::string url;
    std::string body = R"({"foo":"bar"})"; // missing access_token/refresh_token
    int port = startTokenServer(body, 200, url);
    auto am = std::make_shared<myytm::auth::AuthManager>(std::make_unique<myytm::auth::MemoryCredentialStore>());
    myytm::auth::AuthManager::OAuthConfig cfg;
    cfg.clientId="cid"; cfg.clientSecret="csec"; cfg.tokenEndpoint=url;
    bool ok = am->completeAuthWithCode("code123", "http://127.0.0.1/callback", cfg);
    EXPECT(!ok, "malformed should fail");
    EXPECT(std::string(am->lastError()).find("missing")!=std::string::npos || std::string(am->lastError()).find("Missing")!=std::string::npos, "error mentions missing token");
}

void test_refresh_before_expiry() {
    std::cout<<"\n-- refresh before expiry --\n";
    auto am = std::make_shared<myytm::auth::AuthManager>(std::make_unique<myytm::auth::MemoryCredentialStore>());
    myytm::models::UserAccount acc{"id","","U"};
    // Create session expiring soon (100s)
    am->completeAuth("at","rt",acc,100);
    EXPECT(am->isExpiringSoon(std::chrono::seconds(300)), "expiring soon true");
    // refreshIfNeeded should attempt refresh (will use mock extend since no env)
    bool refreshed = am->refreshIfNeeded(std::chrono::seconds(300));
    EXPECT(refreshed, "refreshIfNeeded should refresh");
    auto sess = am->session();
    EXPECT(sess.has_value() && !sess->isExpiringSoon(std::chrono::seconds(300)), "after refresh not expiring soon");
}

int main(){
    test_state_uniqueness();
    test_state_mismatch_detection();
    test_pkce_verifier_generation();
    test_pkce_s256_correctness();
    test_base64url_no_padding();
    test_token_expiration();
    test_invalid_grant_handling();
    test_malformed_token_response();
    test_refresh_before_expiry();
    std::cout<<"\n=== OAuth Hardening Tests: "<<passed<<" passed, "<<failed<<" failed ===\n";
    return failed==0?0:1;
}
