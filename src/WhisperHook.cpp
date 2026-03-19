#include "WhisperHook.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <thread>

namespace TyrianIM {

AddonAPI_t* WhisperHook::s_API = nullptr;
WhisperCallback WhisperHook::s_Callback = nullptr;
HookStatus WhisperHook::s_Status = HookStatus::NotInitialized;
bool WhisperHook::s_ProbeMode = false;
bool WhisperHook::s_EventsSubscribed = false;
bool WhisperHook::s_ShowDebugWindow = false;
std::mutex WhisperHook::s_ProbeMutex;
std::vector<std::string> WhisperHook::s_ProbeLines;
std::atomic<bool> WhisperHook::s_IsSending{false};
std::atomic<bool> WhisperHook::s_IsRestoring{false};
static std::atomic<int> s_RestoreGeneration{0};  // Cancel stale deferred restores
ChatMessageType WhisperHook::s_LastChatChannel = ChatMsg_Local;
ErrorCallback WhisperHook::s_ErrorCallback = nullptr;
std::string WhisperHook::s_LastWhisperTarget;

const char* WhisperHook::ChannelCommand(ChatMessageType type) {
    switch (type) {
    case ChatMsg_Local:   return "s ";
    case ChatMsg_Map:     return "m ";
    case ChatMsg_Party:   return "p ";
    case ChatMsg_Squad:   return "d ";
    case ChatMsg_Guild:   return "g ";
    case ChatMsg_TeamPvP: return "t ";
    case ChatMsg_TeamWvW: return "t ";
    default:              return "s ";
    }
}

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

    // Subscribe to EV_CHAT:Message from the GW2-Chat / "Events: Chat" addon
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

// Find the GW2 game window (try both DX and GR class names)
static HWND FindGameWindow() {
    HWND hwnd = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
    if (!hwnd) hwnd = FindWindowA("ArenaNet_Gr_Window_Class", nullptr);
    return hwnd;
}

bool WhisperHook::SendWhisper(const std::string& recipient, const std::string& message, bool restoreChannel) {
    if (!s_API) {
        ProbeLog("[SEND] FAILED: s_API is null");
        return false;
    }
    if (recipient.empty() || message.empty()) {
        ProbeLog("[SEND] FAILED: recipient or message empty (r='%s' m='%s')",
            recipient.c_str(), message.c_str());
        return false;
    }
    if (s_IsSending.load()) {
        ProbeLog("[SEND] FAILED: already sending");
        return false;
    }

    HWND hwnd = FindGameWindow();
    ProbeLog("[SEND] HWND=0x%p", (void*)hwnd);

    if (!hwnd) {
        ProbeLog("[SEND] FAILED: Could not find GW2 window");
        s_API->Log(LOGL_WARNING, "TyrianIM",
            "SendWhisper: Could not find GW2 window");
        return false;
    }

    char winClass[128] = {};
    GetClassNameA(hwnd, winClass, sizeof(winClass));
    ProbeLog("[SEND] Window class='%s'", winClass);
    ProbeLog("[SEND] Recipient: '%s'", recipient.c_str());
    ProbeLog("[SEND] Message: '%s'", message.c_str());

    s_IsSending.store(true);
    s_LastWhisperTarget = recipient;  // Track for error attribution
    int myGen = ++s_RestoreGeneration;  // Cancel any pending deferred restores
    auto api = s_API;
    std::string rcpt = recipient;
    std::string msg = message;

    std::thread([hwnd, rcpt, msg, api, restoreChannel, myGen]() {
        // Serenade-style SendInput: wVk + wScan with MapVirtualKeyA
        auto sendKeyDown = [](WORD vk) {
            INPUT input = {};
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = vk;
            input.ki.wScan = (WORD)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            input.ki.dwFlags = 0;
            SendInput(1, &input, sizeof(INPUT));
        };
        auto sendKeyUp = [](WORD vk) {
            INPUT input = {};
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = vk;
            input.ki.wScan = (WORD)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
        };
        auto typeChar = [&sendKeyDown, &sendKeyUp](char c) {
            SHORT vks = VkKeyScanA(c);
            if (vks == -1) return;
            WORD vk = LOBYTE(vks);
            bool needShift = (HIBYTE(vks) & 1) != 0;
            if (needShift) sendKeyDown(VK_SHIFT);
            sendKeyDown(vk);
            Sleep(10);
            sendKeyUp(vk);
            if (needShift) sendKeyUp(VK_SHIFT);
            Sleep(5);
        };
        auto typeString = [&typeChar](const std::string& s) {
            for (char c : s) typeChar(c);
        };
        auto pressKey = [&sendKeyDown, &sendKeyUp](WORD vk) {
            sendKeyDown(vk);
            Sleep(30);
            sendKeyUp(vk);
        };
        // Ctrl+V paste via SendInput
        auto pasteFromClipboard = [&sendKeyDown, &sendKeyUp]() {
            sendKeyDown(VK_CONTROL);
            sendKeyDown('V');
            Sleep(30);
            sendKeyUp('V');
            sendKeyUp(VK_CONTROL);
            Sleep(30);
        };
        // Set clipboard text, returns true on success
        auto setClipboard = [](const std::string& text) -> bool {
            if (!OpenClipboard(NULL)) return false;
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
            if (!hMem) { CloseClipboard(); return false; }
            char* pMem = (char*)GlobalLock(hMem);
            memcpy(pMem, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
            CloseClipboard();
            return true;
        };

        // Save existing clipboard content so we can restore it after
        std::string savedClipboard;
        if (OpenClipboard(NULL)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* pData = (char*)GlobalLock(hData);
                if (pData) savedClipboard = pData;
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }

        // Step 1: Open chat in command mode (pre-types '/' for us)
        api->GameBinds_InvokeAsync(GB_UiChatCommand, 50);
        ProbeLog("[SEND] GameBinds: GB_UiChatCommand invoked (types '/')");
        Sleep(150);

        // Step 2: Type 'w ' after the pre-typed '/' to form '/w '
        typeString("w ");
        ProbeLog("[SEND] Typed 'w ' (after /)");
        Sleep(100);

        // Step 3: Paste recipient name
        if (setClipboard(rcpt)) {
            pasteFromClipboard();
            ProbeLog("[SEND] Pasted recipient: '%s'", rcpt.c_str());
        } else {
            typeString(rcpt);
            ProbeLog("[SEND] Typed recipient (clipboard failed): '%s'", rcpt.c_str());
        }
        Sleep(50);

        // Step 4: Tab to message field
        pressKey(VK_TAB);
        ProbeLog("[SEND] Tab pressed");
        Sleep(50);

        // Step 5: Paste message
        if (setClipboard(msg)) {
            pasteFromClipboard();
            ProbeLog("[SEND] Pasted message: '%s'", msg.c_str());
        } else {
            typeString(msg);
            ProbeLog("[SEND] Typed message (clipboard failed): '%s'", msg.c_str());
        }
        Sleep(30);

        // Step 6: Enter to send
        pressKey(VK_RETURN);
        ProbeLog("[SEND] Enter pressed - whisper sent");

        // Release send lock so user can type in TIM again
        s_IsSending.store(false);

        // Restore clipboard right away
        if (!savedClipboard.empty()) {
            setClipboard(savedClipboard);
        }

        // Step 7: Deferred channel restore (if enabled)
        if (!restoreChannel) {
            ProbeLog("[SEND] Channel restore disabled, done");
            return;
        }

        // Set restoring flag to suppress TIM auto-focus during restore
        s_IsRestoring.store(true);

        // Check every 200ms; restore once no keys are pressed for 1 second
        // Give up after 15 seconds to avoid zombie thread
        auto isAnyKeyPressed = []() -> bool {
            for (int vk = 0x08; vk <= 0xFE; vk++) {
                if (GetAsyncKeyState(vk) & 0x8000) return true;
            }
            return false;
        };

        ProbeLog("[SEND] Waiting for keyboard quiet before channel restore (gen=%d)...", myGen);
        int quietMs = 0;
        int totalMs = 0;
        const int QUIET_THRESHOLD = 1000;  // 1 second of no keys
        const int MAX_WAIT = 15000;        // give up after 15 seconds
        while (quietMs < QUIET_THRESHOLD && totalMs < MAX_WAIT) {
            Sleep(200);
            totalMs += 200;
            // Cancel if a newer send started
            if (s_RestoreGeneration.load() != myGen) {
                ProbeLog("[SEND] Channel restore cancelled (newer send detected, gen=%d vs %d)",
                    myGen, s_RestoreGeneration.load());
                s_IsRestoring.store(false);
                return;
            }
            if (isAnyKeyPressed()) {
                quietMs = 0;
            } else {
                quietMs += 200;
            }
        }

        // Final check: cancel if a newer send started during the wait
        if (s_RestoreGeneration.load() != myGen) {
            ProbeLog("[SEND] Channel restore cancelled (stale gen=%d)", myGen);
            s_IsRestoring.store(false);
            return;
        }

        if (totalMs >= MAX_WAIT) {
            ProbeLog("[SEND] Channel restore skipped (timeout)");
        } else {
            // Close any open chat first
            pressKey(VK_RETURN);
            Sleep(100);

            // Open chat in regular mode and type the full command
            const char* restoreCmd = ChannelCommand(s_LastChatChannel);
            api->GameBinds_InvokeAsync(GB_UiChatCommand, 50);
            Sleep(200);
            typeString(restoreCmd);
            Sleep(50);
            pressKey(VK_RETURN);
            ProbeLog("[SEND] Restored channel: '/%s' (type=%d)", restoreCmd, (int)s_LastChatChannel);
        }
        s_IsRestoring.store(false);
        ProbeLog("[SEND] COMPLETE");
        return;
    }).detach();

    return true;
}

bool WhisperHook::IsSending() {
    return s_IsSending.load();
}

bool WhisperHook::IsRestoring() {
    return s_IsRestoring.load();
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
            // Error structure is undocumented — try both GenericMessage and plain string
            ChatGenericMessage* gm = &msg->ErrorGeneric;
            if (IsReadableMemory(gm, sizeof(ChatGenericMessage))) {
                ProbeLog("Error (GenericMsg) Content: \"%s\"",
                    (gm->Content && IsReadableMemory(gm->Content, 1)) ? gm->Content : "(unreadable)");
                ProbeLog("Error (GenericMsg) CharName: \"%s\"",
                    (gm->CharacterName && IsReadableMemory(gm->CharacterName, 1)) ? gm->CharacterName : "(unreadable)");
            }
            // Also try as plain string (same union offset as SquadMessage)
            if (msg->SquadMessage && IsReadableMemory(msg->SquadMessage, 1)) {
                ProbeLog("Error (string): \"%s\"", msg->SquadMessage);
            }
        }
        ProbeLog("========== END ==========");
    }

    // Handle error messages — detect whisper failures
    if (msg->Type == ChatMsg_Error && s_ErrorCallback && !s_LastWhisperTarget.empty()) {
        // Try to read error content — try GenericMessage.Content first, then plain string
        std::string errorText;
        ChatGenericMessage* gm = &msg->ErrorGeneric;
        if (gm->Content && IsReadableMemory(gm->Content, 1)) {
            errorText = gm->Content;
        } else if (msg->SquadMessage && IsReadableMemory(msg->SquadMessage, 1)) {
            errorText = msg->SquadMessage;
        }

        if (!errorText.empty() && errorText.find("not online") != std::string::npos) {
            ProbeLog("[ERROR] Whisper failed: \"%s\" (target: %s)", errorText.c_str(), s_LastWhisperTarget.c_str());
            s_ErrorCallback(s_LastWhisperTarget, errorText);
        }
    }

    // Track the last non-whisper chat channel for restoring after whisper send
    if (msg->Type != ChatMsg_Whisper && msg->Type != ChatMsg_Error &&
        msg->Type != ChatMsg_Emote && msg->Type != ChatMsg_EmoteCustom &&
        msg->Type != ChatMsg_GuildMotD && msg->Type != ChatMsg_SquadMessage &&
        msg->Type != ChatMsg_SquadBroadcast) {
        static const char* CH_NAMES[] = {
            "Error", "Guild", "GuildMotD", "Local", "Map", "Party",
            "Squad", "SquadMessage", "SquadBroadcast", "TeamPvP",
            "TeamWvW", "Whisper", "Emote", "EmoteCustom"
        };
        const char* oldName = (s_LastChatChannel >= 0 && s_LastChatChannel <= ChatMsg_EmoteCustom)
            ? CH_NAMES[s_LastChatChannel] : "?";
        const char* newName = (msg->Type >= 0 && msg->Type <= ChatMsg_EmoteCustom)
            ? CH_NAMES[msg->Type] : "?";
        if (s_LastChatChannel != msg->Type) {
            ProbeLog("[CHANNEL] Changed: %s(%d) -> %s(%d)",
                oldName, (int)s_LastChatChannel, newName, (int)msg->Type);
        }
        s_LastChatChannel = msg->Type;
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
            // Outgoing: sender=charName, recipient=acctName (contact key)
            s_Callback(character_name, account_name, content, false);
        } else {
            // Incoming: sender=acctName (contact key), recipient=charName
            s_Callback(account_name, character_name, content, true);
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
// to help discover the payload struct layout.
// ============================================================================
void WhisperHook::ProbeRawEventData(const char* eventName, void* eventArgs) {
    if (!eventArgs) return;

    ProbeLog("========== CHAT EVENT: %s ==========", eventName);
    ProbeLog("payload ptr: %p", eventArgs);

    uintptr_t base = (uintptr_t)eventArgs;

    // Dump first 0x100 bytes as qwords with annotations
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

    // Scan each qword as a potential char* or wchar_t*
    ProbeLog("--- String scan ---");
    for (int off = 0; off < 0x100; off += 8) {
        if (!IsReadableMemory((void*)(base + off), 8)) break;
        uintptr_t val = *(uintptr_t*)(base + off);
        if (val < 0x10000 || val > 0x00007FFFFFFFFFFF) continue;
        if (!IsReadableMemory((void*)val, 4)) continue;

        // Try as char*
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

        // Try as wchar_t*
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

    // Also dump first 32 bytes as uint32 array (for channel types, flags, etc.)
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
