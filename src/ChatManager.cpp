#include "ChatManager.h"
#include "ChatLinks.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace TyrianIM {

std::unordered_map<std::string, std::unique_ptr<Conversation>> ChatManager::s_Conversations;
std::string ChatManager::s_SelfAccountName;
std::string ChatManager::s_DataDir;
std::mutex ChatManager::s_Mutex;
std::vector<Conversation*> ChatManager::s_SortedCache;
bool ChatManager::s_SortDirty = true;
std::unordered_set<std::string> ChatManager::s_PinnedContacts;

void ChatManager::Initialize(const std::string& data_dir) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_DataDir = data_dir;
    s_Conversations.clear();
    s_SelfAccountName.clear();
    s_SortedCache.clear();
    s_PinnedContacts.clear();
    s_SortDirty = true;
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
    std::string contact = ResolveContact(msg);
    if (contact.empty()) return;
    if (contact[0] == '(') return;

    {
        std::lock_guard<std::mutex> lock(s_Mutex);

        auto& convo_ptr = s_Conversations[contact];
        if (!convo_ptr) {
            convo_ptr = std::make_unique<Conversation>();
            convo_ptr->contact = contact;
            convo_ptr->unread_count = 0;
            convo_ptr->last_activity = 0;
            convo_ptr->pinned = s_PinnedContacts.count(contact) > 0;
        }

        convo_ptr->messages.push_back(msg);
        auto& stored = convo_ptr->messages.back();
        // Parse inline chat links (segments stored on the message itself)
        stored.segments = ParseSegments(stored.text);
        stored.has_links = !stored.segments.empty();
        convo_ptr->last_activity = msg.epoch_ms;
        s_SortDirty = true;

        if (msg.direction == MessageDirection::Incoming) {
            convo_ptr->unread_count++;
        }
    }

    // File I/O outside the lock to avoid blocking the render thread
    AppendMessageToFile(contact, msg);
}

std::vector<Conversation*> ChatManager::GetConversations() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    if (s_SortDirty) {
        s_SortedCache.clear();
        s_SortedCache.reserve(s_Conversations.size());
        for (const auto& [key, convo_ptr] : s_Conversations) {
            s_SortedCache.push_back(convo_ptr.get());
        }
        std::sort(s_SortedCache.begin(), s_SortedCache.end(),
            [](const Conversation* a, const Conversation* b) {
                if (a->pinned != b->pinned) return a->pinned;
                return a->last_activity > b->last_activity;
            });
        s_SortDirty = false;
    }
    return s_SortedCache;
}

Conversation* ChatManager::GetConversation(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it != s_Conversations.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ChatManager::MarkAsRead(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it != s_Conversations.end()) {
        it->second->unread_count = 0;
    }
}

int ChatManager::GetTotalUnreadCount() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    int total = 0;
    for (const auto& [key, convo_ptr] : s_Conversations) {
        total += convo_ptr->unread_count;
    }
    return total;
}

int ChatManager::GetUnreadContactCount() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    int count = 0;
    for (const auto& [key, convo_ptr] : s_Conversations) {
        if (convo_ptr->unread_count > 0) count++;
    }
    return count;
}

void ChatManager::MarkLastOutgoingFailed(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it == s_Conversations.end()) return;
    auto& msgs = it->second->messages;
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
    s_PinnedContacts.erase(contact);
    s_SortDirty = true;
    // Delete the history file
    std::string path = ContactFilePath(contact);
    if (!path.empty()) {
        try { std::filesystem::remove(path); } catch (...) {}
    }
}

void ChatManager::MergeConversation(const std::string& old_key, const std::string& new_key) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    if (old_key == new_key) return;
    auto old_it = s_Conversations.find(old_key);
    if (old_it == s_Conversations.end()) return;

    auto& dest_ptr = s_Conversations[new_key];
    if (!dest_ptr) {
        dest_ptr = std::make_unique<Conversation>();
        dest_ptr->contact = new_key;
        dest_ptr->unread_count = 0;
        dest_ptr->last_activity = 0;
    }
    auto& dest = *dest_ptr;
    auto& src  = *old_it->second;

    // Preserve display_name from old conversation if dest doesn't have one
    if (dest.display_name.empty() && !src.display_name.empty()) {
        dest.display_name = src.display_name;
    }

    // Move messages, avoiding duplicates by epoch_ms + text
    for (auto& msg : src.messages) {
        bool dup = false;
        for (const auto& existing : dest.messages) {
            if (existing.epoch_ms == msg.epoch_ms && existing.text == msg.text) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            dest.messages.push_back(std::move(msg));
        }
    }

    // Sort merged messages by time
    std::sort(dest.messages.begin(), dest.messages.end(),
        [](const ChatMessage& a, const ChatMessage& b) { return a.epoch_ms < b.epoch_ms; });

    // Update activity and unread
    if (src.last_activity > dest.last_activity)
        dest.last_activity = src.last_activity;
    dest.unread_count += src.unread_count;

    // Propagate pinned flag
    if (src.pinned) {
        dest.pinned = true;
        s_PinnedContacts.insert(new_key);
        s_PinnedContacts.erase(old_key);
    }

    // Delete old conversation and its history file
    std::string old_path = ContactFilePath(old_key);
    s_Conversations.erase(old_it);
    s_SortDirty = true;
    if (!old_path.empty()) {
        try { std::filesystem::remove(old_path); } catch (...) {}
    }
}

void ChatManager::SetSelfAccountName(const std::string& name) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_SelfAccountName = name;
}

std::string ChatManager::GetSelfAccountName() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    return s_SelfAccountName;
}

void ChatManager::SetDisplayName(const std::string& contact, const std::string& display_name) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it != s_Conversations.end()) {
        it->second->display_name = display_name;
    }
}

void ChatManager::SetPinnedContacts(const std::vector<std::string>& contacts) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_PinnedContacts.clear();
    for (const auto& c : contacts) s_PinnedContacts.insert(c);
    for (auto& [key, convo_ptr] : s_Conversations)
        convo_ptr->pinned = s_PinnedContacts.count(key) > 0;
    s_SortDirty = true;
}

std::vector<std::string> ChatManager::GetPinnedContacts() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    return std::vector<std::string>(s_PinnedContacts.begin(), s_PinnedContacts.end());
}

void ChatManager::TogglePin(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it == s_Conversations.end()) return;
    it->second->pinned = !it->second->pinned;
    if (it->second->pinned)
        s_PinnedContacts.insert(contact);
    else
        s_PinnedContacts.erase(contact);
    s_SortDirty = true;
}

bool ChatManager::IsPinned(const std::string& contact) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    return s_PinnedContacts.count(contact) > 0;
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

    for (const auto& [contact, convo_ptr] : s_Conversations) {
        std::string path = ContactFilePath(contact);
        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) continue;

        for (const auto& msg : convo_ptr->messages) {
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

                    auto& convo_ptr = s_Conversations[contact];
                    if (!convo_ptr) {
                        convo_ptr = std::make_unique<Conversation>();
                        convo_ptr->contact = contact;
                        convo_ptr->unread_count = 0;
                        convo_ptr->last_activity = 0;
                    }
                    if (msg.direction == MessageDirection::Incoming && !msg.sender.empty()) {
                        convo_ptr->display_name = msg.sender;
                    }
                    auto& stored2 = convo_ptr->messages.emplace_back(msg);
                    stored2.segments  = ParseSegments(stored2.text);
                    stored2.has_links = !stored2.segments.empty();
                    if (msg.epoch_ms > convo_ptr->last_activity) {
                        convo_ptr->last_activity = msg.epoch_ms;
                    }
                }
                old_file.close();
            }

            // Write migrated data as per-contact .tsv files and remove old file
            try {
                std::filesystem::create_directories(hist_dir);
                for (const auto& [contact, convo_ptr] : s_Conversations) {
                    std::string path = ContactFilePath(contact);
                    std::ofstream out(path, std::ios::trunc);
                    if (!out.is_open()) continue;
                    for (const auto& m : convo_ptr->messages) {
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

            auto& convo_ptr = s_Conversations[contact];
            if (!convo_ptr) {
                convo_ptr = std::make_unique<Conversation>();
                convo_ptr->contact = contact;
                convo_ptr->unread_count = 0;
                convo_ptr->last_activity = 0;
            }
            if (msg.direction == MessageDirection::Incoming && !msg.sender.empty()) {
                convo_ptr->display_name = msg.sender;
            }
            auto& stored = convo_ptr->messages.emplace_back(msg);
            stored.segments  = ParseSegments(stored.text);
            stored.has_links = !stored.segments.empty();
            if (msg.epoch_ms > convo_ptr->last_activity) {
                convo_ptr->last_activity = msg.epoch_ms;
            }
        }
    }
}

void ChatManager::UpdateLinkSegment(const std::string& contact, size_t msg_idx, size_t seg_idx,
                                     const std::string& display, const std::string& tooltip,
                                     ImU32 colour, LinkState state) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Conversations.find(contact);
    if (it == s_Conversations.end()) return;
    auto& msgs = it->second->messages;
    if (msg_idx >= msgs.size()) return;
    auto& segs = msgs[msg_idx].segments;
    if (seg_idx >= segs.size() || !segs[seg_idx].is_link) return;
    if (!display.empty())
        segs[seg_idx].link.display  = display;
    segs[seg_idx].link.tooltip_text = tooltip;
    segs[seg_idx].link.colour       = colour;
    segs[seg_idx].link.state        = state;
}

} // namespace TyrianIM
