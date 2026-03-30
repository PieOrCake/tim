#include "WhisperHook.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <cstdarg>

namespace TyrianIM {

AddonAPI_t* WhisperHook::s_API = nullptr;
WhisperCallback WhisperHook::s_Callback = nullptr;
ErrorCallback WhisperHook::s_ErrorCallback = nullptr;
NamePairCallback WhisperHook::s_NamePairCallback = nullptr;
HookStatus WhisperHook::s_Status = HookStatus::NotInitialized;
bool WhisperHook::s_ProbeMode = false;
bool WhisperHook::s_EventsSubscribed = false;
bool WhisperHook::s_ShowDebugWindow = false;
std::mutex WhisperHook::s_ProbeMutex;
std::vector<std::string> WhisperHook::s_ProbeLines;

void WhisperHook::Initialize(AddonAPI_t* api) {
    s_API = api;
    s_Status = HookStatus::NotInitialized;
    s_ProbeMode = false;
    s_EventsSubscribed = false;

    if (s_API) {
        s_API->Log(LOGL_INFO, "TyrianIM", "WhisperHook initialized (event-based mode)");
    }
}

bool WhisperHook::InstallHooks() {
    if (!s_API) {
        s_Status = HookStatus::Failed;
        return false;
    }

    s_Status = HookStatus::Scanning;

    s_API->Log(LOGL_INFO, "TyrianIM",
        "WhisperHook: Subscribing to \"" GW2_CHAT_EVENT "\"...");

    s_API->Events_Subscribe(GW2_CHAT_EVENT, OnEvChatMessage);

    s_EventsSubscribed = true;
    s_Status = HookStatus::Hooked;
    s_API->Log(LOGL_INFO, "TyrianIM",
        "WhisperHook: Listening on " GW2_CHAT_EVENT
        ". Requires 'Events: Chat' addon from Nexus library.");

    return true;
}

void WhisperHook::ProbeLog(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    std::lock_guard<std::mutex> lock(s_ProbeMutex);
    s_ProbeLines.push_back(buf);
}

// ============================================================================
// Nexus Event Handler for EV_CHAT:Message
// Payload is ChatMessage* (defined in Chat.h from jsantorek/GW2-Chat).
// Parses whisper messages and forwards to callback.
// In probe mode: dumps all message types to the debug window.
// ============================================================================
void WhisperHook::OnEvChatMessage(void* eventArgs) {
    if (!s_API || !eventArgs) return;

    ChatMessage* msg = (ChatMessage*)eventArgs;

    // Probe mode: dump structured data for any message type
    if (s_ProbeMode) {
        {
            std::lock_guard<std::mutex> lock(s_ProbeMutex);
            if (s_ProbeLines.size() >= 5000) return;
        }

        static const char* TYPE_NAMES[] = {
            "Error", "Guild", "GuildMotD", "Local", "Map", "Party",
            "Squad", "SquadMessage", "SquadBroadcast", "TeamPvP",
            "TeamWvW", "Whisper", "Emote", "EmoteCustom"
        };
        const char* type_name = (msg->Type >= 0 && msg->Type <= ChatMsg_EmoteCustom)
            ? TYPE_NAMES[msg->Type] : "Unknown";

        ProbeLog("========== EV_CHAT:Message ==========");
        ProbeLog("Type: %d (%s) | Flags: 0x%X", msg->Type, type_name, msg->Flags);

        // For types with GenericMessage (Guild, Local, Map, Party, Squad, Whisper, TeamPvP)
        if (msg->Type == ChatMsg_Whisper || msg->Type == ChatMsg_Local ||
            msg->Type == ChatMsg_Map || msg->Type == ChatMsg_Party ||
            msg->Type == ChatMsg_Squad || msg->Type == ChatMsg_TeamPvP ||
            msg->Type == ChatMsg_Guild) {

            ChatGenericMessage* gm = &msg->Whisper; // union — same offset for all GenericMessage types
            ProbeLog("Account GUID:  {%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}",
                gm->Account.Data1, gm->Account.Data2, gm->Account.Data3,
                gm->Account.Data4[0], gm->Account.Data4[1], gm->Account.Data4[2],
                gm->Account.Data4[3], gm->Account.Data4[4], gm->Account.Data4[5],
                gm->Account.Data4[6], gm->Account.Data4[7]);
            ProbeLog("CharacterName: \"%s\"", gm->CharacterName ? gm->CharacterName : "(null)");
            ProbeLog("AccountName:   \"%s\"", gm->AccountName ? gm->AccountName : "(null)");
            ProbeLog("Content:       \"%s\"", gm->Content ? gm->Content : "(null)");

            if (msg->Type == ChatMsg_Guild) {
                bool is_represented = (msg->Flags & ChatFlag_Guild_IsRepresented) != 0;
                ProbeLog("IsRepresented: %s", is_represented ? "YES (/g)" : "NO (g1-g5?)");
            }
            if (msg->Type == ChatMsg_Whisper) {
                bool from_me = (msg->Flags & ChatFlag_Whisper_IsFromMe) != 0;
                ProbeLog("Direction:     %s", from_me ? "OUTGOING (from me)" : "INCOMING");
            }
        } else if (msg->Type == ChatMsg_SquadMessage) {
            ProbeLog("SquadMessage: \"%s\"", msg->SquadMessage ? msg->SquadMessage : "(null)");
        } else if (msg->Type == ChatMsg_Error) {
            ChatGenericMessage* gm = &msg->ErrorGeneric;
            if (IsReadableMemory(gm, sizeof(ChatGenericMessage))) {
                ProbeLog("Error (GenericMsg) Content: \"%s\"",
                    (gm->Content && IsReadableMemory(gm->Content, 1)) ? gm->Content : "(unreadable)");
                ProbeLog("Error (GenericMsg) CharName: \"%s\"",
                    (gm->CharacterName && IsReadableMemory(gm->CharacterName, 1)) ? gm->CharacterName : "(unreadable)");
            }
            if (msg->SquadMessage && IsReadableMemory(msg->SquadMessage, 1)) {
                ProbeLog("Error (string): \"%s\"", msg->SquadMessage);
            }
        }
        ProbeLog("========== END ==========");
    }

    // Extract name pairs from any message type with ChatGenericMessage
    if (s_NamePairCallback) {
        ChatGenericMessage* gm = nullptr;
        switch (msg->Type) {
            case ChatMsg_Party:   gm = &msg->Party;   break;
            case ChatMsg_Squad:   gm = &msg->Squad;   break;
            case ChatMsg_Guild:   gm = &msg->Local;   break;
            case ChatMsg_Local:   gm = &msg->Local;   break;
            case ChatMsg_Map:     gm = &msg->Map;     break;
            case ChatMsg_TeamPvP: gm = &msg->TeamPvP; break;
            default: break;
        }
        if (gm) {
            std::string cn = (gm->CharacterName && IsReadableMemory(gm->CharacterName, 1)) ? gm->CharacterName : "";
            std::string an = (gm->AccountName && IsReadableMemory(gm->AccountName, 1)) ? gm->AccountName : "";
            if (!cn.empty() && !an.empty() && cn != an) {
                s_NamePairCallback(cn, an);
            }
        }
    }

    // Process whisper messages
    if (msg->Type == ChatMsg_Whisper && s_Callback) {
        ChatGenericMessage* wm = &msg->Whisper;
        bool is_from_me = (msg->Flags & ChatFlag_Whisper_IsFromMe) != 0;

        std::string character_name = wm->CharacterName ? wm->CharacterName : "";
        std::string account_name = wm->AccountName ? wm->AccountName : "";
        std::string content = wm->Content ? wm->Content : "";

        // Use account name if available, otherwise character name
        std::string contact = !account_name.empty() ? account_name : character_name;

        if (is_from_me) {
            // Outgoing: sender=charName, recipient=contact (acctName or charName fallback)
            s_Callback(character_name, contact, content, false);
        } else {
            // Incoming: sender=contact (acctName or charName fallback), recipient=charName
            s_Callback(contact, character_name, content, true);
        }

        if (s_API) {
            char log_buf[256];
            snprintf(log_buf, sizeof(log_buf),
                "WhisperHook: %s whisper %s %s: \"%.80s\"",
                is_from_me ? "Sent" : "Received",
                is_from_me ? "to" : "from",
                character_name.c_str(),
                content.c_str());
            s_API->Log(LOGL_DEBUG, "TyrianIM", log_buf);
        }
    }
}

// ============================================================================
// ProbeRawEventData — Dumps raw bytes at the event payload pointer
// ============================================================================
void WhisperHook::ProbeRawEventData(const char* eventName, void* eventArgs) {
    if (!eventArgs) return;

    ProbeLog("========== CHAT EVENT: %s ==========", eventName);
    ProbeLog("payload ptr: %p", eventArgs);

    uintptr_t base = (uintptr_t)eventArgs;

    ProbeLog("--- Raw qwords ---");
    for (int off = 0; off < 0x100; off += 8) {
        if (!IsReadableMemory((void*)(base + off), 8)) {
            ProbeLog("  +0x%03X: <unreadable>", off);
            break;
        }
        uintptr_t val = *(uintptr_t*)(base + off);
        const char* annotation = "";
        if (val == 0) annotation = " [null]";
        else if (val < 0x1000) annotation = " [small int]";
        else if (val >= 0x10000 && val <= 0x00007FFFFFFFFFFF) annotation = " [ptr?]";
        ProbeLog("  +0x%03X: 0x%016llX%s", off, (unsigned long long)val, annotation);
    }

    ProbeLog("--- String scan ---");
    for (int off = 0; off < 0x100; off += 8) {
        if (!IsReadableMemory((void*)(base + off), 8)) break;
        uintptr_t val = *(uintptr_t*)(base + off);
        if (val < 0x10000 || val > 0x00007FFFFFFFFFFF) continue;
        if (!IsReadableMemory((void*)val, 4)) continue;

        const char* s = (const char*)val;
        int len = 0;
        bool ok = true;
        for (int i = 0; i < 120 && s[i]; i++) {
            if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7E) { ok = false; break; }
            len++;
        }
        if (ok && len >= 2) {
            ProbeLog("  +0x%03X -> char*(%d): \"%.100s\"", off, len, s);
        }

        if (IsReadableMemory((void*)val, 8)) {
            const wchar_t* ws = (const wchar_t*)val;
            int wlen = 0;
            bool wok = true;
            for (int i = 0; i < 80 && ws[i]; i++) {
                if (ws[i] < 0x20 || ws[i] > 0xFFFD) { wok = false; break; }
                wlen++;
            }
            if (wok && wlen >= 2) {
                char utf8[256];
                int written = WideCharToMultiByte(CP_UTF8, 0, ws, (wlen < 60 ? wlen : 60),
                    utf8, sizeof(utf8) - 1, NULL, NULL);
                if (written > 0) utf8[written] = 0; else utf8[0] = 0;
                ProbeLog("  +0x%03X -> wchar(%d): \"%s\"", off, wlen, utf8);
            }
        }
    }

    ProbeLog("--- As uint32 array ---");
    if (IsReadableMemory((void*)base, 32)) {
        uint32_t* u32 = (uint32_t*)base;
        for (int i = 0; i < 8; i++) {
            ProbeLog("  [%d] = 0x%X (%u)", i, u32[i], u32[i]);
        }
    }

    ProbeLog("========== END CHAT EVENT ==========");
}

// ============================================================================
// Debug Window
// ============================================================================
void WhisperHook::RenderDebugWindow() {
    if (!s_ShowDebugWindow) return;

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("TyrianIM Debug##WhisperDebug", &s_ShowDebugWindow)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Mode: Nexus Events | Status: %s | Probe: %s",
        GetStatusString(), s_ProbeMode ? "ON" : "OFF");

    ImGui::TextWrapped("Event: " GW2_CHAT_EVENT " (from 'Events: Chat' addon)");

    if (ImGui::Button(s_ProbeMode ? "Disable Probe" : "Enable Probe")) {
        SetProbeMode(!s_ProbeMode);
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy All")) {
        std::string all;
        {
            std::lock_guard<std::mutex> lock(s_ProbeMutex);
            for (const auto& line : s_ProbeLines) {
                all += line;
                all += '\n';
            }
        }
        ImGui::SetClipboardText(all.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        std::lock_guard<std::mutex> lock(s_ProbeMutex);
        s_ProbeLines.clear();
    }

    ImGui::Separator();
    ImGui::BeginChild("##ProbeOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard<std::mutex> lock(s_ProbeMutex);
        for (const auto& line : s_ProbeLines) {
            if (line.find("===") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", line.c_str());
            } else if (line.find("char*") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 1.0f, 1.0f), "%s", line.c_str());
            } else if (line.find("wchar") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "%s", line.c_str());
            } else if (line.find("[ptr?]") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s", line.c_str());
            } else if (line.find("EVENT") != std::string::npos || line.find("fired") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 1.0f, 1.0f), "%s", line.c_str());
            } else {
                ImGui::Text("%s", line.c_str());
            }
        }
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

// ============================================================================
// Utility
// ============================================================================
bool WhisperHook::IsReadableMemory(const void* ptr, size_t size) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ
                   | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    if (!(mbi.Protect & readable)) return false;
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t region_end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (addr + size > region_end) return false;
    return true;
}

void WhisperHook::RemoveHooks() {
    if (!s_API) return;

    if (s_EventsSubscribed) {
        s_API->Events_Unsubscribe(GW2_CHAT_EVENT, OnEvChatMessage);
        s_EventsSubscribed = false;
        s_API->Log(LOGL_INFO, "TyrianIM", "WhisperHook: Unsubscribed from " GW2_CHAT_EVENT);
    }

    s_Status = HookStatus::Disabled;
}

void WhisperHook::Shutdown() {
    if (s_Status == HookStatus::Hooked) {
        RemoveHooks();
    }
    s_Callback = nullptr;
    s_API = nullptr;
    s_Status = HookStatus::NotInitialized;
}

void WhisperHook::SetCallback(WhisperCallback callback) {
    s_Callback = callback;
}

void WhisperHook::SetErrorCallback(ErrorCallback callback) {
    s_ErrorCallback = callback;
}

void WhisperHook::SetNamePairCallback(NamePairCallback callback) {
    s_NamePairCallback = callback;
}

HookStatus WhisperHook::GetStatus() {
    return s_Status;
}

const char* WhisperHook::GetStatusString() {
    switch (s_Status) {
        case HookStatus::NotInitialized: return "Not Initialized";
        case HookStatus::Scanning:       return "Subscribing...";
        case HookStatus::Hooked:         return "Listening";
        case HookStatus::Failed:         return "Failed";
        case HookStatus::Disabled:       return "Disabled";
        default:                         return "Unknown";
    }
}

void WhisperHook::SetProbeMode(bool enabled) {
    s_ProbeMode = enabled;
    if (s_API) {
        s_API->Log(LOGL_INFO, "TyrianIM",
            enabled ? "WhisperHook: Probe mode ENABLED - chat events will dump raw data to debug window"
                    : "WhisperHook: Probe mode DISABLED");
    }
}

bool WhisperHook::GetProbeMode() {
    return s_ProbeMode;
}

} // namespace TyrianIM
