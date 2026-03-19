#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <ctime>

namespace TyrianIM {

enum class MessageDirection {
    Incoming,
    Outgoing,
    System          // Error/system messages (not from a user)
};

enum class MessageSource {
    Whisper,        // /w private messages (target feature - requires RE hooks)
    Squad,          // Squad/party chat (available via Unofficial Extras)
    NPC             // NPC dialogue (available via Unofficial Extras)
};

struct ChatMessage {
    std::string sender;         // Account name or character name
    std::string recipient;      // For outgoing whispers
    std::string text;
    std::string timestamp;      // ISO8601 or formatted local time
    uint64_t    epoch_ms;       // For sorting
    MessageDirection direction;
    MessageSource source;
    bool failed = false;       // True if delivery failed (e.g. player offline)
};

struct Conversation {
    std::string contact;        // Account name of the other party
    std::string display_name;   // Character name if known
    std::vector<ChatMessage> messages;
    uint64_t last_activity;     // epoch_ms of last message
    int unread_count;
};

class ChatManager {
public:
    static void Initialize(const std::string& data_dir);
    static void Shutdown();

    // Add a message to the appropriate conversation
    static void AddMessage(const ChatMessage& msg);

    // Get all conversations sorted by last activity (most recent first)
    static std::vector<const Conversation*> GetConversations();

    // Get a specific conversation by contact name
    static Conversation* GetConversation(const std::string& contact);

    // Mark a conversation as read
    static void MarkAsRead(const std::string& contact);

    // Get total unread count across all conversations
    static int GetTotalUnreadCount();

    // Mark the last outgoing message to a contact as failed
    static void MarkLastOutgoingFailed(const std::string& contact);

    // Delete a conversation and its history file
    static void DeleteConversation(const std::string& contact);

    // Set the local player's account name
    static void SetSelfAccountName(const std::string& name);
    static const std::string& GetSelfAccountName();

    // Persistence
    static void SaveHistory();
    static void LoadHistory();

private:
    static std::unordered_map<std::string, Conversation> s_Conversations;
    static std::string s_SelfAccountName;
    static std::string s_DataDir;
    static std::mutex s_Mutex;

    // Resolve the contact name from a message (the "other party")
    static std::string ResolveContact(const ChatMessage& msg);

    // Per-contact file persistence helpers
    static std::string HistoryDir();
    static std::string SanitizeFilename(const std::string& contact);
    static std::string ContactFilePath(const std::string& contact);
    static std::string EscapeField(const std::string& field);
    static std::string UnescapeField(const std::string& field);
    static void AppendMessageToFile(const std::string& contact, const ChatMessage& msg);
    static std::string FormatTimestamp(uint64_t epoch_ms);
};

} // namespace TyrianIM

#endif
