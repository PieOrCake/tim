#include <windows.h>
#include <mmsystem.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <cstring>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>
#include "../include/nexus/Nexus.h"
#include "ChatManager.h"
#include "WhisperHook.h"

// Version constants
#define V_MAJOR 0
#define V_MINOR 1
#define V_BUILD 0
#define V_REVISION 0

// Quick Access icon identifiers
#define QA_ID "QA_TYRIAN_IM"
#define TEX_ICON "TEX_TYRIAN_IM"
#define TEX_ICON_HOVER "TEX_TYRIAN_IM_HOVER"

// Nexus event identifier for Unofficial Extras chat messages
#define EV_UNOFFICIAL_EXTRAS_CHAT_MESSAGE "EV_UNOFFICIAL_EXTRAS_CHAT_MESSAGE"
#define EV_MUMBLE_IDENTITY_UPDATED_ID "EV_MUMBLE_IDENTITY_UPDATED"

// Unofficial Extras types (matching Definitions.h from Krappa322)
struct SquadMessageInfo {
    uint32_t ChannelId;
    uint8_t Type;       // 0 = Party, 1 = Squad
    uint8_t Subgroup;
    uint8_t IsBroadcast;
    uint8_t _Unused1;
    const char* Timestamp;
    uint64_t TimestampLength;
    const char* AccountName;
    uint64_t AccountNameLength;
    const char* CharacterName;
    uint64_t CharacterNameLength;
    const char* Text;
    uint64_t TextLength;
};

// Embedded 24x24 speech bubble PNG for quick access icon
static const unsigned char ICON_DATA[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x18,
    0x08, 0x06, 0x00, 0x00, 0x00, 0xE0, 0x77, 0x3D, 0xF8, 0x00, 0x00, 0x00,
    0x8C, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0xED, 0x95, 0xD1, 0x09, 0xC0,
    0x20, 0x0C, 0x44, 0x55, 0xBA, 0x84, 0x93, 0x98, 0xFD, 0xBF, 0x92, 0x49,
    0x3A, 0x46, 0x8B, 0x50, 0x41, 0x34, 0x56, 0x9A, 0x68, 0xA1, 0xD4, 0xFB,
    0x14, 0xF3, 0x2E, 0x89, 0x70, 0x1A, 0xF3, 0x75, 0x59, 0xEE, 0x10, 0x11,
    0x0F, 0x09, 0x0C, 0x00, 0x2A, 0xDE, 0xC6, 0x81, 0x43, 0x08, 0xBB, 0xC4,
    0x00, 0xAF, 0xFA, 0xDC, 0xC8, 0x95, 0x97, 0xA4, 0xF0, 0x56, 0xAD, 0xCB,
    0xDD, 0x35, 0xF0, 0xA4, 0xC8, 0xC8, 0x57, 0x5C, 0x4D, 0x30, 0x5A, 0x6E,
    0x19, 0xF4, 0xB4, 0x56, 0xF4, 0xA7, 0x15, 0x01, 0x80, 0x25, 0x22, 0xAF,
    0x05, 0x12, 0x91, 0xBF, 0xCD, 0x22, 0x8D, 0x09, 0x31, 0xB5, 0xCD, 0xB8,
    0x4E, 0xB9, 0x44, 0x0F, 0x0C, 0xBB, 0x71, 0xCD, 0x75, 0x04, 0x4C, 0x91,
    0x5A, 0x71, 0x02, 0xE9, 0xA7, 0x53, 0xAA, 0x7A, 0x83, 0x51, 0xB1, 0xFD,
    0x4A, 0xF7, 0xA6, 0x65, 0x30, 0x0D, 0x3E, 0x43, 0x27, 0x83, 0xE3, 0x45,
    0xBD, 0xBC, 0x7B, 0x61, 0x13, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
    0x44, 0xAE, 0x42, 0x60, 0x82
};
static const unsigned int ICON_DATA_SIZE = sizeof(ICON_DATA);

// Global variables
HMODULE hSelf;
AddonDefinition_t AddonDef{};
AddonAPI_t* APIDefs = nullptr;
bool g_WindowVisible = false;

// UI State
static std::string g_SelectedContact;
static std::string g_PendingDeleteContact;
static std::string g_ClipboardMsg;
static double g_ClipboardMsgExpiry = 0.0;
static char g_InputBuf[512] = "";
static bool g_ScrollToBottom = false;
static bool g_FocusInput = false;
static std::string g_DataDir;

// Track the last outgoing whisper TIM sent, to deduplicate against game events
static std::mutex g_LastTIMSendMutex;
static std::string g_LastTIMSendRecipient;
static std::string g_LastTIMSendText;
static uint64_t g_LastTIMSendTime = 0; // epoch_ms when last whisper was sent

// Floating icon state
static bool g_ShowFloatingIcon = true;
static float g_FloatingIconX = 100.0f;
static float g_FloatingIconY = 100.0f;
static bool g_FloatingIconPosInitialized = false;
static bool g_FloatingIconLocked = true;
static float g_FloatingIconScale = 1.0f;

// Settings
static bool g_AutoOpenOnWhisper = true;
static bool g_SoundEnabled = true;
static bool g_RestoreChannelAfterWhisper = false;
static std::string g_SelectedSound;  // filename only, empty = default system sound
static std::vector<std::string> g_SoundFiles;  // cached list of .wav files in sounds/ dir

static std::string SoundsDir() {
    return g_DataDir.empty() ? "" : g_DataDir + "/sounds";
}

static std::string SoundFullPath(const std::string& filename) {
    return SoundsDir() + "/" + filename;
}

static void ScanSoundFiles() {
    g_SoundFiles.clear();
    std::string dir = SoundsDir();
    if (dir.empty()) return;
    try {
        std::filesystem::create_directories(dir);
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".wav" || ext == ".mp3") {
                g_SoundFiles.push_back(entry.path().filename().string());
            }
        }
        std::sort(g_SoundFiles.begin(), g_SoundFiles.end());
    } catch (...) {}
}

static void PlayNotificationSound(const std::string& filename) {
    if (filename.empty()) {
        PlaySoundA("SystemNotification", NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT);
        return;
    }
    std::string fullPath = SoundFullPath(filename);
    std::string ext = filename.substr(filename.find_last_of('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".mp3") {
        // Use MCI for mp3 playback
        std::string cmd = "close TyrianIMSound";
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
        cmd = "open \"" + fullPath + "\" type mpegvideo alias TyrianIMSound";
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
        cmd = "play TyrianIMSound";
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    } else {
        PlaySoundA(fullPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

// Settings persistence helpers
static void SaveSettings();
static void LoadSettings();

// Colors
static const ImVec4 COLOR_SELF(0.4f, 0.8f, 1.0f, 1.0f);      // Cyan for own messages
static const ImVec4 COLOR_OTHER(0.9f, 0.7f, 0.3f, 1.0f);      // Gold for others
static const ImVec4 COLOR_TIMESTAMP(0.5f, 0.5f, 0.5f, 1.0f);  // Grey
static const ImVec4 COLOR_UNREAD(1.0f, 0.4f, 0.4f, 1.0f);     // Red for unread badge
static const ImVec4 COLOR_STATUS_OK(0.3f, 0.9f, 0.3f, 1.0f);  // Green
static const ImVec4 COLOR_STATUS_WARN(1.0f, 0.8f, 0.2f, 1.0f);// Yellow
static const ImVec4 COLOR_STATUS_ERR(1.0f, 0.3f, 0.3f, 1.0f); // Red
// Bubble colors
static const ImU32 COLOR_BUBBLE_SELF  = IM_COL32(30, 60, 100, 200);   // Dark blue
static const ImU32 COLOR_BUBBLE_OTHER = IM_COL32(50, 50, 55, 200);    // Dark grey
static const ImU32 COLOR_HEADER_BG    = IM_COL32(35, 35, 45, 220);    // Header bar
static const ImU32 COLOR_AVATAR_BG    = IM_COL32(60, 130, 180, 255);  // Avatar circle
static const ImU32 COLOR_UNREAD_DOT   = IM_COL32(255, 80, 80, 255);   // Unread dot
static const ImU32 COLOR_ACTIVE_BG    = IM_COL32(50, 80, 120, 180);   // Active contact bg
static const ImU32 COLOR_INPUT_BG     = IM_COL32(30, 30, 38, 220);    // Input area bg
static float g_FontScale = 1.0f;

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: hSelf = hModule; break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }
    return TRUE;
}

// Get current epoch in milliseconds
static uint64_t NowEpochMs() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

// Format epoch_ms to a short time string like "14:32"
static std::string FormatTime(uint64_t epoch_ms) {
    time_t t = static_cast<time_t>(epoch_ms / 1000);
    struct tm tm_info;
    localtime_s(&tm_info, &t);
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M", &tm_info);
    return std::string(buf);
}

// Event handler for Unofficial Extras chat messages (squad/party)
static void OnChatMessage(void* eventArgs) {
    if (!eventArgs) return;

    SquadMessageInfo* info = (SquadMessageInfo*)eventArgs;
    if (!info->AccountName || !info->Text) return;

    TyrianIM::ChatMessage msg;
    msg.sender = std::string(info->AccountName, info->AccountNameLength);
    msg.text = std::string(info->Text, info->TextLength);
    msg.epoch_ms = NowEpochMs();
    msg.timestamp = FormatTime(msg.epoch_ms);
    msg.source = TyrianIM::MessageSource::Squad;

    // Determine direction
    const auto& self = TyrianIM::ChatManager::GetSelfAccountName();
    if (!self.empty() && msg.sender == self) {
        msg.direction = TyrianIM::MessageDirection::Outgoing;
        msg.recipient = "Squad";
    } else {
        msg.direction = TyrianIM::MessageDirection::Incoming;
        msg.recipient = self;
    }

    TyrianIM::ChatManager::AddMessage(msg);
    g_ScrollToBottom = true;
}

// Error callback - called when a whisper-related error is detected
static void OnWhisperError(const std::string& contact, const std::string& error_text) {
    // The contact here is the character name (last whisper target), not the account name.
    // We need to find which conversation this maps to.
    // Try to find a conversation where display_name matches, otherwise use contact directly.
    std::string account_key;
    auto convos = TyrianIM::ChatManager::GetConversations();
    for (const auto* c : convos) {
        if (c->display_name == contact || c->contact == contact) {
            account_key = c->contact;
            break;
        }
    }
    if (account_key.empty()) account_key = contact;

    TyrianIM::ChatMessage msg;
    msg.sender = "System";
    msg.recipient = account_key;
    msg.text = error_text;
    msg.epoch_ms = NowEpochMs();
    msg.timestamp = FormatTime(msg.epoch_ms);
    msg.direction = TyrianIM::MessageDirection::System;
    msg.source = TyrianIM::MessageSource::Whisper;

    TyrianIM::ChatManager::MarkLastOutgoingFailed(account_key);
    TyrianIM::ChatManager::AddMessage(msg);
    TyrianIM::ChatManager::SaveHistory();
    g_ScrollToBottom = true;
}

// Whisper hook callback - called when a whisper is intercepted
// For incoming: sender=accountName (contact key), recipient=characterName
// For outgoing: sender=characterName, recipient=accountName (contact key)
static void OnWhisperIntercepted(const std::string& sender, const std::string& recipient,
                                  const std::string& message, bool is_incoming) {
    // Determine the other person's account name (contact key) and character name
    std::string account_name = is_incoming ? sender : recipient;
    std::string char_name = is_incoming ? recipient : sender;

    // For outgoing whispers, check if TIM already added this message
    if (!is_incoming) {
        bool duplicate = false;
        {
            std::lock_guard<std::mutex> lock(g_LastTIMSendMutex);
            if (!g_LastTIMSendRecipient.empty() && g_LastTIMSendText == message) {
                // TIM already added this outgoing message — skip
                duplicate = true;
                g_LastTIMSendRecipient.clear();
                g_LastTIMSendText.clear();
                g_LastTIMSendTime = 0;
            }
        }
        if (duplicate) {
            // Still update display_name from outgoing event
            auto* convo = TyrianIM::ChatManager::GetConversation(account_name);
            if (convo && !char_name.empty()) {
                convo->display_name = char_name;
            }
            return;
        }
    }

    TyrianIM::ChatMessage msg;
    msg.sender = sender;
    msg.recipient = recipient;
    msg.text = message;
    msg.epoch_ms = NowEpochMs();
    msg.timestamp = FormatTime(msg.epoch_ms);
    msg.source = TyrianIM::MessageSource::Whisper;
    msg.direction = is_incoming ? TyrianIM::MessageDirection::Incoming : TyrianIM::MessageDirection::Outgoing;

    TyrianIM::ChatManager::AddMessage(msg);
    g_ScrollToBottom = true;

    // Set character name as display_name on the conversation
    auto* convo = TyrianIM::ChatManager::GetConversation(account_name);
    if (convo && !char_name.empty()) {
        convo->display_name = char_name;
    }

    // If the window is open and this is the active conversation, suppress notifications
    bool isActiveConvo = g_WindowVisible && (g_SelectedContact == account_name);

    if (isActiveConvo) {
        // Already viewing this conversation — just mark as read, no sound
        TyrianIM::ChatManager::MarkAsRead(account_name);
    } else if (is_incoming) {
        if (g_AutoOpenOnWhisper) {
            g_WindowVisible = true;
            g_SelectedContact = account_name;
            g_FocusInput = true;
            g_ScrollToBottom = true;
            TyrianIM::ChatManager::MarkAsRead(account_name);
        }

        if (g_SoundEnabled) {
            PlayNotificationSound(g_SelectedSound);
        }
    }

    if (APIDefs) {
        std::string log = is_incoming ? "Whisper from " + char_name + " (" + account_name + ")"
                                      : "Whisper to " + char_name + " (" + account_name + ")";
        APIDefs->Log(LOGL_DEBUG, "TyrianIM", log.c_str());
    }
}

// --- Settings persistence ---

static std::string SettingsPath() {
    return g_DataDir + "/settings.ini";
}

static void SaveSettings() {
    if (g_DataDir.empty()) return;
    try { std::filesystem::create_directories(g_DataDir); } catch (...) {}
    std::ofstream f(SettingsPath());
    if (!f.is_open()) return;
    f << "show_floating_icon=" << (g_ShowFloatingIcon ? 1 : 0) << "\n";
    f << "floating_icon_x=" << g_FloatingIconX << "\n";
    f << "floating_icon_y=" << g_FloatingIconY << "\n";
    f << "auto_open_on_whisper=" << (g_AutoOpenOnWhisper ? 1 : 0) << "\n";
    f << "sound_enabled=" << (g_SoundEnabled ? 1 : 0) << "\n";
    f << "selected_sound=" << g_SelectedSound << "\n";
    f << "floating_icon_locked=" << (g_FloatingIconLocked ? 1 : 0) << "\n";
    f << "restore_channel=" << (g_RestoreChannelAfterWhisper ? 1 : 0) << "\n";
    f << "font_scale=" << g_FontScale << "\n";
    f << "icon_scale=" << g_FloatingIconScale << "\n";
}

static void LoadSettings() {
    if (g_DataDir.empty()) return;
    std::ifstream f(SettingsPath());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "show_floating_icon") g_ShowFloatingIcon = (val == "1");
        else if (key == "floating_icon_x") try { g_FloatingIconX = std::stof(val); } catch (...) {}
        else if (key == "floating_icon_y") try { g_FloatingIconY = std::stof(val); } catch (...) {}
        else if (key == "auto_open_on_whisper") g_AutoOpenOnWhisper = (val == "1");
        else if (key == "sound_enabled") g_SoundEnabled = (val == "1");
        else if (key == "floating_icon_locked") g_FloatingIconLocked = (val == "1");
        else if (key == "restore_channel") g_RestoreChannelAfterWhisper = (val == "1");
        else if (key == "font_scale") try { g_FontScale = std::stof(val); if (g_FontScale < 0.8f) g_FontScale = 0.8f; if (g_FontScale > 2.0f) g_FontScale = 2.0f; } catch (...) {}
        else if (key == "icon_scale") try { g_FloatingIconScale = std::stof(val); if (g_FloatingIconScale < 0.5f) g_FloatingIconScale = 0.5f; if (g_FloatingIconScale > 3.0f) g_FloatingIconScale = 3.0f; } catch (...) {}
        else if (key == "selected_sound" || key == "custom_sound_path") {
            g_SelectedSound = val;
        }
    }
    g_FloatingIconPosInitialized = true;
}

// --- Floating notification icon ---

static void RenderFloatingIcon() {
    if (!g_ShowFloatingIcon) return;

    int unread = TyrianIM::ChatManager::GetTotalUnreadCount();
    uint64_t now_ms = NowEpochMs();

    // Flash continuously while there are unread whispers
    bool is_flashing = (unread > 0);
    float flash_alpha = 0.0f;
    if (is_flashing) {
        float t = (float)(now_ms % 1000) / 1000.0f;
        flash_alpha = 0.5f + 0.5f * sinf(t * 2.0f * 3.14159f);
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoBackground;
    if (g_FloatingIconLocked) {
        flags |= ImGuiWindowFlags_NoMove;
    }

    if (!g_FloatingIconPosInitialized) {
        ImGui::SetNextWindowPos(ImVec2(g_FloatingIconX, g_FloatingIconY), ImGuiCond_FirstUseEver);
        g_FloatingIconPosInitialized = true;
    }

    // Fully transparent window — bubble is drawn directly, no window background
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    float s = g_FloatingIconScale;
    ImGui::SetNextWindowSize(ImVec2(60 * s, 58 * s));
    ImGui::Begin("##TyrianIM_FloatingIcon", nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // Speech bubble colors — white fill, black border
    ImU32 bubbleCol, borderCol;
    borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    if (is_flashing) {
        float v = 0.85f + 0.15f * flash_alpha;
        bubbleCol = ImGui::ColorConvertFloat4ToU32(ImVec4(v, v, v, 0.95f));
    } else {
        bubbleCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.95f));
    }

    // Draw speech bubble with black border (draw black shape behind, then blue on top)
    float bd = 2.0f * s; // border thickness
    float bx0 = wp.x + 6 * s, by0 = wp.y + 6 * s;
    float bx1 = wp.x + 53 * s, by1 = wp.y + 38 * s;

    // Speech bubble tail points
    ImVec2 t1(wp.x + 20 * s, by1 - 1 * s);
    ImVec2 t2(wp.x + 28 * s, by1 - 1 * s);
    ImVec2 t3(wp.x + 18 * s, wp.y + 52 * s);

    // Black border layer (slightly expanded)
    dl->AddRectFilled(ImVec2(bx0 - bd, by0 - bd), ImVec2(bx1 + bd, by1 + bd), borderCol, 14.0f * s + bd);
    ImVec2 bt1(t1.x - bd, t1.y);
    ImVec2 bt2(t2.x + bd, t2.y);
    ImVec2 bt3(t3.x - bd, t3.y + bd);
    dl->AddTriangleFilled(bt1, bt2, bt3, borderCol);

    // Blue fill layer (normal size)
    dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1, by1), bubbleCol, 14.0f * s);
    dl->AddTriangleFilled(t1, t2, t3, bubbleCol);

    // Draw text inside bubble
    std::string label;
    ImVec4 textCol;
    if (unread > 0) {
        label = std::to_string(unread);
        textCol = ImVec4(0.1f, 0.15f, 0.4f, 1.0f);
    } else {
        label = "TIM";
        textCol = ImVec4(0.1f, 0.15f, 0.4f, 1.0f);
    }

    // Center text in the bubble (larger font for unread count)
    ImFont* font = ImGui::GetFont();
    float fontSize = ((unread > 0) ? font->FontSize * 1.5f : font->FontSize) * s;
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str());
    float cx = bx0 + (bx1 - bx0 - textSize.x) * 0.5f;
    float cy = by0 + (by1 - by0 - textSize.y) * 0.5f;
    dl->AddText(font, fontSize, ImVec2(cx, cy), ImGui::ColorConvertFloat4ToU32(textCol), label.c_str());

    // Click to toggle main window
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        g_WindowVisible = !g_WindowVisible;
        if (g_WindowVisible) {
            if (unread > 0) {
                auto convos = TyrianIM::ChatManager::GetConversations();
                for (auto* c : convos) {
                    if (c->unread_count > 0) {
                        g_SelectedContact = c->contact;
                        TyrianIM::ChatManager::MarkAsRead(c->contact);
                        g_ScrollToBottom = true;
                        break;
                    }
                }
            }
            g_FocusInput = true;
        }
    }

    // Save position if dragged
    ImVec2 pos = ImGui::GetWindowPos();
    if (pos.x != g_FloatingIconX || pos.y != g_FloatingIconY) {
        g_FloatingIconX = pos.x;
        g_FloatingIconY = pos.y;
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

// Forward declarations
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
void AddonRender();
void AddonOptions();

// --- Render helpers ---

static void RenderContactList(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
    ImGui::BeginChild("##Contacts", ImVec2(width, 0), true);

    ImFont* font = ImGui::GetFont();
    float fs = font->FontSize * g_FontScale;

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.9f, 1.0f), "  CONVERSATIONS");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto conversations = TyrianIM::ChatManager::GetConversations();

    if (conversations.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("No conversations yet.");
        ImGui::Spacing();
        ImGui::TextColored(COLOR_TIMESTAMP, "Whispers will appear here.");
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (const auto* convo : conversations) {
        bool is_selected = (g_SelectedContact == convo->contact);
        bool has_unread = (convo->unread_count > 0);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float itemHeight = 30.0f + 18.0f * g_FontScale;

        if (is_selected) {
            dl->AddRectFilled(
                ImVec2(cursor.x, cursor.y),
                ImVec2(cursor.x + width - 16, cursor.y + itemHeight),
                COLOR_ACTIVE_BG, 6.0f);
        }

        if (ImGui::Selectable(("##contact_" + convo->contact).c_str(), false, 0, ImVec2(0, itemHeight))) {
            g_SelectedContact = convo->contact;
            TyrianIM::ChatManager::MarkAsRead(convo->contact);
            g_FocusInput = true;
            g_ScrollToBottom = true;
        }

        // Right-click context menu
        std::string popupId = "##ctx_" + convo->contact;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup(popupId.c_str());
        }
        if (ImGui::BeginPopup(popupId.c_str())) {
            if (ImGui::MenuItem("Delete conversation")) {
                g_PendingDeleteContact = convo->contact;
                ImGui::OpenPopup("Confirm Delete##ConfirmDel");
            }
            ImGui::EndPopup();
        }

        // Avatar circle with initial
        float avatarRadius = 10.0f + 6.0f * g_FontScale;
        ImVec2 avatarCenter(cursor.x + 8 + avatarRadius, cursor.y + itemHeight * 0.5f);
        unsigned int nameHash = 0;
        for (char c : convo->contact) nameHash = nameHash * 31 + (unsigned char)c;
        float hue = (nameHash % 360) / 360.0f;
        ImVec4 avatarColor;
        ImGui::ColorConvertHSVtoRGB(hue, 0.5f, 0.7f, avatarColor.x, avatarColor.y, avatarColor.z);
        avatarColor.w = 1.0f;
        dl->AddCircleFilled(avatarCenter, avatarRadius, ImGui::ColorConvertFloat4ToU32(avatarColor));

        // Initial letter (scaled)
        char initial[2] = { 0, 0 };
        std::string displayStr = !convo->display_name.empty() ? convo->display_name : convo->contact;
        if (!displayStr.empty()) initial[0] = (char)toupper(displayStr[0]);
        ImVec2 initSize = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, initial);
        dl->AddText(font, fs,
            ImVec2(avatarCenter.x - initSize.x * 0.5f, avatarCenter.y - initSize.y * 0.5f),
            IM_COL32(255, 255, 255, 230), initial);

        // Unread dot on avatar
        if (has_unread) {
            dl->AddCircleFilled(
                ImVec2(avatarCenter.x + avatarRadius - 3, avatarCenter.y - avatarRadius + 3),
                7.0f, COLOR_UNREAD_DOT);
            if (convo->unread_count < 10) {
                std::string cnt = std::to_string(convo->unread_count);
                float dotFs = font->FontSize * 0.9f;
                ImVec2 cntSize = font->CalcTextSizeA(dotFs, FLT_MAX, 0.0f, cnt.c_str());
                dl->AddText(font, dotFs,
                    ImVec2(avatarCenter.x + avatarRadius - 3 - cntSize.x * 0.5f,
                           avatarCenter.y - avatarRadius + 3 - cntSize.y * 0.5f),
                    IM_COL32(255, 255, 255, 255), cnt.c_str());
            }
        }

        // Text content (scaled)
        float textX = cursor.x + 8 + avatarRadius * 2 + 10;
        ImVec4 nameColor = has_unread ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                                      : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        std::string primaryName = !convo->display_name.empty() ? convo->display_name : convo->contact;
        dl->AddText(font, fs, ImVec2(textX, cursor.y + 6),
            ImGui::ColorConvertFloat4ToU32(nameColor), primaryName.c_str());

        // Subtitle
        if (!convo->display_name.empty() && convo->display_name != convo->contact) {
            float subFs = font->FontSize * (g_FontScale * 0.85f);
            dl->AddText(font, subFs, ImVec2(textX, cursor.y + 6 + fs + 2),
                ImGui::ColorConvertFloat4ToU32(COLOR_TIMESTAMP), convo->contact.c_str());
        }
    }

    // Confirmation modal (rendered inside the Contacts child window)
    if (!g_PendingDeleteContact.empty()) {
        ImGui::OpenPopup("Confirm Delete");
    }
    if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Delete conversation with");
        ImGui::TextColored(COLOR_OTHER, "%s", g_PendingDeleteContact.c_str());
        ImGui::Text("This will also delete the chat history file.");
        ImGui::Spacing();
        if (ImGui::Button("Yes", ImVec2(80, 0))) {
            if (g_SelectedContact == g_PendingDeleteContact) {
                g_SelectedContact.clear();
            }
            g_ClipboardMsg = "Conversation deleted";
            g_ClipboardMsgExpiry = ImGui::GetTime() + 2.0;
            TyrianIM::ChatManager::DeleteConversation(g_PendingDeleteContact);
            g_PendingDeleteContact.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(80, 0))) {
            g_PendingDeleteContact.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void RenderMessageArea() {
    ImGui::BeginChild("##ChatArea", ImVec2(0, 0));

    if (g_SelectedContact.empty()) {
        // No conversation selected - show welcome
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPosY(avail.y * 0.35f);

        const char* title = "Tyrian Instant Messaging";
        float title_w = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX((avail.x - title_w) * 0.5f);
        ImGui::TextColored(COLOR_SELF, "%s", title);

        ImGui::Spacing();
        const char* subtitle = "Select a conversation or wait for messages";
        float sub_w = ImGui::CalcTextSize(subtitle).x;
        ImGui::SetCursorPosX((avail.x - sub_w) * 0.5f);
        ImGui::TextColored(COLOR_TIMESTAMP, "%s", subtitle);

        ImGui::Spacing();
        ImGui::Spacing();
        auto status = TyrianIM::WhisperHook::GetStatus();
        const char* status_label = "Whisper Hook: ";
        float status_w = ImGui::CalcTextSize(status_label).x + ImGui::CalcTextSize(TyrianIM::WhisperHook::GetStatusString()).x;
        ImGui::SetCursorPosX((avail.x - status_w) * 0.5f);
        ImGui::Text("%s", status_label);
        ImGui::SameLine();
        ImVec4 status_color = (status == TyrianIM::HookStatus::Hooked) ? COLOR_STATUS_OK
                            : (status == TyrianIM::HookStatus::Failed) ? COLOR_STATUS_ERR
                            : COLOR_STATUS_WARN;
        ImGui::TextColored(status_color, "%s", TyrianIM::WhisperHook::GetStatusString());

    } else {
        auto* convo = TyrianIM::ChatManager::GetConversation(g_SelectedContact);
        if (!convo) {
            ImGui::Text("Conversation not found.");
            ImGui::EndChild();
            return;
        }

        // --- Styled header bar ---
        {
            ImFont* font = ImGui::GetFont();
            float fs = font->FontSize * g_FontScale;
            float subFs = font->FontSize * (g_FontScale * 0.85f);

            ImDrawList* hdl = ImGui::GetWindowDrawList();
            ImVec2 hPos = ImGui::GetCursorScreenPos();
            float headerH = 28.0f + 16.0f * g_FontScale;
            hdl->AddRectFilled(hPos, ImVec2(hPos.x + ImGui::GetContentRegionAvail().x, hPos.y + headerH),
                COLOR_HEADER_BG, 4.0f);

            // Avatar in header
            float hAvatarR = 8.0f + 6.0f * g_FontScale;
            ImVec2 hAvatarC(hPos.x + 14 + hAvatarR, hPos.y + headerH * 0.5f);
            unsigned int hHash = 0;
            for (char c : convo->contact) hHash = hHash * 31 + (unsigned char)c;
            float hHue = (hHash % 360) / 360.0f;
            ImVec4 hAvatarCol;
            ImGui::ColorConvertHSVtoRGB(hHue, 0.5f, 0.7f, hAvatarCol.x, hAvatarCol.y, hAvatarCol.z);
            hAvatarCol.w = 1.0f;
            hdl->AddCircleFilled(hAvatarC, hAvatarR, ImGui::ColorConvertFloat4ToU32(hAvatarCol));

            // Initial in avatar
            std::string hDisplay = !convo->display_name.empty() ? convo->display_name : convo->contact;
            char hInit[2] = { (char)toupper(hDisplay[0]), 0 };
            ImVec2 hInitSz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, hInit);
            hdl->AddText(font, fs,
                ImVec2(hAvatarC.x - hInitSz.x * 0.5f, hAvatarC.y - hInitSz.y * 0.5f),
                IM_COL32(255, 255, 255, 230), hInit);

            // Name text
            float hTextX = hPos.x + 14 + hAvatarR * 2 + 10;
            std::string hName = !convo->display_name.empty() ? convo->display_name : convo->contact;
            hdl->AddText(font, fs, ImVec2(hTextX, hPos.y + 6),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), hName.c_str());
            // Account name subtitle
            if (!convo->display_name.empty() && convo->display_name != convo->contact) {
                hdl->AddText(font, subFs, ImVec2(hTextX, hPos.y + 6 + fs + 2),
                    ImGui::ColorConvertFloat4ToU32(COLOR_TIMESTAMP), convo->contact.c_str());
            }

            ImGui::Dummy(ImVec2(0, headerH + 4));
        }

        // --- Messages area ---
        float input_height = ImGui::GetFrameHeightWithSpacing() + 8;
        ImGui::BeginChild("##Messages", ImVec2(0, -input_height), false);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        float fs = font->FontSize * g_FontScale;
        float areaWidth = ImGui::GetContentRegionAvail().x;
        float maxBubbleWidth = areaWidth * 0.75f;
        float padding = 10.0f;
        float bubbleRound = 10.0f;

        ImGui::Spacing();

        for (const auto& msg : convo->messages) {
            // System/error messages — centered, distinct style
            if (msg.direction == TyrianIM::MessageDirection::System) {
                float sysFs = font->FontSize * (g_FontScale * 0.85f);
                ImVec2 sysMsgSize = font->CalcTextSizeA(sysFs, FLT_MAX, areaWidth * 0.8f, msg.text.c_str());
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                float sysW = sysMsgSize.x + padding * 2;
                float sysH = sysMsgSize.y + padding * 2;
                float sysX = cursor.x + (areaWidth - sysW) * 0.5f;
                dl->AddRectFilled(
                    ImVec2(sysX, cursor.y), ImVec2(sysX + sysW, cursor.y + sysH),
                    IM_COL32(80, 20, 20, 180), 8.0f);
                font->RenderText(dl, sysFs,
                    ImVec2(sysX + padding, cursor.y + padding),
                    IM_COL32(255, 120, 120, 255),
                    ImVec4(sysX + padding, cursor.y + padding, sysX + sysW - padding, cursor.y + sysH),
                    msg.text.c_str(), msg.text.c_str() + msg.text.size(), areaWidth * 0.8f);
                ImGui::Dummy(ImVec2(0, sysH + 6));
                continue;
            }

            bool is_self = (msg.direction == TyrianIM::MessageDirection::Outgoing);

            // Build sender label
            std::string senderLabel;
            if (is_self) {
                senderLabel = "You";
            } else {
                senderLabel = (!convo->display_name.empty()) ? convo->display_name : msg.sender;
            }

            // Calculate text sizes with scaled font
            ImVec2 nameSize = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, senderLabel.c_str());
            float timeFs = font->FontSize * (g_FontScale * 0.85f);
            ImVec2 timeSize = font->CalcTextSizeA(timeFs, FLT_MAX, 0.0f, msg.timestamp.c_str());
            float textWrapWidth = maxBubbleWidth - padding * 2;
            ImVec2 msgSize = font->CalcTextSizeA(fs, FLT_MAX, textWrapWidth, msg.text.c_str());

            // Bubble dimensions
            float bubbleW = (std::max)(nameSize.x + timeSize.x + 20, msgSize.x) + padding * 2;
            if (bubbleW > maxBubbleWidth) bubbleW = maxBubbleWidth;
            float bubbleH = nameSize.y + msgSize.y + padding * 2 + 4;

            // Position: right-aligned for self, left-aligned for other
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            float bubbleX = is_self ? (cursor.x + areaWidth - bubbleW - 8) : (cursor.x + 8);

            // Draw bubble
            ImU32 bubbleCol = is_self ? COLOR_BUBBLE_SELF : COLOR_BUBBLE_OTHER;
            dl->AddRectFilled(
                ImVec2(bubbleX, cursor.y),
                ImVec2(bubbleX + bubbleW, cursor.y + bubbleH),
                bubbleCol, bubbleRound);

            // Failed message indicator: red border + red "!" to the left
            if (msg.failed) {
                dl->AddRect(
                    ImVec2(bubbleX, cursor.y),
                    ImVec2(bubbleX + bubbleW, cursor.y + bubbleH),
                    IM_COL32(220, 40, 40, 255), bubbleRound, 0, 2.0f);
                const char* failIcon = "!";
                ImVec2 failSize = font->CalcTextSizeA(fs * 1.2f, FLT_MAX, 0.0f, failIcon);
                float failX = bubbleX - failSize.x - 6;
                float failY = cursor.y + (bubbleH - failSize.y) * 0.5f;
                dl->AddText(font, fs * 1.2f, ImVec2(failX, failY),
                    IM_COL32(220, 40, 40, 255), failIcon);
            }

            // Sender name (left of bubble)
            ImVec4 nameCol = is_self ? COLOR_SELF : COLOR_OTHER;
            dl->AddText(font, fs, ImVec2(bubbleX + padding, cursor.y + padding),
                ImGui::ColorConvertFloat4ToU32(nameCol), senderLabel.c_str());

            // Timestamp (right of name line, slightly smaller)
            dl->AddText(font, timeFs,
                ImVec2(bubbleX + bubbleW - padding - timeSize.x, cursor.y + padding + (nameSize.y - timeSize.y)),
                ImGui::ColorConvertFloat4ToU32(COLOR_TIMESTAMP), msg.timestamp.c_str());

            // Message text (wrapped)
            float textY = cursor.y + padding + nameSize.y + 4;
            font->RenderText(dl, fs,
                ImVec2(bubbleX + padding, textY),
                IM_COL32(220, 220, 220, 255),
                ImVec4(bubbleX + padding, textY, bubbleX + bubbleW - padding, textY + msgSize.y + 10),
                msg.text.c_str(), msg.text.c_str() + msg.text.size(), textWrapWidth);

            // Click bubble to copy message
            ImGui::SetCursorScreenPos(ImVec2(bubbleX, cursor.y));
            if (ImGui::InvisibleButton(("##bubble_" + std::to_string(msg.epoch_ms)).c_str(),
                                       ImVec2(bubbleW, bubbleH))) {
                ImGui::SetClipboardText(msg.text.c_str());
                g_ClipboardMsg = "Copied to clipboard";
                g_ClipboardMsgExpiry = ImGui::GetTime() + 2.0;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (msg.failed) {
                    ImGui::SetTooltip("Message not delivered — player is offline");
                }
            }

            // Advance cursor
            ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + bubbleH + 6));
        }

        if (g_ScrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            g_ScrollToBottom = false;
        }

        ImGui::EndChild();

        // --- Input area with background ---
        {
            ImDrawList* idl = ImGui::GetWindowDrawList();
            ImVec2 iPos = ImGui::GetCursorScreenPos();
            float iH = ImGui::GetFrameHeightWithSpacing() + 4;
            idl->AddRectFilled(iPos, ImVec2(iPos.x + ImGui::GetContentRegionAvail().x, iPos.y + iH),
                COLOR_INPUT_BG, 4.0f);
        }

        bool send = false;
        bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        bool noItemActive = !ImGui::IsAnyItemActive();
        if (g_FocusInput && windowHovered && !TyrianIM::WhisperHook::IsSending() && !TyrianIM::WhisperHook::IsRestoring()) {
            ImGui::SetKeyboardFocusHere();
            g_FocusInput = false;
        }

        bool sending = TyrianIM::WhisperHook::IsSending() || TyrianIM::WhisperHook::IsRestoring();
        if (windowHovered && !sending) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
            ImGui::PushItemWidth(-60);
            if (ImGui::InputText("##MsgInput", g_InputBuf, sizeof(g_InputBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                send = true;
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
            if (ImGui::Button("Send", ImVec2(52, 0))) {
                send = true;
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        } else {
            // No input widget when not hovered
            float w = ImGui::GetContentRegionAvail().x - 60;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
            ImGui::BeginChild("##MsgPlaceholder", ImVec2(w, ImGui::GetFrameHeight()), false);
            ImGui::TextColored(COLOR_TIMESTAMP, "%s", g_InputBuf[0] ? g_InputBuf : "");
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::Button("Send", ImVec2(52, 0));
            ImGui::PopStyleVar(2);
        }

        if (send && g_InputBuf[0] != '\0' && !TyrianIM::WhisperHook::IsSending()) {
            TyrianIM::ChatMessage msg;
            msg.sender = TyrianIM::ChatManager::GetSelfAccountName();
            msg.recipient = g_SelectedContact;
            msg.text = g_InputBuf;
            msg.epoch_ms = NowEpochMs();
            msg.timestamp = FormatTime(msg.epoch_ms);
            msg.direction = TyrianIM::MessageDirection::Outgoing;
            msg.source = TyrianIM::MessageSource::Whisper;

            TyrianIM::ChatManager::AddMessage(msg);
            g_ScrollToBottom = true;

            // Track this send so the outgoing game event is deduplicated
            {
                std::lock_guard<std::mutex> lock(g_LastTIMSendMutex);
                g_LastTIMSendRecipient = g_SelectedContact;
                g_LastTIMSendText = g_InputBuf;
                g_LastTIMSendTime = NowEpochMs();
            }

            g_InputBuf[0] = '\0';

            // Send the whisper in-game via /w CharacterName
            // g_SelectedContact is the account name; use display_name (character name) for /w
            std::string whisper_target = (convo && !convo->display_name.empty())
                ? convo->display_name : g_SelectedContact;
            if (APIDefs) {
                APIDefs->Log(LOGL_DEBUG, "TyrianIM",
                    ("SendWhisper: contact='" + g_SelectedContact +
                     "' display_name='" + (convo ? convo->display_name : "(no convo)") +
                     "' target='" + whisper_target +
                     "' text='" + msg.text + "'").c_str());
            }
            bool sent = TyrianIM::WhisperHook::SendWhisper(whisper_target, msg.text, g_RestoreChannelAfterWhisper);
            if (APIDefs) {
                APIDefs->Log(LOGL_DEBUG, "TyrianIM",
                    sent ? "SendWhisper returned TRUE" : "SendWhisper returned FALSE");
            }

            // Defer input focus until send completes (so PostMessage keys aren't captured)
            g_FocusInput = true;
        }
    }

    ImGui::EndChild();
}

// Main render function
void AddonRender() {
    // Check if the player is logged into a character (not on character select)
    bool isInGame = false;
    if (APIDefs) {
        NexusLinkData_t* nexusLink = (NexusLinkData_t*)APIDefs->DataLink_Get(DL_NEXUS_LINK);
        isInGame = nexusLink && nexusLink->IsGameplay;
    }

    if (!isInGame) {
        // On character select — hide window and icon
        g_WindowVisible = false;
        return;
    }

    // Check for whisper send timeout — if echo not received within 5s, mark as failed
    {
        std::lock_guard<std::mutex> lock(g_LastTIMSendMutex);
        if (!g_LastTIMSendRecipient.empty() && g_LastTIMSendTime > 0) {
            uint64_t elapsed = NowEpochMs() - g_LastTIMSendTime;
            if (elapsed > 5000) {
                std::string failContact = g_LastTIMSendRecipient;
                g_LastTIMSendRecipient.clear();
                g_LastTIMSendText.clear();
                g_LastTIMSendTime = 0;

                TyrianIM::ChatManager::MarkLastOutgoingFailed(failContact);
                TyrianIM::ChatManager::SaveHistory();
                g_ScrollToBottom = true;
            }
        }
    }

    // Always render floating icon (independent of main window)
    RenderFloatingIcon();

    if (!g_WindowVisible) return;

    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 350), ImVec2(1200, 900));
    if (!ImGui::Begin("Tyrian Instant Messenger##TyrianIM", &g_WindowVisible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::SetWindowFontScale(g_FontScale);

    // Status bar at top
    auto hook_status = TyrianIM::WhisperHook::GetStatus();
    int unread = TyrianIM::ChatManager::GetTotalUnreadCount();

    if (unread > 0) {
        ImGui::TextColored(COLOR_UNREAD, "%d unread", unread);
        ImGui::SameLine();
    }

    ImGui::TextColored(COLOR_TIMESTAMP, "Hook:");
    ImGui::SameLine();
    ImVec4 sc = (hook_status == TyrianIM::HookStatus::Hooked) ? COLOR_STATUS_OK
              : (hook_status == TyrianIM::HookStatus::Failed) ? COLOR_STATUS_ERR
              : COLOR_STATUS_WARN;
    ImGui::TextColored(sc, "%s", TyrianIM::WhisperHook::GetStatusString());
    if (hook_status != TyrianIM::HookStatus::Hooked) {
        ImGui::SameLine();
        ImGui::TextColored(COLOR_TIMESTAMP, "| Install 'Events: Chat' addon");
    } else if (TyrianIM::WhisperHook::GetProbeMode()) {
        ImGui::SameLine();
        ImGui::TextColored(COLOR_STATUS_WARN, "| PROBE MODE");
    }
    if (hook_status == TyrianIM::HookStatus::Hooked) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Debug")) {
            TyrianIM::WhisperHook::s_ShowDebugWindow = !TyrianIM::WhisperHook::s_ShowDebugWindow;
        }
    }

    ImGui::Separator();

    // Reserve space for the fixed status bar at the bottom
    float statusBarHeight = ImGui::GetFrameHeightWithSpacing() + 2.0f;

    // Two-panel layout: contact list | message area (fills remaining space above status bar)
    float contact_width = 230.0f;
    ImGui::BeginChild("##MainContent", ImVec2(0, -statusBarHeight), false, ImGuiWindowFlags_NoScrollbar);
    RenderContactList(contact_width);
    ImGui::SameLine();
    RenderMessageArea();
    ImGui::EndChild();

    // Status bar at bottom (always visible, full width, fixed position)
    ImGui::Separator();
    if (!g_ClipboardMsg.empty() && ImGui::GetTime() < g_ClipboardMsgExpiry) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", g_ClipboardMsg.c_str());
    } else {
        g_ClipboardMsg.clear();
        ImGui::TextColored(COLOR_TIMESTAMP, " ");
    }

    ImGui::End();

    // Render debug window (separate floating window)
    TyrianIM::WhisperHook::RenderDebugWindow();
}

// Options panel (shown in Nexus addon settings)
void AddonOptions() {
    ImGui::Text("Tyrian Instant Messaging");
    ImGui::Separator();

    // --- Status ---
    ImGui::Text("Chat Events Status: ");
    ImGui::SameLine();
    auto status = TyrianIM::WhisperHook::GetStatus();
    ImVec4 sc = (status == TyrianIM::HookStatus::Hooked) ? COLOR_STATUS_OK
              : (status == TyrianIM::HookStatus::Failed) ? COLOR_STATUS_ERR
              : COLOR_STATUS_WARN;
    ImGui::TextColored(sc, "%s", TyrianIM::WhisperHook::GetStatusString());

    // --- Notifications ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Notifications");
    ImGui::Spacing();

    bool settings_changed = false;

    if (ImGui::Checkbox("Show floating notification icon", &g_ShowFloatingIcon)) {
        settings_changed = true;
    }
    if (g_ShowFloatingIcon) {
        ImGui::SameLine();
        if (ImGui::Checkbox("Lock position", &g_FloatingIconLocked)) {
            settings_changed = true;
        }
    }
    ImGui::TextColored(COLOR_TIMESTAMP, "  %s", g_FloatingIconLocked ? "Position locked. Uncheck 'Lock position' to drag." : "Drag the icon to reposition it.");
    if (g_ShowFloatingIcon) {
        ImGui::SetNextItemWidth(150);
        if (ImGui::SliderFloat("Icon size", &g_FloatingIconScale, 0.5f, 3.0f, "%.1fx")) {
            settings_changed = true;
        }
    }

    if (ImGui::Checkbox("Auto-open window on incoming whisper", &g_AutoOpenOnWhisper)) {
        settings_changed = true;
    }

    if (ImGui::Checkbox("Restore chat channel after sending whisper", &g_RestoreChannelAfterWhisper)) {
        settings_changed = true;
    }
    ImGui::TextColored(COLOR_TIMESTAMP, "  Waits for keyboard idle then switches back to previous channel");

    ImGui::Spacing();
    if (ImGui::SliderFloat("Font Scale", &g_FontScale, 0.8f, 2.0f, "%.1f")) {
        settings_changed = true;
    }
    ImGui::TextColored(COLOR_TIMESTAMP, "  Adjusts text size in the TIM window (default: 1.0)");

    ImGui::Spacing();
    if (ImGui::Checkbox("Play sound on incoming whisper", &g_SoundEnabled)) {
        settings_changed = true;
    }

    if (g_SoundEnabled) {
        ImGui::Indent();
        ImGui::Text("Notification sound:");
        ImGui::SameLine();
        ImGui::PushItemWidth(220);
        // Build preview label
        const char* preview = g_SelectedSound.empty() ? "(Default system sound)" : g_SelectedSound.c_str();
        if (ImGui::BeginCombo("##SoundSelect", preview)) {
            // Default option
            if (ImGui::Selectable("(Default system sound)", g_SelectedSound.empty())) {
                g_SelectedSound.clear();
                settings_changed = true;
            }
            // List .wav files from sounds/ directory
            for (const auto& file : g_SoundFiles) {
                bool selected = (g_SelectedSound == file);
                if (ImGui::Selectable(file.c_str(), selected)) {
                    g_SelectedSound = file;
                    settings_changed = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) {
            ScanSoundFiles();
        }
        if (ImGui::SmallButton("Test Sound")) {
            PlayNotificationSound(g_SelectedSound);
        }
        ImGui::SameLine();
        ImGui::TextColored(COLOR_TIMESTAMP, "Place .wav/.mp3 files in: addons/TyrianIM/sounds/");
        ImGui::Unindent();
    }

    // --- Debug ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Debug");
    ImGui::Spacing();

    if (status == TyrianIM::HookStatus::Hooked) {
        bool probe = TyrianIM::WhisperHook::GetProbeMode();
        if (ImGui::Checkbox("Probe Mode (dumps raw event data to debug window)", &probe)) {
            TyrianIM::WhisperHook::SetProbeMode(probe);
        }
        if (probe) {
            ImGui::TextColored(COLOR_STATUS_WARN,
                "Probe active: chat messages will dump raw data to the debug window.");
        }
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Requires 'Events: Chat' addon from the Nexus library for whisper interception.");

    ImGui::Spacing();
    if (ImGui::Button("Clear Chat History")) {
        TyrianIM::ChatManager::Shutdown();
        std::string dir = APIDefs ? std::string(APIDefs->Paths_GetAddonDirectory("TyrianIM")) : "";
        TyrianIM::ChatManager::Initialize(dir);
        g_SelectedContact.clear();
    }

    if (settings_changed) {
        SaveSettings();
    }
}

// Addon lifecycle
void AddonLoad(AddonAPI_t* aApi) {
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void(*)(void*, void*))APIDefs->ImguiFree);

    // Initialize data directory
    g_DataDir = std::string(APIDefs->Paths_GetAddonDirectory("TyrianIM"));

    // Load settings first (icon position, sound prefs, etc.)
    LoadSettings();
    ScanSoundFiles();

    // Initialize subsystems
    TyrianIM::ChatManager::Initialize(g_DataDir);
    TyrianIM::ChatManager::LoadHistory();

    TyrianIM::WhisperHook::Initialize(APIDefs);
    TyrianIM::WhisperHook::SetCallback(OnWhisperIntercepted);
    TyrianIM::WhisperHook::SetErrorCallback(OnWhisperError);

    // Attempt to install whisper hooks (will log warning that patterns aren't defined yet)
    TyrianIM::WhisperHook::InstallHooks();

    // Subscribe to squad/party chat events from Unofficial Extras (testbed)
    APIDefs->Events_Subscribe(EV_UNOFFICIAL_EXTRAS_CHAT_MESSAGE, OnChatMessage);

    // Register render callbacks
    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);

    // Register close-on-escape
    APIDefs->GUI_RegisterCloseOnEscape("Tyrian IM##TyrianIM", &g_WindowVisible);

    // Register keybind
    APIDefs->InputBinds_RegisterWithString("KB_TYRIANIM_TOGGLE", ProcessKeybind, "CTRL+SHIFT+M");

    // Load placeholder icon textures
    APIDefs->Textures_LoadFromMemory(TEX_ICON, (void*)ICON_DATA, ICON_DATA_SIZE, nullptr);
    APIDefs->Textures_LoadFromMemory(TEX_ICON_HOVER, (void*)ICON_DATA, ICON_DATA_SIZE, nullptr);

    // Register quick access shortcut
    APIDefs->QuickAccess_Add(QA_ID, TEX_ICON, TEX_ICON_HOVER, "KB_TYRIANIM_TOGGLE", "Tyrian IM");

    APIDefs->Log(LOGL_INFO, "TyrianIM", "Tyrian Instant Messaging loaded");
}

void AddonUnload() {
    // Unsubscribe from events
    APIDefs->Events_Unsubscribe(EV_UNOFFICIAL_EXTRAS_CHAT_MESSAGE, OnChatMessage);

    // Deregister UI
    APIDefs->GUI_DeregisterCloseOnEscape("Tyrian IM##TyrianIM");
    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->InputBinds_Deregister("KB_TYRIANIM_TOGGLE");
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->GUI_Deregister(AddonRender);

    // Save settings (icon position, sound prefs, etc.)
    SaveSettings();

    // Shutdown subsystems
    TyrianIM::WhisperHook::Shutdown();
    TyrianIM::ChatManager::Shutdown();

    APIDefs->Log(LOGL_INFO, "TyrianIM", "Tyrian Instant Messaging unloaded");
    APIDefs = nullptr;
}

void ProcessKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;

    if (strcmp(aIdentifier, "KB_TYRIANIM_TOGGLE") == 0) {
        g_WindowVisible = !g_WindowVisible;
        if (g_WindowVisible && !g_SelectedContact.empty()) {
            g_FocusInput = true;
        }
    }
}

// Export function
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature = 0x584b6b2f;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Tyrian IM";
    AddonDef.Version.Major = V_MAJOR;
    AddonDef.Version.Minor = V_MINOR;
    AddonDef.Version.Build = V_BUILD;
    AddonDef.Version.Revision = V_REVISION;
    AddonDef.Author = "TyrianIM";
    AddonDef.Description = "Instant messaging window for Guild Wars 2 whispers";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_None;
    AddonDef.UpdateLink = nullptr;

    return &AddonDef;
}
