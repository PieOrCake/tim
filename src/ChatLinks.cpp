#include "ChatLinks.h"
#include <cstring>
#include <algorithm>
#include <cctype>

namespace TyrianIM {

// ---------------------------------------------------------------------------
// Base64
// ---------------------------------------------------------------------------

static const char kB64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int B64Val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<uint8_t> Base64Decode(const std::string& b64) {
    std::vector<uint8_t> out;
    out.reserve(b64.size() * 3 / 4 + 1);
    int val = 0, bits = -8;
    for (char c : b64) {
        if (c == '=') break;
        int v = B64Val(c);
        if (v < 0) continue;
        val = (val << 6) | v;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

std::string Base64Encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t grp = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) grp |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) grp |= static_cast<uint32_t>(data[i + 2]);
        out += kB64Chars[(grp >> 18) & 63];
        out += kB64Chars[(grp >> 12) & 63];
        out += (i + 1 < len) ? kB64Chars[(grp >> 6) & 63] : '=';
        out += (i + 2 < len) ? kB64Chars[grp & 63] : '=';
    }
    return out;
}

// ---------------------------------------------------------------------------
// Profession helpers
// ---------------------------------------------------------------------------

const char* ProfessionName(uint8_t b) {
    switch (b) {
        case 1: return "Guardian";
        case 2: return "Warrior";
        case 3: return "Engineer";
        case 4: return "Ranger";
        case 5: return "Thief";
        case 6: return "Elementalist";
        case 7: return "Mesmer";
        case 8: return "Necromancer";
        case 9: return "Revenant";
        default: return nullptr;
    }
}

ImU32 ProfessionColour(const char* name) {
    if (!name) return IM_COL32(200, 200, 200, 255);
    if (strcmp(name, "Guardian")     == 0) return IM_COL32(114, 193, 217, 255);
    if (strcmp(name, "Warrior")      == 0) return IM_COL32(255, 209, 102, 255);
    if (strcmp(name, "Engineer")     == 0) return IM_COL32(208, 156,  89, 255);
    if (strcmp(name, "Ranger")       == 0) return IM_COL32(141, 180,  70, 255);
    if (strcmp(name, "Thief")        == 0) return IM_COL32(192, 143,  57, 255);
    if (strcmp(name, "Elementalist") == 0) return IM_COL32(246, 138, 135, 255);
    if (strcmp(name, "Mesmer")       == 0) return IM_COL32(182, 121, 213, 255);
    if (strcmp(name, "Necromancer")  == 0) return IM_COL32( 82, 167, 111, 255);
    if (strcmp(name, "Revenant")     == 0) return IM_COL32(209, 110,  90, 255);
    return IM_COL32(200, 200, 200, 255);
}

ImU32 RarityColour(const std::string& rarity) {
    if (rarity == "Junk")       return IM_COL32(170, 170, 170, 255);
    if (rarity == "Basic")      return IM_COL32(255, 255, 255, 255);
    if (rarity == "Fine")       return IM_COL32( 98, 164, 218, 255);
    if (rarity == "Masterwork") return IM_COL32( 30, 215,   0, 255);
    if (rarity == "Rare")       return IM_COL32(255, 215,   0, 255);
    if (rarity == "Exotic")     return IM_COL32(255, 170,   0, 255);
    if (rarity == "Ascended")   return IM_COL32(251,  62, 141, 255);
    if (rarity == "Legendary")  return IM_COL32(163,  53, 238, 255);
    return IM_COL32(220, 220, 220, 255);
}

// ---------------------------------------------------------------------------
// Link decoding
// ---------------------------------------------------------------------------

static uint32_t ReadU32LE(const uint8_t* p) {
    return  static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) <<  8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
}

static uint32_t ReadU24LE(const uint8_t* p) {
    return  static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) <<  8)
        | (static_cast<uint32_t>(p[2]) << 16);
}

static GW2LinkType TypeFromByte(uint8_t b) {
    switch (b) {
        case 0x02: return GW2LinkType::Item;
        case 0x04: return GW2LinkType::MapLink;
        case 0x06: return GW2LinkType::Skill;
        case 0x07: return GW2LinkType::Trait;
        case 0x09: return GW2LinkType::Recipe;
        case 0x0A: return GW2LinkType::Skin;
        case 0x0B: return GW2LinkType::Outfit;
        case 0x0F: return GW2LinkType::BuildTemplate;
        default:   return GW2LinkType::Unknown;
    }
}

static const char* GenericLabel(GW2LinkType t) {
    switch (t) {
        case GW2LinkType::Item:          return "[Item]";
        case GW2LinkType::MapLink:       return "[Waypoint]";
        case GW2LinkType::Skill:         return "[Skill]";
        case GW2LinkType::Trait:         return "[Trait]";
        case GW2LinkType::Recipe:        return "[Recipe]";
        case GW2LinkType::Skin:          return "[Skin]";
        case GW2LinkType::Outfit:        return "[Outfit]";
        case GW2LinkType::BuildTemplate: return "[Build]";
        default:                         return "[Link]";
    }
}

static ImU32 DefaultColour(GW2LinkType t) {
    switch (t) {
        case GW2LinkType::MapLink: return IM_COL32(100, 210, 255, 255);
        case GW2LinkType::Skill:   return IM_COL32(100, 180, 255, 255);
        case GW2LinkType::Trait:   return IM_COL32(100, 180, 255, 255);
        default:                   return IM_COL32(200, 200, 200, 255);
    }
}

// Decode a raw [&BASE64] or \[&BASE64\] string into a LinkSegment.
static LinkSegment DecodeGW2Link(const std::string& raw) {
    LinkSegment seg;
    seg.raw = raw;

    // Find the '&' after the opening bracket
    size_t amp = raw.find('&');
    if (amp == std::string::npos || amp + 1 >= raw.size()) {
        seg.display = "[Link]";
        return seg;
    }

    // Find closing bracket (may be escaped as \])
    size_t close = std::string::npos;
    for (size_t j = amp + 1; j < raw.size(); j++) {
        if (raw[j] == ']') { close = j; break; }
        if (raw[j] == '\\' && j + 1 < raw.size() && raw[j + 1] == ']') { close = j; break; }
    }
    if (close == std::string::npos) {
        seg.display = "[Link]";
        return seg;
    }

    std::string b64 = raw.substr(amp + 1, close - (amp + 1));
    auto bytes = Base64Decode(b64);
    if (bytes.empty()) {
        seg.display = "[Link]";
        return seg;
    }

    // Normalize to canonical [&BASE64] — strips backslash escaping so clipboard paste works in-game
    seg.raw = "[&" + b64 + "]";

    seg.type   = TypeFromByte(bytes[0]);
    seg.colour = DefaultColour(seg.type);

    // Item struct (after type byte): [0]=quantity [1..3]=ItemId(UInt24) [4..6]=SkinId [7..9]=Upgrade1Id [10..12]=Upgrade2Id [13]=Flags
    // All other link types: [0..3]=id(uint32)
    if (seg.type == GW2LinkType::Item) {
        if (bytes.size() >= 5)
            seg.primary_id = ReadU24LE(&bytes[2]);
    } else {
        if (bytes.size() >= 5)
            seg.primary_id = ReadU32LE(&bytes[1]);
    }

    seg.display = GenericLabel(seg.type);
    // MapLink stays Pending — GW2API will fetch the waypoint/POI name and map name.

    return seg;
}

// Decode an AE2:... build code.
static LinkSegment ParseAE2(const std::string& raw) {
    static const char* kGameModeNames[] =
        { "PvE", "WvW", "PvP", "Raid", "Fractal", "Strike", "?", "?" };

    LinkSegment seg;
    seg.raw   = raw;
    seg.type  = GW2LinkType::AE2Build;
    seg.display = "[Build]";
    seg.colour  = IM_COL32(200, 200, 200, 255);

    if (raw.size() < 5) return seg;
    auto data = Base64Decode(raw.substr(4)); // strip "AE2:"
    if (data.size() < 4) return seg;

    size_t pos = 0;
    uint8_t version = data[pos++];
    if (version != 2 && version != 3) return seg;

    uint8_t flags = data[pos++];
    seg.ae2_game_mode = flags & 0x07;

    uint8_t linkLen = data[pos++];
    if (pos + linkLen > data.size()) return seg;

    std::vector<uint8_t> linkBytes(data.begin() + pos, data.begin() + pos + linkLen);
    seg.ae2_chat_link = "[&" + Base64Encode(linkBytes.data(), linkBytes.size()) + "]";

    // Profession is encoded in linkBytes[1]
    if (linkBytes.size() >= 2) {
        const char* prof = ProfessionName(linkBytes[1]);
        if (prof) {
            seg.ae2_profession = prof;
            seg.colour  = ProfessionColour(prof);
            seg.display = std::string("Alter Ego ") + prof + " Build Code";

            uint8_t mode = seg.ae2_game_mode;
            seg.tooltip_text  = "Profession: " + seg.ae2_profession + "\n";
            seg.tooltip_text += std::string("Mode: ") + kGameModeNames[mode < 8 ? mode : 7] + "\n";
            seg.tooltip_text += "Click to copy build link";
        }
    }

    // AE2 builds are resolved locally; state Resolved means no API fetch needed.
    seg.state = LinkState::Resolved;
    return seg;
}

// ---------------------------------------------------------------------------
// ParseSegments
// ---------------------------------------------------------------------------

static bool IsBase64Char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}

std::vector<Segment> ParseSegments(const std::string& text) {
    std::vector<Segment> result;
    bool any_link = false;
    size_t i = 0;
    std::string plain;

    auto flushPlain = [&]() {
        if (!plain.empty()) {
            Segment s;
            s.is_link = false;
            s.text    = std::move(plain);
            plain.clear();
            result.push_back(std::move(s));
        }
    };

    auto pushLink = [&](LinkSegment lnk) {
        flushPlain();
        Segment s;
        s.is_link = true;
        s.link    = std::move(lnk);
        result.push_back(std::move(s));
        any_link = true;
    };

    while (i < text.size()) {
        // --- HTTP/HTTPS URL ---
        bool is_http  = i + 7 <= text.size() && text.compare(i, 7, "http://")  == 0;
        bool is_https = i + 8 <= text.size() && text.compare(i, 8, "https://") == 0;
        if (is_http || is_https) {
            size_t j = i;
            while (j < text.size() && !std::isspace((unsigned char)text[j])) j++;
            // Cap URL length to avoid oversized segments from malformed input
            constexpr size_t kMaxUrlLen = 2048;
            if (j - i > kMaxUrlLen) { plain += text[i++]; continue; }
            std::string url = text.substr(i, j - i);
            size_t proto_end = url.find("://");
            std::string domain;
            if (proto_end != std::string::npos) {
                size_t dom_start = proto_end + 3;
                size_t dom_end   = url.find('/', dom_start);
                domain = url.substr(dom_start,
                    dom_end == std::string::npos ? std::string::npos : dom_end - dom_start);
            } else {
                domain = url;
            }
            // Fall back to plain text if domain is empty (malformed URL like http:///path)
            if (domain.empty()) { plain += text[i++]; continue; }
            LinkSegment seg;
            seg.type         = GW2LinkType::Url;
            seg.raw          = url;
            seg.display      = domain;
            seg.tooltip_text = url;
            seg.state        = LinkState::Resolved;
            seg.colour       = IM_COL32(100, 160, 255, 255);
            pushLink(std::move(seg));
            i = j;
            continue;
        }

        // --- AE2 build code ---
        if (i + 4 <= text.size() &&
            text[i] == 'A' && text[i+1] == 'E' && text[i+2] == '2' && text[i+3] == ':') {
            size_t j = i + 4;
            while (j < text.size() && IsBase64Char(text[j])) j++;
            if (j > i + 4) {
                pushLink(ParseAE2(text.substr(i, j - i)));
                i = j;
                continue;
            }
        }

        // --- GW2 chat link: [&...] or \[&...\] ---
        bool escaped = false;
        bool is_link_start = false;
        size_t raw_start = i;

        if (text[i] == '[' && i + 1 < text.size() && text[i+1] == '&') {
            is_link_start = true;
        } else if (text[i] == '\\' && i + 1 < text.size() && text[i+1] == '[' &&
                   i + 2 < text.size() && text[i+2] == '&') {
            is_link_start = true;
            escaped = true;
        }

        if (is_link_start) {
            size_t amp_pos = i + (escaped ? 2 : 1); // position of '['
            // Find closing bracket
            size_t close = std::string::npos;
            for (size_t j = amp_pos + 1; j < text.size(); j++) {
                if (!escaped && text[j] == ']') { close = j; break; }
                if (escaped && text[j] == '\\' && j + 1 < text.size() && text[j+1] == ']') {
                    close = j; break;
                }
            }
            if (close != std::string::npos) {
                size_t raw_end = close + (escaped ? 2 : 1);
                pushLink(DecodeGW2Link(text.substr(raw_start, raw_end - raw_start)));
                i = raw_end;
                continue;
            }
        }

        plain += text[i++];
    }

    flushPlain();

    if (!any_link) result.clear(); // no links → caller uses msg.text as-is
    return result;
}

} // namespace TyrianIM
