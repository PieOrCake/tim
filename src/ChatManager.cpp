#include "ChatManager.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace TyrianIM {

std::unordered_map<std::string, Conversation> ChatManager::s_Conversations;
std::string ChatManager::s_SelfAccountName;
std::string ChatManager::s_DataDir;
std::mutex ChatManager::s_Mutex;

void ChatManager::Initialize(const std::string& data_dir) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_DataDir = data_dir;
    s_Conversations.clear();
    s_SelfAccountName.clear();
}

void ChatManager::Shutdown() {
    SaveHistory();
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_Conversations.clear();
}

std::string ChatManager::ResolveContact(const ChatMessage& msg) {
    if (msg.direction == MessageDirection::Outgoing || msg.direction == MessageDirection::System) {
        return msg.recipient;
    }
    return msg.sender;
}

void ChatManager::AddMessage(const ChatMessage& msg) {
    std::lock_guard<std::mutex> lock(s_Mutex);

    std::string contact = ResolveContact(msg);
    if (contact.empty()) return;

    // Skip internal debug/placeholder contacts
    if (!contact.empty() && contact[0] == '(') return;

    auto& convo = s_Conversations[contact];
    if (convo.contact.empty()) {
        convo.contact = contact;
        convo.unread_count = 0;
        convo.last_activity = 0;
    }

    convo.messages.push_back(msg);
    convo.last_activity = msg.epoch_ms;

    if (msg.direction == MessageDirection::Incoming) {
        convo.unread_count++;
    }

    // Auto-persist: append this message to the contact's history file
    AppendMessageToFile(contact, msg);
}

std::vector<const Conversation*> ChatManager::GetConversations() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    std::vector<const Conversation*> result;
    result.reserve(s_Conversations.size());

    for (const auto& [key, convo] : s_Conversations) {
        result.push_back(&convo);
    }

    std::sort(result.begin(), result.end(),
        [](const Conversation* a, const Conversation* b) {
            return a->last_activity > b->last_activity;
        });

    return result;
}

Conversation* ChatManager::GetConversation(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it != s_Conversations.end()) {
        return &it->second;
    }
    return nullptr;
}

void ChatManager::MarkAsRead(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it != s_Conversations.end()) {
        it->second.unread_count = 0;
    }
}

int ChatManager::GetTotalUnreadCount() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    int total = 0;
    for (const auto& [key, convo] : s_Conversations) {
        total += convo.unread_count;
    }
    return total;
}

void ChatManager::MarkLastOutgoingFailed(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it == s_Conversations.end()) return;
    auto& msgs = it->second.messages;
    for (int i = (int)msgs.size() - 1; i >= 0; --i) {
        if (msgs[i].direction == MessageDirection::Outgoing) {
            msgs[i].failed = true;
            break;
        }
    }
}

void ChatManager::DeleteConversation(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_Conversations.erase(contact);
    // Delete the history file
    std::string path = ContactFilePath(contact);
    if (!path.empty()) {
        try { std::filesystem::remove(path); } catch (...) {}
    }
}

void ChatManager::SetSelfAccountName(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_SelfAccountName = name;
}

const std::string& ChatManager::GetSelfAccountName() {
    return s_SelfAccountName;
}

// ============================================================================
// Persistence — per-contact .tsv files in history/ subdirectory
// Format: epoch_ms \t direction \t source \t sender \t recipient \t timestamp \t text
// Fields are escaped: \t → \\t, \n → \\n, \\ → \\\\
// ============================================================================

std::string ChatManager::HistoryDir() {
    return s_DataDir + "/history";
}

std::string ChatManager::SanitizeFilename(const std::string& contact) {
    std::string result;
    result.reserve(contact.size());
    for (char c : contact) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ' ') {
            result += c;
        } else {
            result += '_';
        }
    }
    // Trim trailing spaces/dots (Windows filesystem restriction)
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }
    if (result.empty()) result = "_unnamed";
    return result;
}

std::string ChatManager::ContactFilePath(const std::string& contact) {
    return HistoryDir() + "/" + SanitizeFilename(contact) + ".tsv";
}

std::string ChatManager::EscapeField(const std::string& field) {
    std::string result;
    result.reserve(field.size());
    for (char c : field) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '\t': result += "\\t"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string ChatManager::UnescapeField(const std::string& field) {
    std::string result;
    result.reserve(field.size());
    for (size_t i = 0; i < field.size(); i++) {
        if (field[i] == '\\' && i + 1 < field.size()) {
            switch (field[i + 1]) {
                case '\\': result += '\\'; i++; break;
                case 't':  result += '\t'; i++; break;
                case 'n':  result += '\n'; i++; break;
                case 'r':  result += '\r'; i++; break;
                default:   result += field[i]; break;
            }
        } else {
            result += field[i];
        }
    }
    return result;
}

std::string ChatManager::FormatTimestamp(uint64_t epoch_ms) {
    time_t seconds = static_cast<time_t>(epoch_ms / 1000);
    struct tm tm_info;
#ifdef _WIN32
    localtime_s(&tm_info, &seconds);
#else
    localtime_r(&seconds, &tm_info);
#endif
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M", &tm_info);
    return std::string(buf);
}

void ChatManager::AppendMessageToFile(const std::string& contact, const ChatMessage& msg) {
    if (s_DataDir.empty()) return;

    try { std::filesystem::create_directories(HistoryDir()); } catch (...) {}

    std::string path = ContactFilePath(contact);
    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) return;

    file << msg.epoch_ms << "\t"
         << static_cast<int>(msg.direction) << "\t"
         << static_cast<int>(msg.source) << "\t"
         << EscapeField(msg.sender) << "\t"
         << EscapeField(msg.recipient) << "\t"
         << EscapeField(msg.timestamp) << "\t"
         << EscapeField(msg.text) << "\t"
         << (msg.failed ? 1 : 0) << "\n";
}

void ChatManager::SaveHistory() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    if (s_DataDir.empty()) return;

    try { std::filesystem::create_directories(HistoryDir()); } catch (...) {}

    for (const auto& [contact, convo] : s_Conversations) {
        std::string path = ContactFilePath(contact);
        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) continue;

        for (const auto& msg : convo.messages) {
            file << msg.epoch_ms << "\t"
                 << static_cast<int>(msg.direction) << "\t"
                 << static_cast<int>(msg.source) << "\t"
                 << EscapeField(msg.sender) << "\t"
                 << EscapeField(msg.recipient) << "\t"
                 << EscapeField(msg.timestamp) << "\t"
                 << EscapeField(msg.text) << "\t"
                 << (msg.failed ? 1 : 0) << "\n";
        }
    }
}

void ChatManager::LoadHistory() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    if (s_DataDir.empty()) return;

    std::string hist_dir = HistoryDir();
    if (!std::filesystem::exists(hist_dir)) {
        // Migrate from old single-file format if it exists
        std::string old_path = s_DataDir + "/history.txt";
        if (std::filesystem::exists(old_path)) {
            std::ifstream old_file(old_path);
            if (old_file.is_open()) {
                std::string line;
                while (std::getline(old_file, line)) {
                    if (line.empty()) continue;
                    std::vector<std::string> fields;
                    std::istringstream iss(line);
                    std::string field;
                    while (std::getline(iss, field, '\t')) {
                        fields.push_back(field);
                    }
                    if (fields.size() < 6) continue;

                    ChatMessage msg;
                    try { msg.epoch_ms = std::stoull(fields[0]); } catch (...) { continue; }
                    msg.direction = static_cast<MessageDirection>(std::stoi(fields[1]));
                    msg.source = static_cast<MessageSource>(std::stoi(fields[2]));
                    msg.sender = fields[3];
                    msg.recipient = fields[4];
                    msg.text = fields[5];
                    msg.timestamp = FormatTimestamp(msg.epoch_ms);

                    std::string contact = (msg.direction == MessageDirection::Outgoing)
                        ? msg.recipient : msg.sender;
                    if (contact.empty()) continue;
                    if (contact[0] == '(') continue;

                    auto& convo = s_Conversations[contact];
                    if (convo.contact.empty()) {
                        convo.contact = contact;
                        convo.unread_count = 0;
                        convo.last_activity = 0;
                    }
                    if (msg.direction == MessageDirection::Incoming && !msg.sender.empty()) {
                        convo.display_name = msg.sender;
                    }
                    convo.messages.push_back(msg);
                    if (msg.epoch_ms > convo.last_activity) {
                        convo.last_activity = msg.epoch_ms;
                    }
                }
                old_file.close();
            }

            // Write migrated data as per-contact .tsv files and remove old file
            try {
                std::filesystem::create_directories(hist_dir);
                for (const auto& [contact, convo] : s_Conversations) {
                    std::string path = ContactFilePath(contact);
                    std::ofstream out(path, std::ios::trunc);
                    if (!out.is_open()) continue;
                    for (const auto& m : convo.messages) {
                        out << m.epoch_ms << "\t"
                            << static_cast<int>(m.direction) << "\t"
                            << static_cast<int>(m.source) << "\t"
                            << EscapeField(m.sender) << "\t"
                            << EscapeField(m.recipient) << "\t"
                            << EscapeField(m.timestamp) << "\t"
                            << EscapeField(m.text) << "\t"
                            << (m.failed ? 1 : 0) << "\n";
                    }
                }
                std::filesystem::remove(old_path);
            } catch (...) {}
        }
        return;
    }

    // Load each .tsv file from history/ directory
    for (const auto& entry : std::filesystem::directory_iterator(hist_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".tsv") continue;

        std::ifstream file(entry.path());
        if (!file.is_open()) continue;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;

            // Parse tab-delimited fields (7 fields with timestamp)
            std::vector<std::string> fields;
            std::istringstream iss(line);
            std::string field;
            while (std::getline(iss, field, '\t')) {
                fields.push_back(field);
            }

            ChatMessage msg;

            if (fields.size() >= 7) {
                // New format: epoch_ms, direction, source, sender, recipient, timestamp, text
                try { msg.epoch_ms = std::stoull(fields[0]); } catch (...) { continue; }
                msg.direction = static_cast<MessageDirection>(std::stoi(fields[1]));
                msg.source = static_cast<MessageSource>(std::stoi(fields[2]));
                msg.sender = UnescapeField(fields[3]);
                msg.recipient = UnescapeField(fields[4]);
                msg.timestamp = UnescapeField(fields[5]);
                msg.text = UnescapeField(fields[6]);
                if (fields.size() >= 8) {
                    msg.failed = (fields[7] == "1");
                }
            } else if (fields.size() >= 6) {
                // Old format fallback: epoch_ms, direction, source, sender, recipient, text
                try { msg.epoch_ms = std::stoull(fields[0]); } catch (...) { continue; }
                msg.direction = static_cast<MessageDirection>(std::stoi(fields[1]));
                msg.source = static_cast<MessageSource>(std::stoi(fields[2]));
                msg.sender = UnescapeField(fields[3]);
                msg.recipient = UnescapeField(fields[4]);
                msg.text = UnescapeField(fields[5]);
                msg.timestamp = FormatTimestamp(msg.epoch_ms);
            } else {
                continue;
            }

            std::string contact = (msg.direction == MessageDirection::Outgoing)
                ? msg.recipient : msg.sender;
            if (contact.empty()) continue;
            if (contact[0] == '(') continue; // skip debug/placeholder entries

            auto& convo = s_Conversations[contact];
            if (convo.contact.empty()) {
                convo.contact = contact;
                convo.unread_count = 0;
                convo.last_activity = 0;
            }
            if (msg.direction == MessageDirection::Incoming && !msg.sender.empty()) {
                convo.display_name = msg.sender;
            }
            convo.messages.push_back(msg);
            if (msg.epoch_ms > convo.last_activity) {
                convo.last_activity = msg.epoch_ms;
            }
        }
    }
}

} // namespace TyrianIM
