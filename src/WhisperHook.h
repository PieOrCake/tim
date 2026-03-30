#ifndef WHISPERHOOK_H
#define WHISPERHOOK_H

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
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
    };
} ChatMessage;

namespace TyrianIM {

// Callback signature for intercepted whisper messages
using WhisperCallback = std::function<void(const std::string& sender, const std::string& recipient, const std::string& message, bool is_incoming)>;
using ErrorCallback = std::function<void(const std::string& contact, const std::string& error_text)>;
using NamePairCallback = std::function<void(const std::string& character_name, const std::string& account_name)>;

enum class HookStatus {
    NotInitialized,
    Scanning,
    Hooked,
    Failed,
    Disabled
};

class WhisperHook {
public:
    static void Initialize(AddonAPI_t* api);
    static bool InstallHooks();
    static void RemoveHooks();
    static void Shutdown();

    static void SetCallback(WhisperCallback callback);
    static void SetErrorCallback(ErrorCallback callback);
    static void SetNamePairCallback(NamePairCallback callback);

    static HookStatus GetStatus();
    static const char* GetStatusString();

    static void SetProbeMode(bool enabled);
    static bool GetProbeMode();

    static void RenderDebugWindow();
    static bool s_ShowDebugWindow;

private:
    static AddonAPI_t* s_API;
    static WhisperCallback s_Callback;
    static ErrorCallback s_ErrorCallback;
    static NamePairCallback s_NamePairCallback;
    static HookStatus s_Status;
    static bool s_ProbeMode;
    static bool s_EventsSubscribed;

    static void OnEvChatMessage(void* eventArgs);
    static bool IsReadableMemory(const void* ptr, size_t size);
    static void ProbeRawEventData(const char* eventName, void* eventArgs);

    static std::mutex s_ProbeMutex;
    static std::vector<std::string> s_ProbeLines;
    static void ProbeLog(const char* fmt, ...);
};

} // namespace TyrianIM

#endif
