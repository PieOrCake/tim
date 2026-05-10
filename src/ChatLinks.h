#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <imgui.h>

namespace TyrianIM {

// GW2 chat link type codes (see wiki.guildwars2.com/wiki/Chat_link_format)
enum class GW2LinkType : uint8_t {
    Item,           // wire byte 0x02 — item with optional quantity/upgrades
    MapLink,        // wire byte 0x04 — waypoint / POI / vista
    Skill,          // wire byte 0x06
    Trait,          // wire byte 0x07
    Recipe,         // wire byte 0x09
    Skin,           // wire byte 0x0A
    Outfit,         // wire byte 0x0B
    BuildTemplate,  // wire byte 0x0F — native GW2 build template
    AE2Build,       // AE2:... Alter Ego build code
    Url,            // http:// or https:// web link
    Unknown
};

enum class LinkState : uint8_t {
    Pending,   // queued for API fetch
    Resolved,  // name/details known
    Failed     // API error; generic label shown permanently
};

struct LinkSegment {
    GW2LinkType  type          = GW2LinkType::Unknown;
    std::string  raw;            // original text: "[&BASE64]", "\[&BASE64\]", or "AE2:..."
    uint32_t     primary_id    = 0;
    std::string  display;        // generic "[Item]" initially, replaced on resolve
    std::string  tooltip_text;   // pre-formatted multiline string
    LinkState    state         = LinkState::Pending;
    ImU32        colour        = IM_COL32(180, 180, 180, 255);
    // AE2-specific fields
    std::string  ae2_profession;
    uint8_t      ae2_game_mode = 0;
    std::string  ae2_chat_link; // reconstructed [&...] build link
};

struct Segment {
    bool        is_link = false;
    std::string text;  // plain text when !is_link; unused when is_link (use link.display)
    LinkSegment link;  // valid when is_link
};

// Parse message text into segments. Returns empty vector if no links are found.
std::vector<Segment> ParseSegments(const std::string& text);

// Base64 encode/decode (standard alphabet, GW2-compatible)
std::vector<uint8_t> Base64Decode(const std::string& b64);
std::string          Base64Encode(const uint8_t* data, size_t len);

// GW2 profession helpers
const char* ProfessionName(uint8_t prof_byte);  // 1-9 → name, nullptr if unknown
ImU32       ProfessionColour(const char* name);
ImU32       RarityColour(const std::string& rarity);

} // namespace TyrianIM
