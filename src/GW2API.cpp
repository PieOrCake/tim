#include "GW2API.h"
#include "ChatManager.h"
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <atomic>
#include <string>

namespace TyrianIM {

// ---------------------------------------------------------------------------
// Resolved data cache key: (type << 32) | id
// ---------------------------------------------------------------------------

struct CachedResult {
    std::string display;
    std::string tooltip;
    ImU32       colour;
    bool        ok; // false = failed, show generic label permanently
};

struct ApiRequest {
    GW2LinkType  type;
    uint32_t     id;
    std::string  contact;
    size_t       msg_idx;
    size_t       seg_idx;
};

static HINTERNET           s_Session  = nullptr;
static HINTERNET           s_Connect  = nullptr;
static std::thread         s_Worker;
static std::mutex          s_Mutex;
static std::condition_variable s_CV;
static std::queue<ApiRequest>  s_Queue;
static std::atomic<bool>       s_Shutdown{false};
static std::unordered_map<uint64_t, CachedResult> s_Cache;
static std::unordered_set<uint64_t>               s_Pending; // type+id keys in flight or done

// POI lookup table built lazily from /v2/continents/{c}/floors/{f}
// poi_id → {poi_name, map_name}
static std::unordered_map<uint32_t, std::pair<std::string,std::string>> s_PoiLookup;
static bool s_FloorLoaded[2] = {false, false}; // [0]=continent1/floor1, [1]=continent2/floor1

static uint64_t CacheKey(GW2LinkType t, uint32_t id) {
    return (static_cast<uint64_t>(t) << 32) | id;
}

// ---------------------------------------------------------------------------
// HTTP helpers
// ---------------------------------------------------------------------------

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), sz, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), sz);
    return w;
}

static std::string HttpGet(const std::wstring& path) {
    if (!s_Connect) return {};
    HINTERNET req = WinHttpOpenRequest(
        s_Connect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!req) return {};

    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req, nullptr)) {
        WinHttpCloseHandle(req);
        return {};
    }

    // Check HTTP status
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(req,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    std::string body;
    if (statusCode == 200) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(req, &avail) && avail > 0) {
            std::string chunk(avail, '\0');
            DWORD read = 0;
            WinHttpReadData(req, chunk.data(), avail, &read);
            body.append(chunk.data(), read);
        }
    }

    WinHttpCloseHandle(req);
    return body;
}

// ---------------------------------------------------------------------------
// POI floor loader + MapLink resolver
// ---------------------------------------------------------------------------

// Called from worker thread (no lock held). Fetches one continent/floor JSON
// and populates s_PoiLookup with all waypoints/POIs found within it.
static void LoadFloor(int continent, int floor) {
    std::wstring path = L"/v2/continents/" + std::to_wstring(continent)
                      + L"/floors/" + std::to_wstring(floor);
    std::string json_str = HttpGet(path);
    if (json_str.empty()) return;

    try {
        auto j = nlohmann::json::parse(json_str);
        if (!j.contains("regions")) return;

        std::unordered_map<uint32_t, std::pair<std::string,std::string>> batch;

        for (auto& [rid, region] : j["regions"].items()) {
            if (!region.contains("maps")) continue;
            for (auto& [mid, map] : region["maps"].items()) {
                std::string map_name = map.value("name", "");
                if (!map.contains("points_of_interest")) continue;
                for (auto& [pid, poi] : map["points_of_interest"].items()) {
                    uint32_t poi_id  = poi.value("id", 0u);
                    std::string name = poi.value("name", "");
                    if (poi_id && !name.empty())
                        batch[poi_id] = {name, map_name};
                }
            }
        }

        std::lock_guard<std::mutex> lk(s_Mutex);
        for (auto& [id, entry] : batch)
            s_PoiLookup.emplace(id, std::move(entry));
    } catch (...) {}
}

static CachedResult BuildPoiResult(const std::string& name, const std::string& map_name) {
    CachedResult r{};
    r.ok      = true;
    r.colour  = IM_COL32(100, 210, 255, 255);
    r.display = "[" + name + "]";
    r.tooltip = name;
    if (!map_name.empty()) r.tooltip += "\n" + map_name;
    r.tooltip += "\nClick to copy \xe2\x80\x94 paste in chat to navigate";
    return r;
}

// Only called from the single worker thread, so s_FloorLoaded needs no mutex.
static CachedResult FetchMapLinkResult(uint32_t poi_id) {
    CachedResult fail{};
    fail.ok     = false;
    fail.colour = IM_COL32(100, 210, 255, 255);

    // Load continent 1 / floor 1 (main Tyria) if not yet done
    if (!s_FloorLoaded[0]) {
        s_FloorLoaded[0] = true;
        LoadFloor(1, 1);
    }

    {
        std::lock_guard<std::mutex> lk(s_Mutex);
        auto it = s_PoiLookup.find(poi_id);
        if (it != s_PoiLookup.end())
            return BuildPoiResult(it->second.first, it->second.second);
    }

    // Not found in Tyria — try continent 2 / floor 1 (Mists / WvW)
    if (!s_FloorLoaded[1]) {
        s_FloorLoaded[1] = true;
        LoadFloor(2, 1);
    }

    std::lock_guard<std::mutex> lk(s_Mutex);
    auto it = s_PoiLookup.find(poi_id);
    if (it != s_PoiLookup.end())
        return BuildPoiResult(it->second.first, it->second.second);

    return fail;
}

// ---------------------------------------------------------------------------
// JSON tooltip builders
// ---------------------------------------------------------------------------

static std::string StripGW2Markup(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool in_tag = false;
    for (char c : s) {
        if (c == '<')       { in_tag = true;  continue; }
        if (c == '>')       { in_tag = false; continue; }
        if (!in_tag) out += c;
    }
    return out;
}

static const char* HumanizeAttribute(const std::string& attr) {
    if (attr == "CritDamage")        return "Ferocity";
    if (attr == "Healing")           return "Healing Power";
    if (attr == "ConditionDamage")   return "Condition Damage";
    if (attr == "BoonDuration")      return "Concentration";
    if (attr == "ConditionDuration") return "Expertise";
    if (attr == "AgonyResistance")   return "Agony Resistance";
    return attr.c_str();
}

static std::string HumanizeSlot(const std::string& outer_type, const std::string& det_type) {
    // Armor slots
    if (det_type == "Helm")         return "Head Armor";
    if (det_type == "HelmAquatic")  return "Aquatic Helm";
    if (det_type == "Shoulders")    return "Shoulder Armor";
    if (det_type == "Coat")         return "Chest Armor";
    if (det_type == "Gloves")       return "Hand Armor";
    if (det_type == "Leggings")     return "Leg Armor";
    if (det_type == "Boots")        return "Foot Armor";
    // Weapon slots
    if (det_type == "HarpoonGun")   return "Harpoon Gun";
    if (det_type == "Shortbow")     return "Short Bow";
    if (!det_type.empty())          return det_type; // all other weapon/trinket subtypes
    // Outer type fallbacks
    if (outer_type == "CraftingMaterial") return "Crafting Material";
    if (outer_type == "MiniPet")          return "Miniature";
    if (outer_type == "UpgradeComponent") return "Upgrade Component";
    return outer_type;
}

static CachedResult BuildItemResult(const std::string& json_str, uint32_t item_id) {
    CachedResult r{};
    r.ok = false;
    try {
        auto j = nlohmann::json::parse(json_str);
        std::string name   = j.value("name", "");
        std::string rarity = j.value("rarity", "");
        std::string type   = j.value("type", "");
        int         level  = j.value("level", 0);
        std::string icon   = j.value("icon", "");
        std::string desc   = StripGW2Markup(j.value("description", ""));

        if (name.empty()) return r;

        r.colour  = RarityColour(rarity);
        r.display = name;

        std::ostringstream tt;

        // Icon reference line (first line, special prefix \x03)
        if (!icon.empty()) {
            // URL format: https://render.guildwars2.com/file/.../id.png
            // Split into remote + endpoint after the host
            const std::string host = "render.guildwars2.com";
            size_t host_pos = icon.find(host);
            if (host_pos != std::string::npos) {
                std::string endpoint = icon.substr(host_pos + host.size());
                tt << "\x03" << "TIM_ITEM_" << item_id
                   << "|" << host << "|" << endpoint << "\n";
            }
        }

        // Stats section
        std::string det_type, weight_class;
        if (j.contains("details")) {
            auto& det = j["details"];
            det_type     = det.value("type", "");
            weight_class = det.value("weight_class", "");

            if (det.contains("defense") && det["defense"].get<int>() > 0)
                tt << "Defense: " << det["defense"].get<int>() << "\n";
            if (det.contains("min_power") && det.contains("max_power"))
                tt << det["min_power"].get<int>() << " \xe2\x80\x93 "
                   << det["max_power"].get<int>() << " Damage\n";
            if (det.contains("infix_upgrade") &&
                det["infix_upgrade"].contains("attributes")) {
                for (auto& a : det["infix_upgrade"]["attributes"])
                    tt << "+" << a["modifier"].get<int>()
                       << " " << HumanizeAttribute(a["attribute"].get<std::string>()) << "\n";
            }
        }

        // Separator
        tt << "\n";

        // Metadata section (rarity in rarity color, then weight/slot/level)
        tt << "\x01" << rarity << "\n";
        if (!weight_class.empty()) tt << weight_class << "\n";
        std::string slot = HumanizeSlot(type, det_type);
        if (!slot.empty()) tt << slot << "\n";
        if (level > 0) tt << "Required Level: " << level << "\n";

        // Flavor text (gray)
        if (!desc.empty()) tt << "\x02" << desc << "\n";

        // Binding info
        if (j.contains("flags")) {
            for (auto& flag : j["flags"]) {
                std::string f = flag.get<std::string>();
                if (f == "AccountBound")      { tt << "Account Bound\n";        break; }
                if (f == "AccountBindOnUse")  { tt << "Account Bound on Use\n"; break; }
                if (f == "SoulbindOnUse")     { tt << "Soulbound on Use\n";     break; }
            }
        }

        r.tooltip = tt.str();
        r.ok = true;
    } catch (...) {}
    return r;
}

static CachedResult BuildSimpleResult(const std::string& json_str, const std::string& type_label) {
    CachedResult r{};
    r.ok = false;
    try {
        auto j = nlohmann::json::parse(json_str);
        std::string name = j.value("name", "");
        if (name.empty()) return r;
        r.colour  = IM_COL32(100, 180, 255, 255);
        r.display = name;
        r.tooltip = type_label + ": " + name;
        std::string desc = StripGW2Markup(j.value("description", ""));
        if (!desc.empty()) r.tooltip += "\n" + desc;
        r.ok = true;
    } catch (...) {}
    return r;
}

static CachedResult BuildSkinResult(const std::string& json_str) {
    CachedResult r{};
    r.ok = false;
    try {
        auto j = nlohmann::json::parse(json_str);
        std::string name   = j.value("name", "");
        std::string rarity = j.value("rarity", "Basic");
        if (name.empty()) return r;
        r.colour  = RarityColour(rarity);
        r.display = name;
        r.tooltip = rarity + " Skin: " + name;
        std::string desc = StripGW2Markup(j.value("description", ""));
        if (!desc.empty()) r.tooltip += "\n" + desc;
        r.ok = true;
    } catch (...) {}
    return r;
}

// ---------------------------------------------------------------------------
// Type → API path
// ---------------------------------------------------------------------------

static std::wstring ApiPath(GW2LinkType type, uint32_t id) {
    std::wstring base;
    switch (type) {
        case GW2LinkType::Item:    base = L"/v2/items/";   break;
        case GW2LinkType::Skill:   base = L"/v2/skills/";  break;
        case GW2LinkType::Trait:   base = L"/v2/traits/";  break;
        case GW2LinkType::Recipe:  base = L"/v2/recipes/"; break;
        case GW2LinkType::Skin:    base = L"/v2/skins/";   break;
        case GW2LinkType::Outfit:  base = L"/v2/outfits/"; break;
        default: return {};
    }
    return base + std::to_wstring(id);
}

static CachedResult ParseResponse(GW2LinkType type, uint32_t id, const std::string& json_str) {
    switch (type) {
        case GW2LinkType::Item:   return BuildItemResult(json_str, id);
        case GW2LinkType::Skill:  return BuildSimpleResult(json_str, "Skill");
        case GW2LinkType::Trait:  return BuildSimpleResult(json_str, "Trait");
        case GW2LinkType::Outfit: return BuildSimpleResult(json_str, "Outfit");
        case GW2LinkType::Skin:   return BuildSkinResult(json_str);
        case GW2LinkType::Recipe: {
            CachedResult r{};
            r.ok = false;
            try {
                auto j = nlohmann::json::parse(json_str);
                uint32_t out_id = j.value("output_item_id", 0);
                int count       = j.value("output_item_count", 1);
                r.colour  = IM_COL32(180, 220, 180, 255);
                r.display = "Recipe";
                r.tooltip = "Recipe";
                if (out_id) r.tooltip += " (output item #" + std::to_string(out_id) + ")";
                if (count > 1) r.tooltip += " x" + std::to_string(count);
                r.ok = true;
            } catch (...) {}
            return r;
        }
        default: return {};
    }
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

static void WorkerThread() {
    while (!s_Shutdown) {
        ApiRequest req;
        {
            std::unique_lock<std::mutex> lock(s_Mutex);
            s_CV.wait(lock, [] { return s_Shutdown || !s_Queue.empty(); });
            if (s_Shutdown && s_Queue.empty()) break;
            req = std::move(s_Queue.front());
            s_Queue.pop();
        }

        uint64_t key = CacheKey(req.type, req.id);

        CachedResult result;
        bool have_cached = false;
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            auto it = s_Cache.find(key);
            if (it != s_Cache.end()) { result = it->second; have_cached = true; }
        }

        if (!have_cached) {
            if (req.type == GW2LinkType::MapLink) {
                result = FetchMapLinkResult(req.id);
            } else {
                auto path = ApiPath(req.type, req.id);
                std::string json_str = path.empty() ? "" : HttpGet(path);
                result = ParseResponse(req.type, req.id, json_str);
            }
            if (!result.ok) {
                result.display = {};
                result.tooltip = "Could not load details";
                result.colour  = IM_COL32(150, 150, 150, 255);
            }
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_Cache[key] = result;
        }

        // Write result into the ChatMessage segment
        LinkState newState = result.ok ? LinkState::Resolved : LinkState::Failed;
        std::string display = result.ok ? result.display : std::string();
        ChatManager::UpdateLinkSegment(req.contact, req.msg_idx, req.seg_idx,
                                        display, result.tooltip, result.colour, newState);
        TIM_NotifyLinkResolved();
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void GW2API::Initialize() {
    s_Shutdown = false;
    s_Session = WinHttpOpen(
        L"TyrianIM/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (s_Session) {
        s_Connect = WinHttpConnect(s_Session, L"api.guildwars2.com",
                                   INTERNET_DEFAULT_HTTPS_PORT, 0);
    }
    s_Worker = std::thread(WorkerThread);
}

void GW2API::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Shutdown = true;
    }
    s_CV.notify_all();
    if (s_Worker.joinable()) s_Worker.join();

    if (s_Connect) { WinHttpCloseHandle(s_Connect); s_Connect = nullptr; }
    if (s_Session) { WinHttpCloseHandle(s_Session); s_Session = nullptr; }

    std::lock_guard<std::mutex> lock(s_Mutex);
    while (!s_Queue.empty()) s_Queue.pop();
    s_Pending.clear();
    s_Cache.clear();
    s_PoiLookup.clear();
    s_FloorLoaded[0] = s_FloorLoaded[1] = false;
}

void GW2API::RequestLink(const std::string& contact, size_t msg_idx, size_t seg_idx,
                          const LinkSegment& link) {
    if (link.type == GW2LinkType::BuildTemplate ||
        link.type == GW2LinkType::AE2Build      ||
        link.type == GW2LinkType::Unknown) return;

    if (link.primary_id == 0) return;
    uint64_t key = CacheKey(link.type, link.primary_id);

    ApiRequest req;
    req.type    = link.type;
    req.id      = link.primary_id;
    req.contact = contact;
    req.msg_idx = msg_idx;
    req.seg_idx = seg_idx;

    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Pending.count(key)) return;
        s_Pending.insert(key);
        s_Queue.push(std::move(req));
    }
    s_CV.notify_one();
}

} // namespace TyrianIM
