#include "youtube/http_client.h"

namespace myytm::youtube {

HttpResponse MockHttpClient::execute(const HttpRequest& req)
{
    if (cannedResponse.has_value()) {
        return *cannedResponse;
    }

    if (!cannedBody.empty()) {
        return HttpResponse{200, cannedBody, {}, ""};
    }

    // Default demo payload — treated as untrusted input by parser.
    // Minimal schema: { "results": [ { "type":"song"|"artist"|"album"|"playlist", "id":"", "title":"", "subtitle":"" } ] }
    std::string demo = R"json({
        "results": [
            {"type":"song","id":"s1","title":"Blinding Lights","subtitle":"The Weeknd"},
            {"type":"song","id":"s2","title":"Bohemian Rhapsody","subtitle":"Queen"},
            {"type":"artist","id":"a1","title":"The Weeknd","subtitle":""},
            {"type":"album","id":"al1","title":"After Hours","subtitle":"The Weeknd"},
            {"type":"playlist","id":"p1","title":"Liked Songs","subtitle":"You"}
        ]
    })json";

    // If query present in URL, echo it in debug header for diagnostics (no secrets)
    (void)req;
    return HttpResponse{200, demo, {}, ""};
}

HttpResponse WinHttpClient::execute(const HttpRequest&)
{
    // Phase 4 stub: real WinHTTP wiring lives here (WinHttpOpen/Connect/Send/Receive)
    // isolated per AGENTS.md Platform/Network. Returning explicit error so callers handle it.
    return HttpResponse{0, "", {}, "WinHttpClient not yet configured — using MockHttpClient for search"};
}

} // namespace myytm::youtube
