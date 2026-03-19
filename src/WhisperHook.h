#ifndef WHISPERHOOK_H
#define WHISPERHOOK_H

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include "../include/nexus/Nexus.h"

// ============================================================================
// GW2-Chat event definitions (from jsantorek/GW2-Chat/src/Chat.h)
// Event name: "EV_CHAT:Message", payload: ChatMessage*
// ============================================================================
#define GW2_CHAT_EVENT "EV_CHAT:Message"

typedef char* ChatStringUTF8;

typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} ChatGUID;

typedef struct {
    uint32_t Low;
    uint32_t High;
} ChatTimestamp; // windows FILETIME

enum ChatMessageType {
    ChatMsg_Error = 0,
    ChatMsg_Guild,
    ChatMsg_GuildMotD,
    ChatMsg_Local,
    ChatMsg_Map,
    ChatMsg_Party,
    ChatMsg_Squad,
    ChatMsg_SquadMessage,
    ChatMsg_SquadBroadcast,
    ChatMsg_TeamPvP,
    ChatMsg_TeamWvW,
    ChatMsg_Whisper,
    ChatMsg_Emote,
    ChatMsg_EmoteCustom
};

enum ChatMetadataFlags {
    ChatFlag_None = 0,
    ChatFlag_Guild_IsRepresented  = 1 << 0,
    ChatFlag_Squad_IsFromCommander = 1 << 1,
    ChatFlag_Local_IsFromMentor   = 1 << 2,
    ChatFlag_SquadMessage_IsBroadcast = 1 << 3,
    ChatFlag_Whisper_IsFromMe     = 1 << 4,
};

typedef struct {
    ChatGUID       Account;
    ChatStringUTF8 CharacterName;
    ChatStringUTF8 AccountName; // nullptr if unavailable
    ChatStringUTF8 Content;
} ChatGenericMessage;

typedef struct {
    ChatTimestamp       DateTime;
    ChatMessageType     Type;
    ChatMetadataFlags   Flags;
    union {
        ChatGenericMessage Whisper;
        ChatGenericMessage Local;
        ChatGenericMessage Map;
        ChatGenericMessage Party;
        ChatGenericMessage Squad;
        ChatStringUTF8     SquadMessage;
        ChatGenericMessage TeamPvP;
        ChatGenericMessage ErrorGeneric; // Error as GenericMessage (fallback)
        // Other types omitted for brevity
    };
} ChatMessage;

namespace TyrianIM {

// Callback signature for intercepted whisper messages
// sender: account or character name, message: whisper text, is_incoming: true = received, false = sent
using WhisperCallback = std::function<void(const std::string& sender, const std::string& recipient, const std::string& message, bool is_incoming)>;
using ErrorCallback = std::function<void(const std::string& contact, const std::string& error_text)>;

// Status of the whisper hook system
enum class HookStatus {
    NotInitialized,     // Haven't attempted yet
    Scanning,           // Subscribing to events
    Hooked,             // Listening for Nexus chat events
    Failed,             // Subscription failed
    Disabled            // Unsubscribed
};

class WhisperHook {
public:
    // Initialize with the Nexus API
    static void Initialize(AddonAPI_t* api);

    // Subscribe to Nexus chat events (from "Events: Chat" addon and others)
    // Returns true if subscriptions were set up
    static bool InstallHooks();

    // Unsubscribe from all events
    static void RemoveHooks();

    // Send a whisper in-game by simulating chat input: /w recipient message
    static bool SendWhisper(const std::string& recipient, const std::string& message, bool restoreChannel = true);

    // Returns true while a send operation is in progress
    static bool IsSending();

    // Returns true while channel restore is in progress (suppress auto-focus)
    static bool IsRestoring();

    // Cleanup
    static void Shutdown();

    // Register a callback for intercepted whispers
    static void SetCallback(WhisperCallback callback);
    static void SetErrorCallback(ErrorCallback callback);

    // Get current status
    static HookStatus GetStatus();
    static const char* GetStatusString();

    // Probe mode: logs raw event data to debug window for format discovery
    static void SetProbeMode(bool enabled);
    static bool GetProbeMode();

    // Debug window: renders ImGui window with probe data
    static void RenderDebugWindow();
    static bool s_ShowDebugWindow;

private:
    static AddonAPI_t* s_API;
    static WhisperCallback s_Callback;
    static ErrorCallback s_ErrorCallback;
    static std::string s_LastWhisperTarget; // account name of last whisper recipient
    static HookStatus s_Status;
    static bool s_ProbeMode;
    static bool s_EventsSubscribed;

    // Nexus event handler for EV_CHAT:Message
    static void OnEvChatMessage(void* eventArgs);

    // Safe memory check
    static bool IsReadableMemory(const void* ptr, size_t size);

    // Probe helpers
    static void ProbeRawEventData(const char* eventName, void* eventArgs);

    // Probe data storage for debug window
    static std::mutex s_ProbeMutex;
    static std::vector<std::string> s_ProbeLines;
    static void ProbeLog(const char* fmt, ...);

    // Send state tracking
    static std::atomic<bool> s_IsSending;
    static std::atomic<bool> s_IsRestoring;

    // Last active non-whisper chat channel (for restoring after whisper send)
    static ChatMessageType s_LastChatChannel;
    static const char* ChannelCommand(ChatMessageType type);
};

} // namespace TyrianIM

#endif
