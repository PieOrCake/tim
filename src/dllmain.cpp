#include <windows.h>
#include <shellapi.h>
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
#include <thread>
#include <mutex>
#include <atomic>
#include <cwchar>
#include <unordered_map>
#include <deque>
#include "../include/nexus/Nexus.h"
#include "../include/mumble/Mumble.h"
#include "../lib/toml.hpp"
#include <nlohmann/json.hpp>
#include "ChatManager.h"
#include "WhisperHook.h"
#include "ChatLinks.h"
#include "GW2API.h"
#include "nyancat_frames.h"

// Version constants
#define V_MAJOR 0
#define V_MINOR 1
#define V_BUILD 0
#define V_REVISION 0

// Quick Access icon identifiers
#define QA_ID          "QA_TYRIAN_IM"
#define TEX_ICON       "TEX_TYRIAN_IM"
#define TEX_ICON_HOVER "TEX_TYRIAN_IM_HOVER"
#define TEX_FLOAT_ICON "TEX_TYRIAN_IM_FLOAT"

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

// Embedded QA bar icon (tim.png, 32x32)
static const unsigned char ICON_DATA[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
  0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20,
  0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a, 0xf4, 0x00, 0x00, 0x00,
  0x01, 0x73, 0x52, 0x47, 0x42, 0x01, 0xd9, 0xc9, 0x2c, 0x7f, 0x00, 0x00,
  0x00, 0x04, 0x67, 0x41, 0x4d, 0x41, 0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc,
  0x61, 0x05, 0x00, 0x00, 0x00, 0x20, 0x63, 0x48, 0x52, 0x4d, 0x00, 0x00,
  0x7a, 0x26, 0x00, 0x00, 0x80, 0x84, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x00,
  0x80, 0xe8, 0x00, 0x00, 0x75, 0x30, 0x00, 0x00, 0xea, 0x60, 0x00, 0x00,
  0x3a, 0x98, 0x00, 0x00, 0x17, 0x70, 0x9c, 0xba, 0x51, 0x3c, 0x00, 0x00,
  0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00,
  0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74,
  0x49, 0x4d, 0x45, 0x07, 0xea, 0x05, 0x06, 0x09, 0x0c, 0x32, 0xfd, 0x33,
  0x67, 0xeb, 0x00, 0x00, 0x05, 0xe9, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3,
  0xed, 0x57, 0x6b, 0x50, 0x94, 0x65, 0x14, 0x7e, 0xbe, 0x6f, 0xbf, 0xbd,
  0xb0, 0xec, 0xc2, 0x2e, 0x01, 0x0b, 0xe4, 0xae, 0xe0, 0x0a, 0x0a, 0xc8,
  0xb6, 0x71, 0x4b, 0x02, 0x1a, 0x15, 0x71, 0x44, 0xc3, 0x19, 0x2f, 0xa5,
  0xcd, 0x98, 0x96, 0x8e, 0x33, 0xf2, 0xa3, 0x1a, 0x6d, 0xba, 0x8c, 0xa9,
  0x33, 0x56, 0x63, 0x63, 0xa5, 0x3f, 0xaa, 0x29, 0xad, 0x71, 0x2a, 0x53,
  0x27, 0x89, 0x2e, 0x66, 0x29, 0x9a, 0x80, 0x22, 0x9a, 0xa8, 0x21, 0x88,
  0xa8, 0x80, 0xb8, 0xc2, 0x82, 0xc0, 0xb2, 0xbb, 0xec, 0x2e, 0xbb, 0xdf,
  0xde, 0xbe, 0xdd, 0xb7, 0x1f, 0xe8, 0x26, 0xee, 0x4a, 0x5e, 0xa2, 0x3f,
  0x75, 0x66, 0xbe, 0x3f, 0xef, 0xfb, 0x9e, 0x73, 0x9e, 0xf7, 0x9c, 0xf3,
  0xbc, 0xe7, 0x7c, 0xc0, 0x7f, 0x5d, 0x78, 0x0f, 0xa2, 0xb4, 0x65, 0xdd,
  0xf2, 0xe4, 0xf3, 0xcd, 0xd7, 0x26, 0x6c, 0x59, 0xb7, 0x5c, 0xf3, 0x6c,
  0x69, 0x41, 0xea, 0xa0, 0xc5, 0x1e, 0x39, 0xa7, 0x28, 0x5b, 0x7a, 0xae,
  0xe9, 0xaa, 0x69, 0x4c, 0x50, 0x2e, 0x28, 0xc9, 0x8b, 0x8d, 0x8e, 0x8a,
  0x78, 0x29, 0x57, 0x9b, 0x7c, 0x44, 0x12, 0x2e, 0xb2, 0x02, 0x20, 0xea,
  0xf1, 0x71, 0x64, 0x46, 0xbe, 0x86, 0x14, 0xe5, 0x6b, 0x48, 0x5a, 0xb2,
  0x92, 0x00, 0x20, 0x79, 0x59, 0x93, 0x38, 0x55, 0x42, 0x4c, 0x55, 0x71,
  0xa1, 0xf6, 0xad, 0xcf, 0x36, 0xaf, 0x56, 0xdd, 0x8b, 0x6d, 0x6a, 0xb4,
  0xcd, 0x35, 0xab, 0xe6, 0x4d, 0xac, 0x3c, 0x76, 0x7e, 0xa3, 0xc3, 0xe1,
  0x7c, 0x2e, 0x2d, 0x45, 0xc5, 0x4c, 0x4c, 0x8c, 0x87, 0x22, 0x46, 0x86,
  0x48, 0xa9, 0x18, 0x14, 0x15, 0xac, 0x6a, 0x1d, 0x62, 0x61, 0x30, 0x5a,
  0xa0, 0xd3, 0xf7, 0xe3, 0x50, 0x75, 0x03, 0xb4, 0x53, 0x92, 0xf6, 0x19,
  0xcd, 0xb6, 0x4d, 0xba, 0xae, 0xea, 0x56, 0x8a, 0x9a, 0xe2, 0xbf, 0x2f,
  0x00, 0x19, 0x93, 0xc7, 0xaf, 0xef, 0xec, 0x19, 0x58, 0x57, 0x3a, 0x33,
  0x27, 0x2c, 0x3d, 0x45, 0x05, 0x49, 0xb8, 0x08, 0x14, 0x45, 0x81, 0xa2,
  0x28, 0x10, 0x42, 0x46, 0xbd, 0x15, 0x21, 0x04, 0x0e, 0xd6, 0x8d, 0xcb,
  0xed, 0x7a, 0x54, 0x1c, 0x3c, 0xc5, 0x29, 0x62, 0x64, 0x1f, 0x56, 0xec,
  0x78, 0xe3, 0xed, 0xac, 0x92, 0xb5, 0xce, 0x7b, 0x01, 0x20, 0x2e, 0xcc,
  0x4d, 0xfb, 0xd9, 0xe9, 0xf6, 0x14, 0x95, 0xce, 0xcc, 0x41, 0x94, 0x4c,
  0x1a, 0xd2, 0x09, 0x4d, 0x51, 0xf0, 0xdf, 0x05, 0xc8, 0xed, 0x00, 0xed,
  0x0e, 0x17, 0x0e, 0x1f, 0x6f, 0xc0, 0xc5, 0xd6, 0xae, 0xba, 0x4f, 0x37,
  0x97, 0x2d, 0x5c, 0x5c, 0xf6, 0xbe, 0x61, 0x34, 0x00, 0x62, 0x3e, 0x9f,
  0xa9, 0x2c, 0x2e, 0x7c, 0xac, 0xb0, 0xb8, 0x50, 0x0b, 0x86, 0xe1, 0x8d,
  0x9a, 0xbb, 0x3b, 0xdd, 0xdf, 0x2d, 0x3a, 0x3e, 0x9f, 0x1f, 0xc7, 0x4f,
  0x5f, 0x84, 0xcf, 0xe7, 0xef, 0x98, 0x5f, 0x32, 0x35, 0xe7, 0xc5, 0xb5,
  0x1f, 0x0d, 0x86, 0x04, 0x90, 0x9f, 0x9d, 0x5a, 0x21, 0x95, 0x84, 0x2d,
  0x9c, 0x3d, 0x2d, 0x13, 0x3c, 0x1e, 0xfd, 0xc0, 0x45, 0x4b, 0x08, 0x09,
  0xaa, 0x11, 0xbf, 0x9f, 0xe0, 0xb7, 0xda, 0x46, 0x9c, 0x3e, 0xdf, 0x7a,
  0xd4, 0x68, 0xb6, 0x15, 0x07, 0xd1, 0x70, 0x6e, 0x51, 0xf6, 0x92, 0xeb,
  0xdd, 0x86, 0x0d, 0x8b, 0x4b, 0x0b, 0xc0, 0xe7, 0x33, 0x81, 0x1b, 0xdd,
  0x19, 0x76, 0x72, 0xc7, 0xfa, 0xad, 0xba, 0xb8, 0x73, 0x2d, 0x54, 0xca,
  0x12, 0x95, 0x0a, 0xe8, 0xf4, 0xfd, 0x13, 0xd2, 0x92, 0x95, 0x16, 0x9d,
  0xbe, 0xbf, 0x1e, 0x00, 0x68, 0x00, 0xf8, 0xe3, 0xe0, 0x36, 0x1e, 0xc7,
  0xf9, 0xb6, 0xce, 0x9f, 0x3d, 0x15, 0x42, 0x21, 0x3f, 0x64, 0x2e, 0x01,
  0x04, 0x72, 0x7e, 0xfb, 0x3a, 0x21, 0x24, 0xf0, 0x85, 0xd2, 0x09, 0x9c,
  0x03, 0xc0, 0xe3, 0xd1, 0x98, 0x33, 0x3d, 0x0b, 0x0d, 0xcd, 0x1d, 0x1b,
  0x9f, 0x5f, 0x38, 0x5d, 0x1a, 0x88, 0xc0, 0x75, 0xbd, 0x61, 0xc1, 0x8d,
  0x7e, 0xd3, 0xca, 0x19, 0xf9, 0x9a, 0x90, 0xe8, 0xef, 0x47, 0xfe, 0x4e,
  0x3f, 0x5c, 0x2c, 0x82, 0xc9, 0x32, 0x14, 0x16, 0x26, 0x14, 0xf4, 0xb6,
  0xb4, 0x75, 0x9d, 0xa1, 0x01, 0xc0, 0x34, 0x68, 0x5b, 0x92, 0x9f, 0x93,
  0x1a, 0xa4, 0xec, 0xf3, 0x8d, 0xa4, 0xae, 0xc7, 0xc3, 0x05, 0xed, 0xb3,
  0x4e, 0xf7, 0xf0, 0x9e, 0x97, 0x0b, 0xe9, 0x90, 0xe3, 0x7c, 0x70, 0xb9,
  0x3c, 0x23, 0x00, 0x6a, 0x26, 0x27, 0xe2, 0xd8, 0xef, 0xcd, 0x73, 0x03,
  0x29, 0xa0, 0x68, 0x2a, 0x57, 0x11, 0x2d, 0x1b, 0x69, 0xdc, 0xef, 0x47,
  0x55, 0x5d, 0x13, 0x8c, 0x66, 0x1b, 0x00, 0x80, 0x75, 0xba, 0x51, 0x5b,
  0xdf, 0x02, 0xe7, 0x4d, 0x63, 0xa6, 0xc1, 0x21, 0x9c, 0x3c, 0x77, 0x19,
  0x2d, 0xad, 0x5d, 0xe0, 0x38, 0x1f, 0x8e, 0x9e, 0x68, 0x02, 0xc7, 0xf9,
  0x40, 0x0d, 0x47, 0x14, 0x83, 0x16, 0x3b, 0x28, 0x00, 0xfa, 0x5e, 0x23,
  0x06, 0x6e, 0xda, 0xb8, 0x25, 0x8a, 0x68, 0x19, 0x3c, 0x5e, 0xae, 0xf0,
  0xaf, 0x1a, 0xb8, 0xd0, 0xa1, 0x12, 0x09, 0x05, 0x23, 0x9b, 0x04, 0x4d,
  0x63, 0x6a, 0xe6, 0x24, 0x34, 0xb6, 0xe8, 0x60, 0xb1, 0x39, 0x70, 0xa6,
  0xb1, 0x1d, 0xda, 0xf4, 0x24, 0x84, 0x89, 0x04, 0xd0, 0xe9, 0xfb, 0xd1,
  0x6b, 0x30, 0xa3, 0x20, 0x27, 0x15, 0x84, 0x10, 0x30, 0x0c, 0x0f, 0x49,
  0x4a, 0x05, 0xfa, 0x06, 0x2c, 0xf0, 0x72, 0x3e, 0xb4, 0xb4, 0x75, 0xe1,
  0x86, 0xc1, 0x0c, 0x02, 0xc0, 0x68, 0xb2, 0x21, 0x4a, 0x26, 0x19, 0x11,
  0x81, 0xc8, 0x08, 0x31, 0x2c, 0x36, 0x87, 0xf8, 0xc8, 0x9e, 0x4d, 0xf1,
  0xf4, 0xad, 0xc2, 0x11, 0x08, 0x98, 0xa0, 0xf0, 0x45, 0x48, 0xc5, 0xc8,
  0xd5, 0x26, 0xa3, 0xb6, 0xbe, 0x05, 0x49, 0x4a, 0x05, 0xa2, 0xa3, 0x22,
  0x00, 0x00, 0x91, 0x52, 0x31, 0x7c, 0x3e, 0x3f, 0xac, 0x43, 0x2c, 0x28,
  0x7a, 0x38, 0x6d, 0xf1, 0x0a, 0x39, 0x8c, 0x66, 0x1b, 0xcc, 0x56, 0x3b,
  0x92, 0x93, 0x12, 0x60, 0x34, 0xdb, 0xe0, 0xf7, 0x13, 0x58, 0x87, 0x58,
  0x84, 0x8b, 0x45, 0x21, 0x29, 0xba, 0xf5, 0x8b, 0xfd, 0x09, 0x0c, 0x00,
  0x30, 0x3c, 0x1e, 0xdc, 0x1e, 0x2f, 0x84, 0x02, 0x7e, 0x10, 0x9f, 0x0d,
  0x26, 0x2b, 0xc6, 0xc5, 0x3d, 0x82, 0xae, 0x1e, 0x03, 0xe2, 0x15, 0x72,
  0x08, 0xf8, 0x0c, 0xa2, 0x64, 0x52, 0xc8, 0x23, 0x25, 0x70, 0xb0, 0x2e,
  0x44, 0x48, 0xc4, 0x20, 0x84, 0x20, 0x52, 0x2a, 0x46, 0xdf, 0xc0, 0x20,
  0x58, 0xa7, 0x0b, 0x8f, 0x4f, 0x51, 0xc3, 0xe3, 0xe5, 0xd0, 0x76, 0xad,
  0x07, 0xb1, 0xd1, 0x91, 0x41, 0x17, 0xf3, 0xfb, 0x87, 0x99, 0xb2, 0xfd,
  0xbd, 0xb2, 0x1e, 0x1a, 0x00, 0x12, 0x95, 0xb1, 0x6d, 0x2c, 0xeb, 0x0e,
  0x3a, 0xd8, 0x6b, 0x18, 0x84, 0xd1, 0x6c, 0x83, 0x36, 0x3d, 0x09, 0x89,
  0x4a, 0x05, 0xea, 0x1b, 0x5a, 0x03, 0x85, 0x48, 0x51, 0x14, 0x1c, 0xac,
  0x1b, 0x7c, 0x86, 0x17, 0x78, 0x07, 0xe2, 0x62, 0xe4, 0xa0, 0x69, 0x1a,
  0x42, 0x01, 0x1f, 0xf1, 0xb1, 0x72, 0x5c, 0xed, 0xec, 0x0d, 0x44, 0x6d,
  0x64, 0xd3, 0x72, 0x20, 0x57, 0x9b, 0xc2, 0x26, 0xe5, 0xad, 0xea, 0xa3,
  0x01, 0x20, 0x41, 0x11, 0x55, 0xd7, 0xd3, 0x37, 0xb2, 0x95, 0x3b, 0x5d,
  0x1e, 0x74, 0x76, 0x1b, 0x90, 0x99, 0xa1, 0x06, 0x4d, 0xd3, 0x50, 0x26,
  0x44, 0x23, 0x2e, 0x56, 0x0e, 0x9b, 0x9d, 0x0d, 0x70, 0xdd, 0xe5, 0xf6,
  0x20, 0x2e, 0x56, 0x1e, 0xd0, 0x89, 0x57, 0xc8, 0x91, 0x9a, 0xac, 0x04,
  0x45, 0x01, 0x51, 0x32, 0x09, 0xd2, 0x53, 0x54, 0x88, 0x8b, 0x91, 0x05,
  0x01, 0xe8, 0x1f, 0xb0, 0xa0, 0xd7, 0x60, 0x3e, 0x1e, 0x78, 0x8a, 0xb3,
  0x32, 0xd4, 0xd3, 0x4c, 0x96, 0xa1, 0x9a, 0x57, 0x56, 0x96, 0x82, 0xe1,
  0xf1, 0x30, 0xd6, 0xf2, 0xcd, 0xf7, 0x35, 0x10, 0x09, 0x05, 0xcb, 0x6b,
  0xeb, 0x5b, 0x76, 0x0d, 0xb3, 0xa0, 0xb9, 0xe3, 0x98, 0x50, 0xc0, 0x3f,
  0xd5, 0x7c, 0xa5, 0x73, 0xcc, 0x9d, 0x5f, 0xef, 0x36, 0x80, 0xcf, 0x30,
  0x37, 0xf6, 0x7e, 0xf2, 0x6a, 0x45, 0x80, 0x86, 0x00, 0x30, 0x2d, 0x2f,
  0x63, 0xd5, 0xd7, 0xe5, 0xd5, 0xac, 0x4e, 0xdf, 0x3f, 0x66, 0xce, 0x1d,
  0xac, 0x0b, 0xdf, 0xee, 0x3f, 0x01, 0xa3, 0xd9, 0xb6, 0x7a, 0x5c, 0xce,
  0x0a, 0x76, 0x04, 0x80, 0x1d, 0xbb, 0x2b, 0x2f, 0x2d, 0x5b, 0x34, 0x7d,
  0xde, 0xae, 0x8a, 0x1a, 0xa7, 0xae, 0xeb, 0x9f, 0x07, 0xc1, 0x3a, 0xdd,
  0xd8, 0x77, 0xa0, 0x0e, 0xb3, 0x9e, 0xd2, 0x6e, 0x6b, 0xbd, 0xd6, 0x73,
  0x20, 0xe4, 0x50, 0xda, 0xd8, 0xa2, 0xd3, 0xc9, 0x23, 0xc3, 0x6b, 0xaf,
  0x5c, 0xed, 0x2e, 0x21, 0x84, 0x48, 0xe2, 0x62, 0xe5, 0x0f, 0xd5, 0x96,
  0x6f, 0x49, 0x4f, 0x9f, 0x09, 0x9f, 0xef, 0x39, 0x8c, 0x2c, 0x8d, 0xfa,
  0x83, 0xdd, 0x3f, 0x1c, 0x7b, 0x6d, 0xd4, 0xa9, 0x78, 0xc8, 0xee, 0xec,
  0x5a, 0xba, 0x60, 0xda, 0x9e, 0xef, 0x7e, 0x3d, 0x39, 0xd1, 0x66, 0x67,
  0x27, 0xa7, 0xa7, 0xa8, 0x1e, 0xd8, 0xf1, 0xa0, 0xd5, 0x8e, 0xda, 0xd3,
  0x17, 0x71, 0xe0, 0xe8, 0x39, 0x93, 0x5a, 0x15, 0xb7, 0xf4, 0x50, 0x4d,
  0xc3, 0xc7, 0xf7, 0x34, 0x96, 0x9f, 0x6d, 0x6a, 0xb7, 0xcf, 0x29, 0xca,
  0xb6, 0xba, 0x5c, 0xde, 0xa5, 0x69, 0x29, 0xca, 0x40, 0xe3, 0xe9, 0x35,
  0x98, 0x21, 0x60, 0x98, 0x51, 0x27, 0x25, 0xbb, 0xc3, 0x05, 0x9d, 0xbe,
  0x1f, 0x75, 0x67, 0x2e, 0x61, 0xcf, 0x8f, 0xc7, 0xed, 0x51, 0x72, 0xe9,
  0xf6, 0x17, 0x9e, 0x99, 0xb1, 0xe8, 0xcb, 0xf2, 0xaa, 0xf3, 0xf7, 0x35,
  0x94, 0x4e, 0x52, 0x3f, 0xba, 0x5e, 0x93, 0x9a, 0xf8, 0x4e, 0x61, 0x6e,
  0x1a, 0xec, 0x0e, 0x17, 0x0e, 0x56, 0x9f, 0x83, 0xcd, 0xee, 0xec, 0xbe,
  0xd4, 0xae, 0x1f, 0xa7, 0x4a, 0x88, 0x46, 0xa2, 0x52, 0x01, 0x86, 0x47,
  0x83, 0xcf, 0x30, 0x70, 0x7b, 0xbd, 0xf0, 0xf9, 0xfc, 0x38, 0xdb, 0xd8,
  0x0e, 0x45, 0x8c, 0xcc, 0x23, 0x12, 0x0a, 0x6a, 0x34, 0xa9, 0x89, 0xbf,
  0xe4, 0x6a, 0x93, 0xf7, 0xae, 0xd9, 0xb4, 0xd3, 0xfc, 0x40, 0x63, 0xf9,
  0xf8, 0x71, 0x31, 0x4d, 0xf3, 0x8a, 0x9f, 0xd0, 0x78, 0xbc, 0x5e, 0xfc,
  0x54, 0x59, 0x8f, 0x70, 0xb1, 0x68, 0x73, 0x4d, 0xf9, 0xbb, 0x1b, 0x76,
  0xee, 0x3b, 0xca, 0x7c, 0x55, 0x5e, 0x95, 0x91, 0x99, 0xa1, 0x9e, 0xb2,
  0xff, 0x70, 0xbd, 0x90, 0xc7, 0xa3, 0xc1, 0x71, 0x3e, 0xbc, 0xbc, 0xe2,
  0x69, 0xeb, 0x99, 0xc6, 0xf6, 0xcb, 0xa7, 0x1b, 0x5a, 0x2f, 0x3c, 0x74,
  0xd1, 0x2c, 0x9c, 0xf3, 0x64, 0x06, 0x45, 0x51, 0x44, 0x2a, 0x09, 0x23,
  0xea, 0xf1, 0xf1, 0xb5, 0x65, 0xcb, 0x4a, 0xb2, 0xfe, 0xd5, 0xff, 0xb5,
  0xf2, 0xed, 0xaf, 0xf3, 0xcb, 0x96, 0x95, 0xbc, 0x39, 0xeb, 0x29, 0x6d,
  0x01, 0xfe, 0x97, 0x31, 0x96, 0x3f, 0x01, 0xd3, 0xa7, 0x86, 0x1f, 0xb0,
  0x8f, 0xb2, 0x87, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
  0x42, 0x60, 0x82
};
static const unsigned int ICON_DATA_SIZE = 1683;

// Embedded QA bar hover icon (tim.png +20% brightness)
static const unsigned char ICON_HOVER_DATA[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
  0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20,
  0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a, 0xf4, 0x00, 0x00, 0x05,
  0xcb, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0xed, 0x56, 0x69, 0x50, 0x53,
  0x57, 0x14, 0xfe, 0x5e, 0x36, 0x42, 0x64, 0x0f, 0xd0, 0x04, 0x22, 0x28,
  0x84, 0x65, 0x10, 0xa8, 0x80, 0x80, 0x28, 0x9b, 0x82, 0x08, 0x3a, 0x22,
  0x05, 0x05, 0x5c, 0xa8, 0x5a, 0x07, 0xc6, 0xfe, 0x68, 0xa5, 0xb6, 0xb5,
  0xb5, 0x3a, 0x4c, 0x75, 0xb4, 0xce, 0xb4, 0x55, 0xa9, 0x3f, 0xca, 0xb4,
  0x3a, 0x9d, 0x8a, 0x9d, 0xd6, 0x52, 0x5b, 0x0b, 0xca, 0x32, 0x58, 0xc1,
  0x05, 0xb1, 0x45, 0x10, 0x01, 0xcb, 0x20, 0x12, 0x61, 0x58, 0x42, 0x20,
  0x10, 0x96, 0x84, 0xb0, 0x05, 0x5e, 0xe7, 0x3d, 0x4c, 0x4a, 0x02, 0x38,
  0xee, 0x7f, 0xda, 0x6f, 0xe6, 0x25, 0xf7, 0x9d, 0x73, 0xee, 0x3d, 0xdf,
  0x39, 0xef, 0x9c, 0xf3, 0x1e, 0xf0, 0x3f, 0x9e, 0x01, 0x3f, 0x9c, 0xdc,
  0xe3, 0x62, 0xcb, 0x37, 0x5f, 0x72, 0xe1, 0xf4, 0xbe, 0xe8, 0x92, 0x9c,
  0xc3, 0xeb, 0xd6, 0xac, 0xf4, 0xf3, 0xcf, 0x48, 0x4f, 0x72, 0xc3, 0xcb,
  0xc2, 0x5b, 0x49, 0x91, 0xb6, 0x02, 0x1b, 0x8b, 0x77, 0xc2, 0x83, 0x3c,
  0x8b, 0xcd, 0x4c, 0x8d, 0x07, 0x00, 0x90, 0x1e, 0x2e, 0x22, 0x72, 0x7d,
  0x54, 0x20, 0x19, 0x17, 0x15, 0x48, 0xfa, 0x7a, 0x3a, 0x91, 0x94, 0x2c,
  0x32, 0xc4, 0x5b, 0x23, 0x76, 0x14, 0x5e, 0x89, 0x8f, 0x09, 0xda, 0x9f,
  0xff, 0xfd, 0x01, 0x87, 0x27, 0x39, 0x9b, 0x78, 0x9c, 0xf2, 0xe8, 0xbe,
  0x14, 0x71, 0xce, 0xc5, 0x9b, 0x19, 0x4a, 0xa5, 0x7a, 0x93, 0xaf, 0x97,
  0x98, 0xb5, 0xc8, 0xcd, 0x01, 0x22, 0x21, 0x1f, 0x56, 0x16, 0x26, 0x20,
  0x88, 0x99, 0x5b, 0x15, 0xfd, 0x2a, 0x48, 0x65, 0xbd, 0x68, 0x90, 0xb4,
  0xe3, 0x5c, 0x6e, 0x19, 0x96, 0xf9, 0xbb, 0xff, 0x2c, 0xeb, 0xee, 0x3b,
  0x78, 0x5f, 0xd2, 0x7e, 0x9f, 0x20, 0x18, 0x93, 0x4f, 0x45, 0x20, 0x60,
  0xb1, 0xcb, 0x81, 0xc6, 0xe6, 0xce, 0x4f, 0x52, 0xe2, 0xc3, 0x8c, 0xfd,
  0xbc, 0xc4, 0x30, 0x37, 0xe3, 0xd1, 0x4e, 0xa9, 0x8b, 0x24, 0xa9, 0x80,
  0xe7, 0x06, 0xa5, 0x57, 0xaa, 0x86, 0x71, 0xa7, 0xee, 0x21, 0x4e, 0xff,
  0x54, 0xac, 0xb1, 0xb7, 0xe3, 0x7f, 0x59, 0x5d, 0x78, 0xe2, 0x10, 0xcf,
  0x25, 0x71, 0xf8, 0x49, 0x08, 0xf0, 0x62, 0xc2, 0x7d, 0xf3, 0x86, 0x46,
  0x46, 0x23, 0xb6, 0xc6, 0x87, 0xc1, 0x96, 0x6f, 0x31, 0xab, 0x13, 0x06,
  0x41, 0x60, 0x72, 0x0e, 0x22, 0xd3, 0x09, 0x0e, 0x0c, 0xaa, 0xf1, 0x4b,
  0x7e, 0x19, 0x6e, 0xd7, 0x34, 0x95, 0x5d, 0x3a, 0x93, 0x91, 0x10, 0xb0,
  0xf6, 0xfd, 0xee, 0xc7, 0x11, 0xe0, 0x71, 0x38, 0xac, 0xa2, 0xf8, 0x98,
  0xa0, 0x90, 0x84, 0x98, 0x20, 0xb0, 0xd9, 0xac, 0x39, 0xa3, 0xa4, 0x36,
  0x1a, 0xba, 0x9f, 0x2b, 0x3b, 0x1a, 0xcd, 0x24, 0xf2, 0xaf, 0xdc, 0x86,
  0x46, 0x33, 0x21, 0xd9, 0x91, 0x1c, 0xe9, 0xbf, 0x32, 0xf1, 0x40, 0xdf,
  0xac, 0x04, 0xa2, 0x42, 0x7d, 0xce, 0x5b, 0x98, 0xcd, 0x4b, 0x48, 0x5c,
  0x17, 0x0c, 0x16, 0x8b, 0x81, 0x67, 0x05, 0x45, 0xc2, 0xb0, 0x46, 0x26,
  0x27, 0x48, 0xfc, 0x5a, 0x50, 0x8e, 0x2b, 0x37, 0x6b, 0xfe, 0x90, 0xc9,
  0xfb, 0x57, 0x69, 0xe5, 0x0c, 0xed, 0x62, 0x73, 0x5c, 0x68, 0x72, 0x7b,
  0xa7, 0x3c, 0x21, 0x3e, 0x7a, 0xa9, 0xce, 0xb9, 0xe1, 0x21, 0x54, 0xda,
  0x0d, 0xe5, 0xda, 0xba, 0x30, 0x94, 0x19, 0x82, 0xc9, 0x24, 0xf0, 0x46,
  0x4c, 0x10, 0x9c, 0x1c, 0x05, 0x91, 0xeb, 0xa3, 0x02, 0x77, 0xeb, 0x11,
  0x50, 0x37, 0xe6, 0x30, 0xc7, 0xc7, 0x35, 0xc7, 0x76, 0x24, 0x46, 0x82,
  0x6b, 0xcc, 0xd1, 0x8b, 0x44, 0x2f, 0x8a, 0x47, 0xf7, 0xd3, 0xe5, 0xd4,
  0x5a, 0x7b, 0xcd, 0xb6, 0x47, 0x67, 0x07, 0xd0, 0x81, 0x6d, 0x8a, 0x0d,
  0x45, 0x59, 0x45, 0x7d, 0xc6, 0xee, 0x9d, 0xb1, 0xa6, 0x3a, 0x02, 0x6f,
  0xa6, 0x67, 0xc6, 0x35, 0xb7, 0x75, 0xdb, 0x89, 0x17, 0x0a, 0xf1, 0xbc,
  0x98, 0x2d, 0xfa, 0xe9, 0x10, 0xd9, 0xf1, 0x11, 0xe8, 0xe7, 0x6a, 0x35,
  0x32, 0x32, 0xb6, 0x5d, 0x47, 0xa0, 0xab, 0xa7, 0x3f, 0x79, 0x75, 0xb8,
  0xcf, 0x8c, 0xcd, 0x54, 0xf1, 0x4c, 0xc7, 0xe8, 0xe8, 0xf8, 0x0c, 0xbd,
  0x6a, 0x68, 0x64, 0x4a, 0x37, 0xa6, 0xaf, 0xd3, 0x62, 0x7c, 0x5c, 0x03,
  0xb5, 0x7a, 0x54, 0x8f, 0x60, 0xe0, 0x62, 0x37, 0x5c, 0xbc, 0x5c, 0xb1,
  0x56, 0x47, 0x80, 0x60, 0x10, 0x01, 0x22, 0x01, 0x5f, 0xff, 0xf0, 0x89,
  0x49, 0xfc, 0x5e, 0x74, 0x0b, 0xb2, 0xee, 0xa9, 0x82, 0xa5, 0x1c, 0xe5,
  0x97, 0x54, 0x61, 0xe8, 0xd1, 0x61, 0x5d, 0xf2, 0x7e, 0x14, 0x5f, 0xbf,
  0x83, 0xca, 0x9a, 0x26, 0xda, 0xc9, 0x6f, 0x85, 0x7f, 0xd2, 0xff, 0x54,
  0x08, 0x8d, 0x12, 0x29, 0x7a, 0x7a, 0x07, 0xe9, 0xb5, 0xa4, 0x55, 0x86,
  0x4e, 0x79, 0x9f, 0x7e, 0x16, 0x04, 0x7c, 0x8a, 0x70, 0x88, 0x8e, 0xc0,
  0x8d, 0xbf, 0xea, 0x1d, 0x8c, 0xb9, 0x46, 0x7a, 0x46, 0x2c, 0x26, 0x03,
  0x11, 0xc1, 0xaf, 0xa3, 0xbc, 0xea, 0x3e, 0x7a, 0xfb, 0x94, 0x28, 0x2d,
  0xaf, 0xc3, 0x32, 0x3f, 0x37, 0xcc, 0xe3, 0x19, 0xd1, 0x93, 0xae, 0x55,
  0x2a, 0x47, 0x74, 0x98, 0x2f, 0x48, 0x72, 0x92, 0x6e, 0x57, 0x77, 0x27,
  0x7b, 0xb4, 0x75, 0xf6, 0x62, 0x6c, 0x5c, 0x83, 0xca, 0xda, 0x26, 0xb4,
  0x74, 0x74, 0xd3, 0xcf, 0x5d, 0xd6, 0xd5, 0x0f, 0x5b, 0xbe, 0x99, 0x5e,
  0x06, 0xac, 0x2c, 0x4d, 0xd0, 0xdb, 0xaf, 0xe4, 0x35, 0x97, 0x7f, 0x2b,
  0x64, 0x68, 0x0b, 0x87, 0xcb, 0x65, 0xcf, 0x48, 0x9f, 0xa5, 0x85, 0x09,
  0x56, 0x04, 0x79, 0x22, 0xbf, 0xa4, 0x12, 0xee, 0xce, 0x22, 0x08, 0x6c,
  0x2d, 0x69, 0x39, 0x35, 0x8a, 0x27, 0x34, 0x13, 0x50, 0x0c, 0xa8, 0x40,
  0x30, 0xa6, 0x3a, 0xc6, 0x41, 0x64, 0x43, 0x67, 0x4b, 0xae, 0x18, 0x84,
  0xa7, 0xbb, 0x23, 0x64, 0xf2, 0x3e, 0xba, 0xf5, 0x28, 0x1b, 0x53, 0x13,
  0xde, 0xac, 0x2d, 0xba, 0xf7, 0xb3, 0x6c, 0x3b, 0x7a, 0xd2, 0xb0, 0x98,
  0x4c, 0x0c, 0x8f, 0x8c, 0x81, 0x6b, 0xf4, 0x6f, 0x07, 0x68, 0x8d, 0xa5,
  0x5d, 0x0a, 0x38, 0xcd, 0x7f, 0x0d, 0x4d, 0x2d, 0x1d, 0x70, 0x10, 0x59,
  0xc3, 0x88, 0xc3, 0xa6, 0xa7, 0xa3, 0x8d, 0x95, 0x39, 0x94, 0x2a, 0x35,
  0x2c, 0xcd, 0x4d, 0x68, 0x3b, 0x8a, 0x54, 0x7b, 0x67, 0x0f, 0x54, 0x43,
  0xc3, 0x58, 0xee, 0xef, 0x41, 0xd7, 0x44, 0x6d, 0x43, 0x33, 0xec, 0x05,
  0x56, 0x33, 0x02, 0xa3, 0x88, 0x51, 0xc8, 0xc9, 0xce, 0xe8, 0xa0, 0xe9,
  0xbb, 0x39, 0xdb, 0x37, 0xaa, 0x54, 0x53, 0xc5, 0x34, 0x1d, 0xad, 0x1d,
  0x72, 0x3a, 0x92, 0x65, 0x7e, 0xee, 0x70, 0x75, 0x16, 0xa1, 0xa4, 0xac,
  0x46, 0x57, 0x88, 0x54, 0x14, 0x83, 0xaa, 0x11, 0x70, 0xd8, 0x4c, 0xdd,
  0x1c, 0x10, 0x09, 0xad, 0xc1, 0x64, 0x32, 0x60, 0x6c, 0xc4, 0x81, 0x83,
  0x9d, 0x35, 0xfe, 0x6e, 0x6c, 0x85, 0xc0, 0x66, 0xe6, 0x28, 0x57, 0x0c,
  0x28, 0x11, 0x1e, 0xe4, 0xa5, 0x26, 0xac, 0xa2, 0x64, 0x34, 0x01, 0x47,
  0x91, 0x4d, 0x59, 0x4b, 0x7b, 0x97, 0x9e, 0x11, 0x55, 0x6c, 0x0f, 0x9a,
  0xa5, 0x08, 0x0e, 0xf0, 0x00, 0x83, 0xc9, 0x80, 0xb3, 0xa3, 0x00, 0xf3,
  0xed, 0x6c, 0xd0, 0x37, 0xa0, 0xd2, 0xf5, 0xfa, 0xf0, 0xf0, 0x28, 0x2d,
  0xd3, 0x82, 0x7a, 0x0c, 0x3e, 0x9e, 0xce, 0x20, 0x18, 0x80, 0xad, 0xb5,
  0x39, 0xfc, 0xbc, 0xc5, 0x98, 0x2f, 0xb4, 0x9e, 0x41, 0x80, 0xca, 0x54,
  0xab, 0x54, 0x7e, 0x8d, 0x0e, 0x84, 0xfa, 0x09, 0x09, 0xf0, 0x08, 0xef,
  0xea, 0xe9, 0x2f, 0x3d, 0xf2, 0xd1, 0x56, 0xb0, 0x59, 0x73, 0xcf, 0xff,
  0x17, 0x85, 0xcc, 0xd3, 0x79, 0xe0, 0x19, 0x1b, 0x6d, 0x2b, 0x28, 0xa9,
  0xca, 0x9e, 0xea, 0x82, 0x8a, 0xfa, 0xab, 0x5c, 0x2e, 0xa7, 0xbc, 0xe2,
  0xee, 0x83, 0x97, 0xee, 0xbc, 0xf1, 0xa1, 0x14, 0x1c, 0x36, 0x5b, 0x9a,
  0x9f, 0xfb, 0xf9, 0x79, 0xea, 0x5e, 0xf7, 0x2e, 0x58, 0x17, 0x19, 0x90,
  0x7a, 0xfc, 0x9b, 0x5c, 0x35, 0xd5, 0x62, 0x2f, 0x0b, 0x54, 0xd1, 0x7e,
  0x7d, 0xa6, 0x80, 0xea, 0x96, 0x5d, 0x84, 0x69, 0x84, 0x5a, 0x8f, 0xc0,
  0x91, 0x93, 0x39, 0xf5, 0xef, 0xa5, 0xc6, 0xc6, 0x66, 0x9e, 0xca, 0x1b,
  0x6e, 0x68, 0x7a, 0xf1, 0x24, 0xa8, 0x41, 0x96, 0x75, 0xb6, 0x08, 0x1b,
  0xd6, 0x2e, 0x3b, 0x5e, 0xdb, 0xd0, 0x72, 0x51, 0x2b, 0x27, 0x0c, 0x0d,
  0x17, 0x88, 0x6c, 0x42, 0x58, 0x4c, 0x66, 0x4e, 0x64, 0xc8, 0x62, 0xc1,
  0x8a, 0xe5, 0x5e, 0x74, 0xdb, 0x3d, 0x2f, 0x5a, 0xda, 0xba, 0x70, 0xe2,
  0x54, 0x1e, 0xd6, 0x44, 0x2c, 0xf9, 0x22, 0x2b, 0xbb, 0x70, 0xef, 0x74,
  0x1d, 0x31, 0xdb, 0x86, 0x4f, 0xf7, 0x24, 0x0b, 0x8e, 0x9f, 0xca, 0xcd,
  0x0a, 0x0e, 0xf0, 0x88, 0x4b, 0xdb, 0xbc, 0xfa, 0x99, 0x1d, 0xf7, 0x28,
  0x06, 0x51, 0x7a, 0xb3, 0x16, 0x25, 0xe5, 0xf7, 0x7a, 0x3d, 0xdd, 0x1c,
  0x52, 0x2f, 0xdf, 0xb8, 0x7b, 0xc1, 0xd0, 0x86, 0x98, 0x6b, 0xf3, 0xf6,
  0xc4, 0x88, 0x68, 0x79, 0xef, 0x40, 0x61, 0xea, 0xe6, 0x28, 0xdd, 0x8b,
  0xa7, 0xbd, 0x53, 0x4e, 0x0f, 0x21, 0x1e, 0x4f, 0x7f, 0x6c, 0x4f, 0xc7,
  0xa0, 0x52, 0x8d, 0xe6, 0xb6, 0x6e, 0x54, 0xd7, 0x49, 0x50, 0x74, 0xad,
  0x5a, 0x15, 0xba, 0x74, 0xd1, 0x77, 0x29, 0xf1, 0x61, 0x07, 0x53, 0x76,
  0x67, 0x2a, 0x66, 0xb3, 0x67, 0xcd, 0x75, 0xd0, 0x9d, 0x7b, 0x92, 0x25,
  0x4b, 0x7d, 0x5c, 0xe9, 0x9e, 0xa7, 0xbe, 0xeb, 0xce, 0xe5, 0x5e, 0x47,
  0xdf, 0xe0, 0x50, 0x7b, 0x55, 0x9d, 0x44, 0x24, 0x5e, 0x20, 0x80, 0x9b,
  0x93, 0x08, 0x6c, 0x16, 0x83, 0xaa, 0x68, 0x0c, 0x8f, 0x8d, 0x61, 0x42,
  0x33, 0x89, 0xab, 0xb7, 0xea, 0xa8, 0xaf, 0xe6, 0x31, 0x63, 0x2e, 0xb7,
  0x34, 0xd0, 0xc7, 0xf5, 0xd2, 0xd9, 0xaf, 0xd2, 0x7f, 0xdc, 0xb8, 0xeb,
  0x73, 0x45, 0xf1, 0xb5, 0x6a, 0x3c, 0x35, 0x5c, 0x16, 0x0a, 0x6b, 0x0e,
  0x7d, 0xb0, 0x85, 0xdc, 0xff, 0xee, 0x46, 0xd2, 0x5e, 0xc0, 0x27, 0xdd,
  0xc5, 0xa2, 0x23, 0x64, 0xff, 0x65, 0x46, 0xd6, 0xd1, 0xb7, 0x39, 0xe2,
  0x05, 0x42, 0xbf, 0xed, 0x89, 0x11, 0xdb, 0x08, 0x82, 0x48, 0x63, 0xb1,
  0x98, 0x69, 0x00, 0xd2, 0x0e, 0x7f, 0xb8, 0x25, 0x29, 0x22, 0xd8, 0xdb,
  0x1b, 0x2f, 0x02, 0x3b, 0x93, 0x57, 0x79, 0x11, 0x04, 0x41, 0x9a, 0x9b,
  0xf1, 0x48, 0x0f, 0x17, 0x87, 0xeb, 0x19, 0xe9, 0x49, 0x7e, 0x78, 0x95,
  0xa8, 0x2c, 0x38, 0xc6, 0xce, 0x48, 0x4f, 0xfa, 0x78, 0xc3, 0x9a, 0xa0,
  0xe0, 0x57, 0xea, 0x18, 0xff, 0x45, 0xfc, 0x03, 0xc3, 0xe3, 0x63, 0xd8,
  0x09, 0xcf, 0xd3, 0x99, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
  0xae, 0x42, 0x60, 0x82
};
static const unsigned int ICON_HOVER_DATA_SIZE = 1540;

// Embedded floating notification icon (tim2.png, 64x64)
static const unsigned char FLOAT_ICON_DATA[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x08, 0x06, 0x00, 0x00, 0x00, 0xaa, 0x69, 0x71,
    0xde, 0x00, 0x00, 0x0b, 0xae, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0xed, 0x9b, 0x07, 0x5c, 0x54,
    0x47, 0x1a, 0xc0, 0xbf, 0xb7, 0xcb, 0xd2, 0x7b, 0x5b, 0xfa, 0x52, 0x76, 0x11, 0x01, 0xa9, 0x82,
    0x88, 0x34, 0x41, 0x3a, 0xd2, 0x11, 0x45, 0x04, 0x51, 0x8a, 0x80, 0x2d, 0x46, 0x83, 0x98, 0x90,
    0xc4, 0x18, 0x63, 0xee, 0x62, 0xb9, 0xb3, 0x90, 0x18, 0x8c, 0x31, 0x5e, 0x8c, 0x25, 0x89, 0x25,
    0x31, 0xf1, 0xf4, 0x42, 0x72, 0x7a, 0xa4, 0x18, 0xa3, 0xa2, 0x11, 0x13, 0x03, 0x01, 0x91, 0x2e,
    0x82, 0x20, 0x52, 0x94, 0x85, 0xb9, 0xdf, 0x0c, 0xbe, 0x75, 0x97, 0x45, 0xd9, 0x25, 0x04, 0x81,
    0xf0, 0xc7, 0xfd, 0xbd, 0xb7, 0x3b, 0xf3, 0x66, 0xe6, 0xfb, 0xe6, 0xfb, 0x66, 0xe6, 0x7d, 0x33,
    0x02, 0x4c, 0x30, 0xf2, 0x20, 0xf4, 0x1b, 0x05, 0x7f, 0x75, 0xd0, 0x84, 0x12, 0x46, 0x07, 0xd4,
    0x48, 0x55, 0x74, 0xfc, 0xbd, 0x75, 0xa6, 0x57, 0x7f, 0xb9, 0x69, 0xf6, 0xdd, 0x4f, 0xbf, 0x9a,
    0xca, 0xcb, 0xb1, 0x54, 0x2b, 0xaa, 0x6e, 0xab, 0x86, 0x07, 0xb8, 0x1a, 0x01, 0x02, 0xea, 0xb3,
    0x33, 0xe7, 0xab, 0x6c, 0xad, 0x38, 0x5d, 0xa5, 0x15, 0xb5, 0x4d, 0xfe, 0x9e, 0x0e, 0x35, 0x72,
    0xb2, 0xac, 0xdf, 0xf3, 0x5e, 0x5d, 0x5c, 0x4e, 0xa9, 0xf8, 0xf5, 0x8e, 0x49, 0x05, 0x7c, 0xb8,
    0x7d, 0x15, 0xe3, 0x66, 0x55, 0x83, 0xef, 0x85, 0x2b, 0x65, 0xde, 0x95, 0x35, 0x8d, 0xae, 0xca,
    0x8a, 0xf2, 0x6e, 0x45, 0x17, 0xae, 0xab, 0x8a, 0x55, 0x4e, 0xf5, 0x55, 0x8f, 0x10, 0x12, 0x2b,
    0xc3, 0xde, 0xda, 0xec, 0x81, 0xb6, 0xa6, 0xea, 0x25, 0x25, 0x45, 0xb9, 0xef, 0xb9, 0xa6, 0xfa,
    0xe7, 0x72, 0xb2, 0x62, 0x0a, 0xd9, 0x0e, 0x49, 0xf7, 0x25, 0xa9, 0x1f, 0xa1, 0xdf, 0x18, 0x14,
    0x65, 0x49, 0x94, 0x87, 0xd0, 0x0d, 0x8a, 0xa2, 0x26, 0x89, 0x57, 0x40, 0xb7, 0x01, 0x86, 0x91,
    0x05, 0x31, 0x33, 0x3d, 0x3b, 0x3a, 0x1f, 0xa4, 0x96, 0x57, 0xd6, 0x87, 0x5f, 0x2e, 0x29, 0x57,
    0x1f, 0x28, 0x8f, 0xaa, 0x8a, 0x22, 0x70, 0x4d, 0xf5, 0x41, 0x53, 0x4d, 0x19, 0x10, 0x20, 0x81,
    0x22, 0xda, 0x3b, 0x1e, 0x40, 0x79, 0x65, 0x3d, 0x34, 0xdc, 0x69, 0x11, 0x28, 0x06, 0x70, 0x8e,
    0x47, 0x4d, 0x77, 0x71, 0xe0, 0x75, 0x03, 0x42, 0x9f, 0x39, 0x4d, 0xe1, 0x1e, 0xdc, 0xf8, 0x42,
    0xe2, 0x09, 0x6d, 0xbb, 0x44, 0x3e, 0x8c, 0x06, 0xf8, 0x95, 0xc7, 0x64, 0x13, 0x22, 0xbd, 0xd2,
    0x3c, 0x5c, 0xad, 0xaf, 0x53, 0x14, 0x85, 0x9b, 0x2b, 0xf8, 0x4c, 0xb1, 0xe2, 0xa0, 0xb9, 0xe1,
    0x9e, 0x68, 0x55, 0x5a, 0x04, 0xba, 0x72, 0xe9, 0x3c, 0x92, 0x86, 0xe7, 0x33, 0x22, 0x51, 0x52,
    0xcc, 0x4c, 0x34, 0xd5, 0x9e, 0x2b, 0x52, 0x26, 0xae, 0xc3, 0xc6, 0xd2, 0xa4, 0x2e, 0x21, 0xd2,
    0x2b, 0xf7, 0x50, 0xfe, 0x1a, 0x81, 0x92, 0x11, 0x2a, 0x51, 0x1a, 0x71, 0xe1, 0x97, 0x2e, 0x0c,
    0x4d, 0xe1, 0x9a, 0xea, 0x97, 0x0b, 0x37, 0xce, 0xcd, 0x69, 0x12, 0xca, 0x4e, 0x0e, 0x41, 0xc3,
    0xcd, 0x8a, 0xc5, 0xb3, 0x91, 0xbf, 0x97, 0x03, 0xa9, 0x83, 0xae, 0x6f, 0x32, 0xcf, 0xb8, 0x35,
    0x2b, 0x39, 0x24, 0xef, 0xe2, 0xa9, 0xad, 0x0a, 0x23, 0x2a, 0x78, 0xe6, 0x82, 0x60, 0x1b, 0x5f,
    0x77, 0xbb, 0xef, 0x19, 0x0c, 0x8a, 0x34, 0x08, 0x5f, 0x23, 0x02, 0xa7, 0xa1, 0x37, 0xd6, 0xa5,
    0xa3, 0x91, 0x60, 0x7e, 0x94, 0x37, 0x52, 0x51, 0x56, 0x10, 0xd4, 0xed, 0xe1, 0x6a, 0x5d, 0xb1,
    0x73, 0x43, 0xfa, 0xec, 0x11, 0x11, 0x7e, 0x51, 0xfc, 0xac, 0x9c, 0xc9, 0x3c, 0xe3, 0x87, 0x74,
    0x8f, 0x63, 0xc1, 0x9f, 0x15, 0xd8, 0x45, 0x64, 0x64, 0x98, 0xc4, 0x1a, 0x64, 0x59, 0x32, 0x28,
    0x39, 0xce, 0xb7, 0x60, 0xc7, 0x86, 0x74, 0xf9, 0x3f, 0x45, 0xf0, 0x8c, 0xc4, 0x40, 0xc5, 0x84,
    0x48, 0xaf, 0xcf, 0xb1, 0xc6, 0x69, 0xff, 0x5e, 0x9b, 0x1d, 0x83, 0x9e, 0x35, 0x07, 0xf7, 0xed,
    0x44, 0x9e, 0xae, 0xd6, 0x02, 0x17, 0xf4, 0x75, 0xb7, 0xbb, 0xf8, 0x66, 0x6e, 0x12, 0x67, 0x58,
    0x85, 0xff, 0x72, 0xff, 0xcb, 0x86, 0x53, 0xed, 0xb9, 0x97, 0xe8, 0x4a, 0xe2, 0x67, 0x7b, 0xa0,
    0xd1, 0x46, 0x66, 0x52, 0x30, 0x62, 0x32, 0x19, 0x44, 0x11, 0x66, 0xc6, 0xec, 0xea, 0x1d, 0x1b,
    0xd2, 0x1d, 0x87, 0x45, 0xf8, 0x3d, 0x6f, 0x2d, 0x35, 0x0e, 0xf5, 0x9b, 0x5a, 0x41, 0xfb, 0xdb,
    0xd2, 0x85, 0xa1, 0x68, 0xb4, 0x92, 0x9b, 0x1d, 0x8b, 0xf4, 0x75, 0x35, 0x88, 0x12, 0x5c, 0x1d,
    0x78, 0xad, 0xdb, 0x5f, 0x4b, 0x73, 0xfb, 0x43, 0xc2, 0x6f, 0xc9, 0x4b, 0x51, 0x77, 0x9e, 0x62,
    0x71, 0x15, 0x0b, 0x8f, 0x07, 0x9d, 0xd1, 0x60, 0xf2, 0x92, 0xc0, 0x33, 0xd3, 0x27, 0x96, 0xea,
    0xe7, 0x61, 0xdf, 0xe2, 0xef, 0xe5, 0x60, 0x31, 0x24, 0xe1, 0x51, 0xef, 0x77, 0x4c, 0xdf, 0x19,
    0x76, 0x85, 0xf8, 0x96, 0x25, 0xc3, 0x44, 0xb9, 0x4b, 0x63, 0xd1, 0x58, 0xc2, 0xd8, 0x40, 0x9b,
    0x58, 0x82, 0xd7, 0x34, 0x9b, 0xf2, 0xf7, 0xb7, 0x2e, 0xd7, 0x90, 0x5a, 0x01, 0x61, 0xb3, 0x5c,
    0x36, 0xd2, 0x73, 0xee, 0xca, 0xd4, 0x70, 0x34, 0xd6, 0xc0, 0x83, 0xa3, 0x9a, 0x8a, 0x12, 0xb1,
    0x84, 0x10, 0xdf, 0xa9, 0x9f, 0x49, 0x25, 0xfc, 0x6b, 0xab, 0xe7, 0x7b, 0x72, 0x8c, 0x74, 0xf1,
    0x5a, 0x9a, 0x4c, 0x35, 0x63, 0x95, 0x55, 0xe9, 0x11, 0x82, 0x45, 0x53, 0x5a, 0x42, 0x40, 0x9a,
    0x44, 0xc2, 0xf3, 0x2b, 0x8f, 0xc9, 0xf8, 0x79, 0xd8, 0x97, 0xe0, 0x87, 0xf0, 0xaa, 0x6e, 0xac,
    0x13, 0xee, 0xef, 0x4a, 0xac, 0x60, 0x86, 0xcb, 0xe4, 0x7b, 0xfb, 0xb6, 0xae, 0xd0, 0x16, 0x96,
    0x15, 0xa1, 0x62, 0xf1, 0x77, 0xa1, 0xc5, 0x73, 0xfd, 0xd3, 0x98, 0x0c, 0x06, 0x79, 0x68, 0xd7,
    0x5b, 0x2f, 0xa2, 0xf1, 0x00, 0x9e, 0x19, 0xf0, 0x0c, 0x16, 0xe4, 0xe3, 0xb4, 0xed, 0xa9, 0xbd,
    0x5f, 0x52, 0xb8, 0x43, 0x26, 0xd0, 0xdb, 0xb1, 0x1a, 0x0b, 0x1f, 0x13, 0xe2, 0x8e, 0xc6, 0x0b,
    0xf8, 0xdd, 0x04, 0x77, 0x38, 0xcf, 0xcc, 0xa0, 0x6b, 0xef, 0xe6, 0x65, 0x86, 0x03, 0x0a, 0x8f,
    0xd0, 0x35, 0xd6, 0xba, 0x65, 0x71, 0x09, 0xb4, 0xcf, 0x8c, 0x37, 0x8c, 0xf4, 0xb5, 0x88, 0x5c,
    0x29, 0x73, 0xfc, 0x5e, 0x17, 0x96, 0x9b, 0x41, 0xdf, 0x50, 0x94, 0x6d, 0x77, 0x71, 0x49, 0x79,
    0x12, 0x7e, 0x17, 0x0f, 0xf1, 0x75, 0x86, 0xf1, 0x86, 0xbf, 0xa7, 0x03, 0x89, 0x33, 0xfc, 0x56,
    0x5e, 0x9b, 0x34, 0x60, 0x86, 0xc3, 0xf9, 0x6b, 0x34, 0x0d, 0xf5, 0xb4, 0x7a, 0xb0, 0x96, 0xd6,
    0x66, 0x8d, 0x8d, 0x05, 0x8f, 0xb4, 0xd0, 0xd3, 0xfa, 0xb2, 0x94, 0x30, 0x77, 0x31, 0x0b, 0x28,
    0xfc, 0xf6, 0x6a, 0x40, 0x4d, 0x7d, 0x13, 0x43, 0x4e, 0x96, 0x05, 0x9b, 0x76, 0x7d, 0x02, 0xe3,
    0x11, 0x27, 0x5b, 0x73, 0x72, 0xad, 0xaa, 0xbb, 0x13, 0x26, 0xa6, 0x80, 0x5b, 0x35, 0x8d, 0x1e,
    0xf8, 0xea, 0xe5, 0x66, 0x23, 0x51, 0x61, 0x6b, 0x96, 0x44, 0x43, 0x52, 0xcc, 0xcc, 0x27, 0xa6,
    0x2f, 0x5b, 0x14, 0x86, 0x17, 0x53, 0x90, 0xbf, 0xf9, 0xa5, 0x01, 0xd3, 0x1b, 0xea, 0x6a, 0x20,
    0x3e, 0xdc, 0x13, 0x02, 0xbc, 0x1c, 0x61, 0xcd, 0x92, 0x28, 0xc1, 0xef, 0xf3, 0xa3, 0x7c, 0x20,
    0x7d, 0x7e, 0x20, 0x48, 0xc3, 0xdc, 0x70, 0x4f, 0x08, 0xf2, 0x71, 0x1a, 0x34, 0x9f, 0xb5, 0xa5,
    0x09, 0xb9, 0xfe, 0x7e, 0xb3, 0x6e, 0x9a, 0x98, 0x02, 0x9a, 0x5b, 0xda, 0xec, 0xb1, 0x8f, 0x70,
    0x0c, 0x75, 0x24, 0xaa, 0xf4, 0x72, 0x49, 0x39, 0xd4, 0x37, 0xb6, 0x40, 0x5a, 0x42, 0x80, 0x58,
    0x1a, 0x16, 0xfa, 0x46, 0x59, 0x0d, 0x74, 0xf3, 0x7b, 0xa0, 0xa1, 0x06, 0x07, 0x8c, 0x44, 0x59,
    0xbe, 0x28, 0x0c, 0x12, 0xe2, 0x42, 0xa0, 0xa9, 0xf9, 0x1e, 0x30, 0x18, 0x14, 0x74, 0x76, 0xe1,
    0xf0, 0x02, 0xc0, 0x2b, 0xab, 0x12, 0xe0, 0x4e, 0xf3, 0x3d, 0xa8, 0xac, 0x69, 0x14, 0x7b, 0xa6,
    0xf0, 0xd4, 0x31, 0x08, 0xf4, 0x71, 0x1a, 0xb0, 0xbe, 0xe6, 0xd6, 0xfb, 0x20, 0xcb, 0x92, 0x19,
    0xb4, 0xcd, 0x6c, 0x6d, 0x35, 0x32, 0x0e, 0x28, 0x2a, 0xc8, 0xb9, 0x88, 0x29, 0x40, 0x45, 0x59,
    0xc1, 0x06, 0x47, 0x67, 0x35, 0xd4, 0x94, 0x41, 0x12, 0x9c, 0xa7, 0x58, 0x00, 0x93, 0xc9, 0x80,
    0xaa, 0xda, 0x3b, 0xa4, 0xb7, 0x85, 0xf9, 0xf7, 0xe9, 0x33, 0xc0, 0x60, 0x52, 0x60, 0x67, 0xc5,
    0x81, 0xf5, 0xdb, 0x3e, 0x12, 0x49, 0x4b, 0x88, 0xf4, 0x86, 0xd2, 0x8a, 0x3a, 0x70, 0x75, 0xe0,
    0xc1, 0x57, 0x45, 0x57, 0xa0, 0xb7, 0x17, 0x41, 0x47, 0xe7, 0x03, 0x92, 0xb6, 0x7e, 0xeb, 0x47,
    0xa0, 0xaf, 0xab, 0x41, 0xe6, 0xa1, 0xe7, 0xd2, 0x22, 0x44, 0x9e, 0xdb, 0xbf, 0x77, 0x07, 0xe0,
    0x58, 0x29, 0xae, 0x4f, 0x98, 0x43, 0x1f, 0xec, 0x22, 0xed, 0x30, 0x36, 0x10, 0x59, 0xe3, 0x0c,
    0xc8, 0x5b, 0xef, 0x1c, 0x23, 0x11, 0xe8, 0x1f, 0x8b, 0x4b, 0x55, 0x8a, 0x8e, 0xbd, 0xa9, 0x27,
    0x50, 0x00, 0x42, 0xe7, 0x59, 0x57, 0x7f, 0xa9, 0x24, 0x2f, 0x0c, 0xa6, 0x16, 0xd6, 0x12, 0x29,
    0xe0, 0x6f, 0xf9, 0x9f, 0x82, 0xb5, 0xa5, 0x31, 0xf4, 0xf4, 0xf6, 0xe2, 0x91, 0x15, 0x36, 0xbd,
    0x94, 0x41, 0x7e, 0xc7, 0x66, 0xdd, 0xf9, 0xe0, 0x21, 0x58, 0x70, 0xf4, 0x49, 0x85, 0xc2, 0x60,
    0x97, 0xb8, 0x77, 0xbf, 0x03, 0x4e, 0x7d, 0x73, 0x11, 0x36, 0xed, 0xec, 0x1b, 0x67, 0x70, 0x8f,
    0xc8, 0xcb, 0xb1, 0x1e, 0x97, 0xbb, 0x7d, 0x3f, 0x50, 0x0c, 0x80, 0x9b, 0x55, 0x0d, 0x22, 0xcf,
    0xd6, 0x35, 0xdc, 0x05, 0x0a, 0xff, 0x31, 0x28, 0x11, 0xb7, 0xfa, 0xf6, 0xec, 0x29, 0x40, 0xbd,
    0x08, 0x8c, 0x8c, 0x8c, 0x25, 0x6a, 0x77, 0x9f, 0xbc, 0x08, 0xf6, 0x1e, 0xfa, 0x8a, 0xf8, 0x03,
    0x51, 0xc0, 0xfb, 0x5b, 0x0f, 0xa8, 0x34, 0x36, 0xb5, 0x92, 0xc4, 0xac, 0xd5, 0x22, 0xd3, 0xe4,
    0x53, 0xd9, 0xb2, 0xfb, 0x38, 0x4c, 0xb5, 0xe3, 0x92, 0xfb, 0xb3, 0x45, 0x17, 0x20, 0x75, 0x5e,
    0x00, 0xb4, 0xb4, 0xb6, 0x93, 0xde, 0xd8, 0xb5, 0xef, 0x0b, 0x91, 0xbc, 0x2f, 0x64, 0x46, 0x03,
    0x9f, 0xdf, 0x03, 0x3d, 0x3d, 0xbd, 0xb0, 0x4a, 0xa8, 0x77, 0x71, 0x68, 0x5c, 0x46, 0xe6, 0xb1,
    0xf9, 0xb2, 0xf5, 0x0d, 0x41, 0x49, 0x41, 0x9e, 0x28, 0x91, 0x26, 0x6f, 0x65, 0x3c, 0x51, 0x94,
    0xba, 0x9a, 0x12, 0xf4, 0xf6, 0x20, 0x28, 0xf9, 0x19, 0xc7, 0x66, 0xfa, 0x68, 0xbb, 0xdf, 0x49,
    0xca, 0xcc, 0xdd, 0xb8, 0x1b, 0xa4, 0xc1, 0x50, 0x4f, 0x8b, 0x84, 0xce, 0x88, 0x02, 0x3e, 0xf8,
    0xf8, 0x6b, 0xa1, 0x58, 0xbc, 0x74, 0xe0, 0x9e, 0xb4, 0xe6, 0x19, 0x93, 0xf8, 0x3d, 0x36, 0x4f,
    0x0d, 0x75, 0x65, 0xac, 0x5d, 0xb1, 0x7c, 0x7f, 0x7f, 0xfb, 0x28, 0x38, 0xdb, 0x71, 0x41, 0x4d,
    0x55, 0x09, 0x7e, 0xbf, 0x55, 0x0f, 0x2f, 0x2e, 0x9f, 0x43, 0x7e, 0xc7, 0x63, 0x40, 0x7f, 0xff,
    0xe5, 0x18, 0xe9, 0x00, 0xea, 0x05, 0x58, 0x9b, 0x1d, 0x43, 0xbe, 0xdf, 0xaa, 0xc6, 0x63, 0x02,
    0x82, 0x83, 0xc7, 0xcf, 0x81, 0x92, 0xa2, 0x9c, 0x88, 0x1b, 0xe0, 0x31, 0x03, 0xef, 0x35, 0x48,
    0xcb, 0xe6, 0xdd, 0xc7, 0x88, 0x86, 0x89, 0x02, 0x32, 0x12, 0x03, 0x07, 0xdc, 0x9d, 0x91, 0x94,
    0x6d, 0x05, 0x27, 0xc8, 0xf3, 0xf8, 0xaf, 0xa3, 0xa3, 0xeb, 0x89, 0xf9, 0x36, 0x6e, 0x3f, 0x02,
    0x87, 0x4e, 0x9c, 0x83, 0x13, 0xa7, 0xcf, 0x93, 0xfb, 0x4d, 0x2f, 0xf6, 0xb9, 0x8d, 0x9c, 0x90,
    0x0b, 0x60, 0xb6, 0xbe, 0x7b, 0x82, 0xf8, 0x7b, 0x4d, 0x7d, 0x33, 0xf9, 0x5e, 0xdf, 0x78, 0x17,
    0xcc, 0x4c, 0xd8, 0xe4, 0xde, 0x80, 0xad, 0x09, 0x0f, 0xf9, 0x7c, 0xc1, 0x4c, 0xf2, 0xf0, 0x21,
    0x1f, 0x74, 0xb5, 0xd5, 0xa4, 0x6e, 0x73, 0x4a, 0xfc, 0x2c, 0xa6, 0x40, 0x01, 0xf3, 0xb2, 0x17,
    0xdf, 0xa3, 0x47, 0xff, 0xbc, 0x15, 0xf1, 0x52, 0x17, 0x36, 0xdb, 0xdf, 0x95, 0x0c, 0x44, 0xa6,
    0x46, 0xba, 0xd0, 0xdd, 0xd3, 0x03, 0x31, 0x21, 0x82, 0x75, 0xc6, 0x53, 0x69, 0x6d, 0x69, 0x22,
    0x03, 0x9e, 0xa1, 0x89, 0xa5, 0x58, 0x1a, 0x93, 0xc9, 0x04, 0xec, 0x96, 0x49, 0xb1, 0xbe, 0x64,
    0x03, 0x2b, 0x2e, 0xa1, 0x4f, 0x59, 0x1e, 0x33, 0x43, 0x88, 0x1b, 0x64, 0x24, 0x06, 0xc1, 0xe6,
    0xf5, 0x4b, 0x49, 0xbd, 0x6a, 0x52, 0x58, 0x00, 0xb6, 0x74, 0xfc, 0x31, 0xd6, 0xd7, 0x7a, 0x6c,
    0x01, 0x14, 0x65, 0xcd, 0xd7, 0xd3, 0xd5, 0xa8, 0xc6, 0xf7, 0xf4, 0x88, 0x2c, 0x29, 0x71, 0x61,
    0x33, 0xa0, 0xbb, 0x9b, 0x0f, 0x76, 0x93, 0x4d, 0xe1, 0xdd, 0x03, 0xa7, 0x41, 0x5b, 0x43, 0x15,
    0xda, 0x3b, 0xba, 0x20, 0x36, 0x74, 0x70, 0x25, 0x74, 0x74, 0x76, 0x11, 0xd7, 0x19, 0x68, 0xdc,
    0x31, 0xd4, 0xd3, 0x24, 0xd7, 0xdb, 0x4d, 0x2d, 0xc0, 0x62, 0x31, 0xc1, 0x2f, 0xb8, 0x6f, 0xad,
    0x30, 0x37, 0x39, 0x1b, 0x64, 0x65, 0x65, 0xe0, 0x66, 0xd5, 0x6d, 0xb8, 0x5e, 0x5a, 0x45, 0xac,
    0xee, 0xcd, 0x5d, 0x9f, 0x4a, 0xd4, 0xd6, 0xab, 0x97, 0x7f, 0x24, 0x96, 0x8a, 0x5d, 0xce, 0x6b,
    0x9a, 0xcd, 0xaf, 0x02, 0x05, 0x60, 0xe4, 0x64, 0x59, 0xbf, 0x62, 0xcd, 0xdc, 0x7e, 0x34, 0x18,
    0x4a, 0xba, 0x00, 0xa1, 0x07, 0x3d, 0x3c, 0x2b, 0x60, 0xfe, 0xf5, 0xe9, 0x37, 0x78, 0x9e, 0x85,
    0xfb, 0x1d, 0x5d, 0x10, 0x3f, 0xdb, 0xf3, 0xa9, 0xcf, 0xb7, 0xb6, 0x75, 0x08, 0xf6, 0x07, 0xfb,
    0x43, 0xa6, 0x63, 0x0a, 0xff, 0xa3, 0xc4, 0xa6, 0x38, 0xec, 0x06, 0x78, 0xa6, 0xc0, 0x53, 0xa8,
    0xa2, 0xbc, 0x9c, 0xc4, 0xed, 0xdd, 0x97, 0xbf, 0x91, 0x5c, 0x5d, 0x1c, 0x78, 0x0d, 0x33, 0xa2,
    0xd6, 0xb6, 0xf5, 0x5f, 0x07, 0xe0, 0xcd, 0x3b, 0x28, 0xad, 0xa8, 0x95, 0xa8, 0xb0, 0x45, 0x73,
    0x67, 0x41, 0x4b, 0x5b, 0x3b, 0x68, 0xaa, 0x2b, 0x43, 0xc1, 0x47, 0x67, 0x44, 0xd2, 0x8e, 0x9e,
    0xfa, 0x1e, 0x98, 0x0c, 0x06, 0xb4, 0xde, 0x6f, 0x87, 0xb9, 0x11, 0x4f, 0x56, 0x02, 0x36, 0x71,
    0x79, 0x59, 0x51, 0xff, 0xa7, 0xa1, 0x15, 0x8a, 0x85, 0x7c, 0x7b, 0xff, 0x29, 0x91, 0xb4, 0xf8,
    0xc4, 0x25, 0xc4, 0x0d, 0x70, 0x7b, 0x79, 0x66, 0x06, 0x20, 0x29, 0x35, 0x0d, 0xcd, 0xc4, 0xfc,
    0x99, 0x0c, 0xc6, 0x05, 0xfa, 0x37, 0x06, 0x7d, 0x63, 0x69, 0x6e, 0xf0, 0x35, 0x4e, 0xfc, 0xb1,
    0xb8, 0x74, 0xd0, 0x82, 0x72, 0xb2, 0x62, 0xa0, 0xba, 0xb6, 0x09, 0x18, 0x14, 0x05, 0x87, 0x3f,
    0x2f, 0x1a, 0x30, 0x8f, 0x23, 0x5e, 0x77, 0x23, 0x00, 0x05, 0xb9, 0x27, 0xf7, 0x10, 0x16, 0x8e,
    0x67, 0xfe, 0x64, 0x01, 0x64, 0x59, 0x2c, 0xb0, 0xb4, 0x10, 0x4f, 0xc7, 0xee, 0x60, 0xc5, 0x35,
    0x22, 0x03, 0x23, 0xad, 0x28, 0x49, 0xb8, 0x78, 0xb5, 0x8c, 0x5c, 0x39, 0x46, 0x3a, 0xff, 0x13,
    0x4b, 0x44, 0xcd, 0x67, 0x58, 0xae, 0x0e, 0x3c, 0x3c, 0xec, 0xa2, 0xcc, 0x05, 0xc1, 0x83, 0xbe,
    0x59, 0x0d, 0xc7, 0x3e, 0x60, 0x7d, 0x6d, 0x35, 0x1a, 0x29, 0x5e, 0x5e, 0x39, 0x4f, 0xb0, 0x85,
    0x96, 0xff, 0xc6, 0x12, 0xde, 0x80, 0x1a, 0x8a, 0x0e, 0x9e, 0x9e, 0x8f, 0x5f, 0x19, 0xf1, 0xb6,
    0xd7, 0x78, 0x23, 0x36, 0xd4, 0x9d, 0x28, 0xc0, 0xdb, 0xcd, 0xf6, 0xbc, 0xb0, 0xcc, 0x0c, 0xe1,
    0x2f, 0x31, 0x21, 0xee, 0xf9, 0x6a, 0x2a, 0x8a, 0xe8, 0xda, 0x8d, 0x5b, 0xb0, 0x32, 0x35, 0x1c,
    0xc6, 0x13, 0xc7, 0x4f, 0xf7, 0xc9, 0x6d, 0x6b, 0x65, 0x52, 0xf0, 0xd4, 0x8c, 0xb3, 0xfd, 0x5d,
    0x0e, 0x60, 0x4d, 0x71, 0x8c, 0x74, 0xd1, 0x78, 0x21, 0x3a, 0x78, 0x3a, 0xbd, 0x85, 0x5f, 0xff,
    0x46, 0xce, 0x82, 0x81, 0x47, 0x5d, 0x84, 0x4a, 0xc9, 0x5a, 0xf8, 0x68, 0xc1, 0x5a, 0x73, 0x3d,
    0x1d, 0xf5, 0x36, 0xec, 0x0a, 0x51, 0x41, 0x6e, 0x68, 0xac, 0xb3, 0x6e, 0x59, 0x9c, 0x60, 0x6f,
    0x20, 0x39, 0xce, 0x77, 0xa1, 0x44, 0xe6, 0xf2, 0x42, 0x66, 0x74, 0x3a, 0x1d, 0x1a, 0x5f, 0x14,
    0x3f, 0x0b, 0x8d, 0x65, 0xb8, 0xa6, 0x7d, 0xfb, 0x84, 0xc1, 0x33, 0x9d, 0xff, 0x2b, 0x95, 0xcf,
    0x04, 0xf9, 0x38, 0xbd, 0x47, 0x6b, 0x0e, 0x6f, 0x3d, 0x8f, 0x45, 0xa6, 0x3b, 0x4d, 0x22, 0xc2,
    0x3b, 0xd9, 0x9a, 0x37, 0x9e, 0xdc, 0x97, 0x67, 0xfa, 0xd8, 0xda, 0xaf, 0x0d, 0xfe, 0xe6, 0xf7,
    0xe1, 0xf6, 0x55, 0xac, 0xc8, 0x20, 0xb7, 0x33, 0xf4, 0xb6, 0x78, 0x5a, 0x42, 0x00, 0x1a, 0x4b,
    0xf8, 0x4c, 0xb7, 0x25, 0x9d, 0xc7, 0xd6, 0x51, 0x7f, 0x90, 0x93, 0x15, 0xed, 0x07, 0x43, 0xe1,
    0xfc, 0xe7, 0x9b, 0x95, 0xbc, 0xdd, 0x6c, 0x4f, 0x62, 0x25, 0x90, 0xe3, 0x30, 0x01, 0xcf, 0xee,
    0x38, 0x8c, 0x34, 0x4c, 0x77, 0xb6, 0xa2, 0x0f, 0x51, 0xa1, 0xd5, 0x19, 0x91, 0xf3, 0xe1, 0x8f,
    0x50, 0xf9, 0xc3, 0x1e, 0xd9, 0x88, 0x80, 0x69, 0xc4, 0x1d, 0x28, 0x0a, 0x90, 0xf3, 0x14, 0x0b,
    0x34, 0x5a, 0x79, 0x69, 0xf9, 0x1c, 0x64, 0xf1, 0xc8, 0xe7, 0x6d, 0x27, 0x71, 0xf8, 0x8b, 0xe7,
    0xf9, 0xf7, 0x05, 0x1d, 0x86, 0x83, 0xac, 0xe4, 0x90, 0x2c, 0xb6, 0xb6, 0x7a, 0x07, 0x2e, 0x1c,
    0x1f, 0x45, 0x49, 0x8e, 0xf3, 0x45, 0xa3, 0x89, 0x84, 0x48, 0x2f, 0xc1, 0x68, 0xef, 0x62, 0xcf,
    0x6b, 0xcd, 0x5b, 0x11, 0x3f, 0x34, 0xb3, 0x7f, 0x1a, 0xff, 0x78, 0x35, 0xd5, 0x71, 0x86, 0xcb,
    0xe4, 0xe2, 0xd1, 0x70, 0x42, 0x4c, 0x78, 0x0b, 0xdc, 0xca, 0xc2, 0x90, 0xb4, 0x07, 0x7f, 0x38,
    0x86, 0x3a, 0xe7, 0x16, 0xc4, 0xf8, 0x0c, 0xef, 0x21, 0x29, 0x61, 0x5a, 0xaf, 0x1f, 0x94, 0x73,
    0x75, 0xe0, 0xed, 0xc5, 0x95, 0xcd, 0xf2, 0xb4, 0x7f, 0x66, 0x82, 0x3f, 0x97, 0x1a, 0x8e, 0x1c,
    0x6d, 0xcd, 0x05, 0xbd, 0xce, 0x31, 0xd2, 0xe9, 0x48, 0x9f, 0x1f, 0xf8, 0x4a, 0xe1, 0xa1, 0x0d,
    0x7f, 0xfe, 0x01, 0xf0, 0x94, 0x39, 0x7e, 0x5b, 0x70, 0xa5, 0x21, 0xbe, 0xce, 0x23, 0x2a, 0x34,
    0xde, 0xae, 0x4f, 0x8a, 0x9d, 0x89, 0x4c, 0x0c, 0x75, 0x04, 0xc7, 0xe2, 0xd8, 0xda, 0xea, 0xb8,
    0x23, 0x0e, 0x47, 0x05, 0xb9, 0x49, 0x1e, 0x16, 0x16, 0x42, 0x66, 0x28, 0x0f, 0xd5, 0x34, 0x34,
    0x9b, 0xe1, 0xab, 0xba, 0xaa, 0xd2, 0x13, 0x5f, 0x97, 0xf7, 0x1c, 0xfc, 0x0f, 0x4c, 0xb5, 0xe7,
    0x82, 0x99, 0x31, 0x1b, 0x74, 0x34, 0x55, 0x61, 0xc3, 0x3f, 0x0f, 0x0f, 0xa5, 0x2a, 0x78, 0x3e,
    0x23, 0x92, 0x04, 0x41, 0x8b, 0x4b, 0xca, 0xc9, 0x7e, 0x02, 0x8d, 0x99, 0x09, 0xbb, 0xc7, 0xcd,
    0xd1, 0xf2, 0x13, 0x2d, 0x0d, 0xd5, 0x8d, 0x3b, 0xf7, 0x7d, 0xf1, 0xf3, 0x90, 0x0a, 0x87, 0x21,
    0x2a, 0xa0, 0xa9, 0xf9, 0x9e, 0x0b, 0x8e, 0x1d, 0xe8, 0x68, 0x89, 0x07, 0x23, 0xb3, 0x17, 0x86,
    0x92, 0x00, 0x06, 0x0e, 0x56, 0x9c, 0x39, 0x7b, 0x99, 0xfc, 0x86, 0xf3, 0xbe, 0xbe, 0xfd, 0x08,
    0xe8, 0xe9, 0x6a, 0x90, 0x08, 0xb2, 0x92, 0x82, 0x1c, 0x89, 0x0e, 0xe3, 0x78, 0x1e, 0x8e, 0x29,
    0xe0, 0xbc, 0xbd, 0x08, 0xc1, 0xc3, 0x6e, 0x3e, 0x34, 0xdf, 0x6d, 0x83, 0xbb, 0xad, 0xed, 0x50,
    0x7c, 0xbd, 0x82, 0x84, 0xda, 0xfa, 0xc7, 0xf2, 0x02, 0xbc, 0x1c, 0x6e, 0xe0, 0x65, 0x8a, 0x01,
    0x5b, 0xb3, 0xe0, 0xfd, 0x23, 0x85, 0xa2, 0x9b, 0x07, 0x23, 0x41, 0x4e, 0x56, 0xb4, 0x15, 0xbd,
    0x38, 0xea, 0x0f, 0x3e, 0xcc, 0x4c, 0x9b, 0xa6, 0xa7, 0xab, 0xf5, 0xd9, 0x79, 0x11, 0x5e, 0x7b,
    0xdc, 0x9d, 0xad, 0xae, 0x9b, 0x99, 0xb0, 0x45, 0x4e, 0x7b, 0xf7, 0xbf, 0xef, 0x7f, 0xca, 0x5c,
    0xf8, 0xe3, 0xe6, 0x34, 0xa9, 0xc5, 0xd3, 0xd5, 0xfa, 0xcb, 0xa5, 0x0b, 0x43, 0x73, 0x8f, 0x16,
    0xe4, 0xda, 0x20, 0x74, 0x77, 0x58, 0x7d, 0x9c, 0x92, 0xf6, 0x81, 0x39, 0x61, 0x1e, 0xeb, 0x8f,
    0x9c, 0x2c, 0x7a, 0xd9, 0xca, 0xc2, 0x10, 0x7e, 0x29, 0x23, 0x71, 0x54, 0xb2, 0xd1, 0x71, 0xe4,
    0x64, 0x11, 0xd4, 0x36, 0xdc, 0x25, 0xa1, 0xb0, 0x10, 0x5f, 0xe7, 0x5d, 0x7b, 0xb7, 0x2c, 0x7f,
    0x5e, 0xdb, 0x2e, 0x91, 0x44, 0x58, 0x3b, 0xcb, 0x3e, 0x56, 0xde, 0xb4, 0xf3, 0x13, 0x9b, 0xb2,
    0x9b, 0x75, 0x93, 0xb5, 0x35, 0x55, 0xb9, 0x3f, 0x5c, 0xba, 0xc1, 0x36, 0x36, 0xd0, 0x51, 0x36,
    0x60, 0x6b, 0x9a, 0x55, 0x56, 0xdf, 0x06, 0x16, 0x4b, 0x06, 0x47, 0x69, 0x3a, 0x7e, 0xba, 0x52,
    0x76, 0xcb, 0x40, 0x4f, 0xb3, 0x5d, 0x5b, 0x43, 0xb5, 0xd2, 0x48, 0x5f, 0xab, 0x54, 0x43, 0x5d,
    0xf9, 0x5a, 0x46, 0x4e, 0xfe, 0xe0, 0x21, 0xaa, 0x91, 0x02, 0xb5, 0x15, 0x32, 0x4d, 0x8d, 0x75,
    0xcb, 0x70, 0x8f, 0xe1, 0xa5, 0xf1, 0xea, 0x25, 0x51, 0x64, 0x61, 0x44, 0x4f, 0x41, 0x81, 0xde,
    0x8e, 0x4d, 0x59, 0xc9, 0x21, 0x8f, 0xb7, 0x7a, 0xc7, 0x12, 0x08, 0x95, 0x0c, 0x3a, 0x1e, 0x64,
    0x26, 0x05, 0xc7, 0xd2, 0xe6, 0x6f, 0x6e, 0xc2, 0x16, 0x98, 0xae, 0x82, 0xbc, 0x2c, 0x7f, 0x8a,
    0x15, 0x67, 0xe7, 0x6b, 0xab, 0x13, 0x06, 0xfc, 0x5f, 0x22, 0xe3, 0x06, 0x77, 0x67, 0xab, 0x54,
    0x61, 0xff, 0xd5, 0xd5, 0x56, 0x43, 0xbe, 0x33, 0xec, 0x8e, 0x2e, 0x4b, 0x09, 0xb5, 0x85, 0xbf,
    0x02, 0xef, 0x6c, 0xca, 0xd4, 0x70, 0xb6, 0xe3, 0x7e, 0xe5, 0x33, 0xdd, 0xb6, 0x2b, 0x22, 0x60,
    0xda, 0x81, 0xbc, 0x15, 0xf1, 0x76, 0xcf, 0xba, 0x4d, 0x13, 0x4c, 0x30, 0xc1, 0x04, 0x13, 0xc0,
    0xd0, 0xf9, 0x3f, 0x5e, 0x09, 0x9d, 0x72, 0xa7, 0x0e, 0x9a, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x49,
    0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int FLOAT_ICON_DATA_SIZE = 3047;

// Nyan Cat animation frame textures (populated via LoadFromMemory callback)
static Texture_t* s_NyanFrames[NYAN_FRAME_COUNT] = {};

static void NyanFrameCallback(const char* aId, Texture_t* aTex) {
    int i = atoi(aId + 16); // skip "TEX_TYRIAN_NYAN_"
    if (i >= 0 && i < NYAN_FRAME_COUNT)
        s_NyanFrames[i] = aTex;
}

// URL toast notification queue
struct UrlToast {
    std::string     url;
    std::string     domain;
    std::string     charName;
    ChatMessageType chanType;
};
static std::deque<UrlToast> g_UrlToastQueue;
static std::mutex           g_UrlToastMutex;
static bool                 g_UrlToastEnabled = true;
static bool                 g_UrlToastLocked  = true;
static bool                 g_UrlToastShowDummy = false;
static float                g_UrlToastX       = -1.0f;
static float                g_UrlToastY       = -1.0f;

// Global variables
HMODULE hSelf;
AddonDefinition_t AddonDef{};
AddonAPI_t* APIDefs = nullptr;
bool g_WindowVisible = false;

// UI State
static std::string g_SelectedContact;
static std::string g_PendingDeleteContact;
static bool        g_ConfirmClearHistoryOpen = false;
static bool g_ContextMenuContactPinned = false;
static std::unordered_map<std::string, std::string> g_ContactNotes;
static std::string g_NoteEditContact;
static char        g_NoteEditBuf[500] = {};
static bool        g_NoteEditOpen     = false;
static std::vector<std::string> g_PinnedContactNames;
static std::string g_ClipboardMsg;
static uint64_t g_SessionStartMs = 0;
static std::unordered_map<uint64_t, double> g_MessageArrivalTimes;
static double g_ClipboardMsgExpiry = 0.0;
static std::atomic<bool> g_ScrollToBottom{false};
static std::atomic<bool> g_LinksDirty{false};
static std::string g_DataDir;

// AE2 build import popup state

// Floating icon state
static bool g_ShowFloatingIcon = true;
static float g_FloatingIconX = 100.0f;
static float g_FloatingIconY = 100.0f;
static bool g_FloatingIconLocked = true;
static float g_FloatingIconScale = 1.0f;
static bool g_FloatingIconOnlyOnUnread = false;
static float g_FloatingIconFlashDuration = 3.0f;
static bool g_ShowTimestamps = true;
static std::atomic<uint64_t> g_FloatingIconFlashStartMs{0};
static std::atomic<bool> g_PendingSound{false};
static Texture_t* g_FloatIconTexture = nullptr;

// Cached data link pointers (set once in AddonLoad)
static HWND g_GameHWND = nullptr;
static Mumble::Data* g_MumbleLink = nullptr;
static NexusLinkData_t* g_NexusLink = nullptr;
static char g_InputBuf[200] = {};
static bool g_FocusInput = false;
static int g_SendDelay = 50;  // ms between keystrokes
static bool g_RestoreChannel = false;
static std::atomic<bool> g_IsSending{false};
static std::atomic<int> g_LastChanType(ChatMsg_Local);
static std::atomic<int> g_LastChanFlags(0);   // ChatMetadataFlags for last channel
static std::atomic<int> g_LastGuildIndex(0);  // guild index from +0x38, only valid when last chan was Guild
static std::string g_ActiveThemeName = "Nexus";
static std::unordered_map<std::string, std::string> g_Drafts;

// Called from GW2API worker thread when any link resolves.
void TIM_NotifyLinkResolved() { g_LinksDirty = true; }

// Write an AE2 build into Alter Ego's build_library.json.
// Returns error string on failure, empty string on success.

// Request API resolution for all pending links in a conversation.
static void RequestConversationLinks(const TyrianIM::Conversation* convo) {
    if (!convo) return;
    for (size_t mi = 0; mi < convo->messages.size(); mi++) {
        const auto& msg = convo->messages[mi];
        if (!msg.has_links) continue;
        for (size_t si = 0; si < msg.segments.size(); si++) {
            const auto& seg = msg.segments[si];
            if (seg.is_link && seg.link.state == TyrianIM::LinkState::Pending)
                TyrianIM::GW2API::RequestLink(convo->contact, mi, si, seg.link);
        }
    }
}

// --- Bubble layout cache (amortized recomputation) ---
struct SegLayout {
    ImVec2 pos;        // relative to message text origin (bubbleX+padding, textY)
    ImVec2 size;
    bool   is_link;
    int    segment_idx;
};

struct BubbleLayout {
    float bubbleW, bubbleH;
    ImVec2 nameSize, timeSize, msgSize;
    float textWrapWidth;
    std::string senderLabel;
    std::string displayTimestamp;
    bool isSystem;
    // For system messages
    float sysW, sysH;
    // For messages with inline links
    std::vector<SegLayout> seg_layouts; // populated when msg.has_links
};

struct LayoutCache {
    std::string contact;
    float areaWidth = 0;
    float fontScale = 0;
    size_t messageCount = 0;
    std::vector<BubbleLayout> layouts;
};
static LayoutCache g_BubbleCache;
static std::vector<size_t> g_LayoutQueue;  // indices pending recomputation
static const int LAYOUT_BATCH_SIZE = 10;   // max recomputations per frame

static void InvalidateBubbleCache() {
    g_BubbleCache.layouts.clear();
    g_BubbleCache.messageCount = 0;
    g_LayoutQueue.clear();
}

// Forward declaration
static std::string FormatDisplayTime(uint64_t epoch_ms);

// Compute layout for a single message at index i in the conversation
static void ComputeBubbleLayout(const TyrianIM::Conversation* convo, size_t i,
                                 float areaWidth, float fontScale, ImFont* font) {
    if (i >= convo->messages.size() || i >= g_BubbleCache.layouts.size()) return;

    const auto& msg = convo->messages[i];
    auto& layout = g_BubbleCache.layouts[i];
    float fs = font->FontSize * fontScale;
    float maxBubbleWidth = areaWidth * 0.75f;
    float padding = 10.0f;

    if (msg.direction == TyrianIM::MessageDirection::System) {
        layout.isSystem = true;
        float sysFs = font->FontSize * (fontScale * 0.85f);
        layout.msgSize = font->CalcTextSizeA(sysFs, FLT_MAX, areaWidth * 0.8f, msg.text.c_str());
        layout.sysW = layout.msgSize.x + padding * 2;
        layout.sysH = layout.msgSize.y + padding * 2;
        return;
    }

    layout.isSystem = false;
    bool is_self = (msg.direction == TyrianIM::MessageDirection::Outgoing);
    layout.senderLabel = is_self ? "You"
        : (!convo->display_name.empty() ? convo->display_name : msg.sender);

    layout.nameSize = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, layout.senderLabel.c_str());
    layout.displayTimestamp = FormatDisplayTime(msg.epoch_ms);
    if (g_ShowTimestamps) {
        float timeFs = font->FontSize * (fontScale * 0.85f);
        layout.timeSize = font->CalcTextSizeA(timeFs, FLT_MAX, 0.0f, layout.displayTimestamp.c_str());
    } else {
        layout.timeSize = ImVec2(0, 0);
    }
    layout.textWrapWidth = maxBubbleWidth - padding * 2;

    layout.seg_layouts.clear();

    if (!msg.has_links) {
        // Fast path: single text block, unchanged behaviour
        layout.msgSize = font->CalcTextSizeA(fs, FLT_MAX, layout.textWrapWidth, msg.text.c_str());
    } else {
        // Segmented layout: plain text blocks stack to new lines; links flow inline with each other.
        float cursor_x = 0.0f, cursor_y = 0.0f;
        float line_h   = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, "Mg").y;
        float max_x    = 0.0f;

        for (int si = 0; si < (int)msg.segments.size(); si++) {
            const auto& seg = msg.segments[si];
            SegLayout sl{};
            sl.segment_idx = si;

            if (!seg.is_link) {
                // Plain text: always starts at x=0 on a new line if cursor_x > 0
                if (cursor_x > 0.0f) { cursor_y += line_h; cursor_x = 0.0f; }
                ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, layout.textWrapWidth, seg.text.c_str());
                sl.is_link = false;
                sl.pos  = ImVec2(0.0f, cursor_y);
                sl.size = sz;
                cursor_y += sz.y;
                cursor_x  = 0.0f;
                max_x = (std::max)(max_x, sz.x);
            } else {
                // Link: flows inline; wrap if it doesn't fit on the current line
                const std::string& label = seg.link.display;
                ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label.c_str());
                if (cursor_x > 0.0f && cursor_x + sz.x > layout.textWrapWidth) {
                    cursor_y += line_h; cursor_x = 0.0f;
                }
                sl.is_link = true;
                sl.pos  = ImVec2(cursor_x, cursor_y);
                sl.size = ImVec2(sz.x, line_h);
                cursor_x += sz.x + 4.0f; // small gap between consecutive links
                max_x = (std::max)(max_x, sl.pos.x + sl.size.x);
            }
            layout.seg_layouts.push_back(sl);
        }
        // Total message body size
        float total_h = cursor_y + (cursor_x > 0.0f ? line_h : 0.0f);
        layout.msgSize = ImVec2(max_x, total_h);
    }

    layout.bubbleW = (std::max)(layout.nameSize.x + layout.timeSize.x + 20, layout.msgSize.x) + padding * 2;
    if (layout.bubbleW > maxBubbleWidth) layout.bubbleW = maxBubbleWidth;
    layout.bubbleH = layout.nameSize.y + layout.msgSize.y + padding * 2 + 4;
}

// Process a batch of pending layout recomputations per frame
static void TickLayoutQueue(const TyrianIM::Conversation* convo, float areaWidth, float fontScale, ImFont* font) {
    if (g_LayoutQueue.empty()) return;
    int count = (std::min)(LAYOUT_BATCH_SIZE, (int)g_LayoutQueue.size());
    for (int i = 0; i < count; i++) {
        size_t idx = g_LayoutQueue.back();
        g_LayoutQueue.pop_back();
        ComputeBubbleLayout(convo, idx, areaWidth, fontScale, font);
    }
}

// Settings
static bool g_ShowQAIcon = true;
static bool g_SoundEnabled = true;
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
        // Use MCI for mp3 playback — unique alias to avoid conflicts with other addons
        static int s_mciCounter = 0;
        std::string alias = "TIM_Snd_" + std::to_string(++s_mciCounter);
        if (s_mciCounter > 1) {
            std::string prevAlias = "TIM_Snd_" + std::to_string(s_mciCounter - 1);
            mciSendStringA(("close " + prevAlias).c_str(), NULL, 0, NULL);
        }
        std::string cmd = "open \"" + fullPath + "\" type mpegvideo alias " + alias;
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
        cmd = "play " + alias;
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    } else {
        PlaySoundA(fullPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

// Build LPARAM for WM_KEYDOWN/WM_KEYUP messages
static LPARAM GetKeyLParam(uint32_t vk, bool down) {
    int64_t lp = !down ? 1 : 0; // transition state
    lp = lp << 1;
    lp += !down ? 1 : 0; // previous key state
    lp = lp << 1;
    lp += 0; // context code
    lp = lp << 1;
    lp = lp << 4;
    lp = lp << 1;
    lp = lp << 8;
    lp += MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    lp = lp << 16;
    lp += 1;
    return (LPARAM)lp;
}

// Strip GW2 chat link escaping: \[ → [ and \] → ] so pasted codes work in-game
static std::string NormalizeGW2Text(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size() && (s[i+1] == '[' || s[i+1] == ']')) continue;
        out += s[i];
    }
    return out;
}

// Copy wide string to clipboard
static bool CopyToClipboard(HWND hwnd, const std::wstring& text) {
    if (!OpenClipboard(hwnd)) return false;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(WCHAR) * (text.length() + 1));
    if (!hMem) { CloseClipboard(); return false; }
    WCHAR* dst = (WCHAR*)GlobalLock(hMem);
    if (dst) {
        wcscpy(dst, text.c_str());
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
    return true;
}

static std::string GetOwnCharName() {
    if (!g_MumbleLink || !g_MumbleLink->Identity[0]) return {};
    // Identity is GW2 JSON: {"name":"Pie Or Cake","profession":4,...}
    char identity[512] = {};
    WideCharToMultiByte(CP_UTF8, 0, g_MumbleLink->Identity, -1, identity, sizeof(identity), nullptr, nullptr);
    const char* key = "\"name\":\"";
    const char* found = strstr(identity, key);
    if (!found) return {};
    found += strlen(key);
    const char* end = strchr(found, '"');
    if (!end) return {};
    return std::string(found, end);
}

static std::wstring GetChannelCommand(ChatMessageType type, ChatMetadataFlags flags, int guildIndex) {
    switch (type) {
        case ChatMsg_Local:   return L"/s";
        case ChatMsg_Map:     return L"/m";
        case ChatMsg_Party:   return L"/p";
        case ChatMsg_Squad:   return L"/d";
        case ChatMsg_TeamPvP:
        case ChatMsg_TeamWvW: return L"/t";
        case ChatMsg_Guild:
            // guildIndex from +0x38: 0 = /g1, 1 = /g2, ..., 5 = /g6
            return L"/g" + std::to_wstring(guildIndex + 1);
        default: return L"/s";
    }
}

// Send a whisper via the chatbox using clipboard paste
// Runs on a detached thread; uses the Chat Shorts hybrid pattern
static void SendWhisperViaChatbox(const std::string& recipient, const std::string& message) {
    if (!g_MumbleLink) return;

    // Lazily find the game window handle
    if (!g_GameHWND) {
        g_GameHWND = FindWindowA("ArenaNet_Dx_Window_Class", nullptr);
        if (!g_GameHWND) g_GameHWND = FindWindowA("ArenaNet_Gr_Window_Class", nullptr);
        if (!g_GameHWND) return;
    }

    // Convert recipient and message to wide strings separately
    auto toWide = [](const std::string& s) -> std::wstring {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        std::wstring ws(wlen - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], wlen);
        return ws;
    };

    std::wstring wrecip = toWide(recipient);
    std::wstring wmsg = toWide(message);
    HWND hwnd = g_GameHWND;
    int delay = g_SendDelay;

    // Capture the SendToGameOnly function pointer for use on the send thread
    auto sendToGame = APIDefs->WndProc_SendToGameOnly;

    bool doRestore = g_RestoreChannel;
    std::wstring chanCmd = GetChannelCommand(
        (ChatMessageType)g_LastChanType.load(),
        (ChatMetadataFlags)g_LastChanFlags.load(),
        g_LastGuildIndex.load());

    std::thread([wrecip, wmsg, hwnd, delay, sendToGame, doRestore, chanCmd]() {

        auto pasteClipboard = [&](const std::wstring& text) {
            if (!CopyToClipboard(hwnd, text)) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            // Ctrl down (SendInput for modifier)
            INPUT ctrlDown[1] = {};
            ctrlDown[0].type = INPUT_KEYBOARD;
            ctrlDown[0].ki.wVk = VK_CONTROL;
            SendInput(ARRAYSIZE(ctrlDown), ctrlDown, sizeof(INPUT));
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            // V key (bypass ImGui, send directly to game)
            sendToGame(hwnd, WM_KEYDOWN, 'V', GetKeyLParam('V', true));
            sendToGame(hwnd, WM_KEYUP, 'V', GetKeyLParam('V', false));
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            // Ctrl up (SendInput)
            INPUT ctrlUp[1] = {};
            ctrlUp[0].type = INPUT_KEYBOARD;
            ctrlUp[0].ki.wVk = VK_CONTROL;
            ctrlUp[0].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(ARRAYSIZE(ctrlUp), ctrlUp, sizeof(INPUT));
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        };

        // If chatbox is already focused, close it first
        if (g_MumbleLink && g_MumbleLink->Context.IsTextboxFocused) {
            sendToGame(hwnd, WM_KEYDOWN, VK_ESCAPE, GetKeyLParam(VK_ESCAPE, true));
            sendToGame(hwnd, WM_KEYUP, VK_ESCAPE, GetKeyLParam(VK_ESCAPE, false));
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }

        // Step 1: Open chatbox, paste "/w "
        sendToGame(hwnd, WM_KEYDOWN, VK_RETURN, GetKeyLParam(VK_RETURN, true));
        sendToGame(hwnd, WM_KEYUP, VK_RETURN, GetKeyLParam(VK_RETURN, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        pasteClipboard(L"/w ");

        // Step 2: Paste recipient name
        pasteClipboard(wrecip);

        // Step 3: Tab to set whisper target
        sendToGame(hwnd, WM_KEYDOWN, VK_TAB, GetKeyLParam(VK_TAB, true));
        sendToGame(hwnd, WM_KEYUP, VK_TAB, GetKeyLParam(VK_TAB, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));

        // Step 4: Paste message
        pasteClipboard(wmsg);

        // Step 5: Send with Enter, then close chatbox only if it is still open.
        sendToGame(hwnd, WM_KEYDOWN, VK_RETURN, GetKeyLParam(VK_RETURN, true));
        sendToGame(hwnd, WM_KEYUP, VK_RETURN, GetKeyLParam(VK_RETURN, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(delay * 3));
        if (g_MumbleLink && g_MumbleLink->Context.IsTextboxFocused) {
            sendToGame(hwnd, WM_KEYDOWN, VK_ESCAPE, GetKeyLParam(VK_ESCAPE, true));
            sendToGame(hwnd, WM_KEYUP, VK_ESCAPE, GetKeyLParam(VK_ESCAPE, false));
            std::this_thread::sleep_for(std::chrono::milliseconds(delay * 2));
        }

        // Step 6: Restore previous chat channel.
        // '/' opens the chatbox; Enter commits the channel switch (leaves chatbox open);
        // a final conditional Escape closes it.
        if (doRestore && !chanCmd.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay * 3));
            for (wchar_t ch : chanCmd) {
                sendToGame(hwnd, WM_CHAR, (WPARAM)ch, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
            sendToGame(hwnd, WM_KEYDOWN, VK_RETURN, GetKeyLParam(VK_RETURN, true));
            sendToGame(hwnd, WM_KEYUP,   VK_RETURN, GetKeyLParam(VK_RETURN, false));
            std::this_thread::sleep_for(std::chrono::milliseconds(delay * 2));
            if (g_MumbleLink && g_MumbleLink->Context.IsTextboxFocused) {
                sendToGame(hwnd, WM_KEYDOWN, VK_ESCAPE, GetKeyLParam(VK_ESCAPE, true));
                sendToGame(hwnd, WM_KEYUP,   VK_ESCAPE, GetKeyLParam(VK_ESCAPE, false));
            }
        }

        g_IsSending = false;
    }).detach();
}

// GW2 account names match "Name.1234" (dot followed by digits)
static bool IsAccountName(const std::string& name) {
    auto dot = name.rfind('.');
    if (dot == std::string::npos || dot + 1 >= name.size()) return false;
    for (size_t i = dot + 1; i < name.size(); ++i) {
        if (!isdigit((unsigned char)name[i])) return false;
    }
    return true;
}

// Settings persistence helpers
static void SaveSettings();
static void LoadSettings();

struct TyrianTheme {
    bool   inherit_style = false;  // if true, PushGW2Theme skips ImGui style swap
    // ImU32 chat colors ([R,G,B,A] 0-255)
    ImU32  bubble_self  = IM_COL32(30,  60, 100, 200);
    ImU32  bubble_other = IM_COL32(50,  50,  55, 200);
    ImU32  header_bg    = IM_COL32(35,  35,  45, 220);
    ImU32  active_bg    = IM_COL32(50,  80, 120, 180);
    ImU32  input_bg     = IM_COL32(30,  30,  38, 220);
    ImU32  input_border = IM_COL32(0,   0,   0,   0);  // 0 alpha = no border
    ImU32  avatar_bg    = IM_COL32(60, 130, 180, 255);
    ImU32  unread_dot   = IM_COL32(255, 80,  80, 255);
    ImU32  pin_accent   = IM_COL32(180, 150, 60, 255);
    // Bubble gradient (top/bottom; if equal = flat, uses bubble_rounding; if different = gradient, no rounding)
    ImU32 bubble_self_top  = IM_COL32(30,  60, 100, 200);  // defaults match bubble_self
    ImU32 bubble_self_bot  = IM_COL32(30,  60, 100, 200);
    ImU32 bubble_other_top = IM_COL32(50,  50,  55, 200);  // defaults match bubble_other
    ImU32 bubble_other_bot = IM_COL32(50,  50,  55, 200);
    float bubble_rounding  = 10.0f;
    // Background overlay (drawn over chat area, both colors transparent = none)
    ImU32 chat_bg_top = IM_COL32(0, 0, 0, 0);  // top of chat area
    ImU32 chat_bg_bot = IM_COL32(0, 0, 0, 0);  // bottom of chat area
    // Animation parameters
    float bob_amplitude   = 3.0f;    // floating icon vertical bob, pixels
    float bob_period_ms   = 2000.0f; // bob period in milliseconds
    float flash_period_ms = 1000.0f; // flash/pulse period in milliseconds
    float fade_ms         = 150.0f;  // message fade-in duration in milliseconds
    // Background texture (TOML themes: filename relative to themes/ dir)
    std::string    bg_texture_path;
    Texture_t*     bg_texture = nullptr;
    // Per-theme floating icon (TOML themes: filename relative to themes/ dir)
    std::string    icon_texture_path;
    Texture_t*     icon_texture = nullptr;
    // Custom draw hooks for compiled-in themes (nullptr = skip)
    // Called with the panel's draw list and (min, max) screen coords
    using DrawHook = void(*)(ImDrawList*, ImVec2, ImVec2);
    DrawHook draw_chat_bg     = nullptr;  // behind messages
    DrawHook draw_contacts_bg = nullptr;  // behind contact list
    // ImVec4 chat colors ([R,G,B,A] 0-1 floats)
    ImVec4 sender_self  = {0.4f, 0.8f, 1.0f, 1.0f};
    ImVec4 sender_other = {0.9f, 0.7f, 0.3f, 1.0f};
    ImVec4 timestamp    = {0.5f, 0.5f, 0.5f, 1.0f};
    ImVec4 unread_label = {1.0f, 0.4f, 0.4f, 1.0f};
    ImVec4 status_ok    = {0.3f, 0.9f, 0.3f, 1.0f};
    ImVec4 status_warn  = {1.0f, 0.8f, 0.2f, 1.0f};
    ImVec4 status_err   = {1.0f, 0.3f, 0.3f, 1.0f};
    // ImGui style
    ImGuiStyle imgui_style;
    // Metadata
    std::string name, author, description, file_path;
};

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

// Returns "HH:MM" for today's messages, "Weekday, DD Mon YY @ HH:MM" for older ones
static std::string FormatDisplayTime(uint64_t epoch_ms) {
    time_t t = static_cast<time_t>(epoch_ms / 1000);
    struct tm msg_tm;
    localtime_s(&msg_tm, &t);

    time_t now_t = time(nullptr);
    struct tm now_tm;
    localtime_s(&now_tm, &now_t);

    bool sameDay = (msg_tm.tm_year == now_tm.tm_year &&
                    msg_tm.tm_yday == now_tm.tm_yday);
    if (sameDay) {
        char buf[8];
        strftime(buf, sizeof(buf), "%H:%M", &msg_tm);
        return std::string(buf);
    }

    char buf[40];
    strftime(buf, sizeof(buf), "%a, %d %b %y @ %H:%M", &msg_tm);
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
    // Determine the other person's contact key and character name
    std::string account_name = is_incoming ? sender : recipient;
    std::string char_name = is_incoming ? recipient : sender;

    // If we now have an account name that differs from char name, merge any
    // existing char-name-keyed conversation into the account-name-keyed one
    if (!account_name.empty() && !char_name.empty() && account_name != char_name) {
        TyrianIM::ChatManager::MergeConversation(char_name, account_name);
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

    // Set character name as display_name on the conversation (thread-safe)
    if (!char_name.empty()) {
        TyrianIM::ChatManager::SetDisplayName(account_name, char_name);
    }

    if (is_incoming) {
        if (g_SoundEnabled) {
            g_PendingSound = true;  // defer to render thread (Wine threading)
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
    f << "show_timestamps=" << (g_ShowTimestamps ? 1 : 0) << "\n";
    f << "sound_enabled=" << (g_SoundEnabled ? 1 : 0) << "\n";
    f << "selected_sound=" << g_SelectedSound << "\n";
    f << "floating_icon_locked=" << (g_FloatingIconLocked ? 1 : 0) << "\n";
    f << "floating_icon_only_on_unread=" << (g_FloatingIconOnlyOnUnread ? 1 : 0) << "\n";
    f << "font_scale=" << g_FontScale << "\n";
    f << "icon_scale=" << g_FloatingIconScale << "\n";
    f << "show_qa_icon=" << (g_ShowQAIcon ? 1 : 0) << "\n";
    f << "send_delay=" << g_SendDelay << "\n";
    f << "restore_channel=" << (g_RestoreChannel ? 1 : 0) << "\n";
    f << "url_toast_enabled=" << (g_UrlToastEnabled ? 1 : 0) << "\n";
    f << "url_toast_locked=" << (g_UrlToastLocked ? 1 : 0) << "\n";
    f << "url_toast_x=" << g_UrlToastX << "\n";
    f << "url_toast_y=" << g_UrlToastY << "\n";
    f << "active_theme=" << g_ActiveThemeName << "\n";
    auto pinned = TyrianIM::ChatManager::GetPinnedContacts();
    std::sort(pinned.begin(), pinned.end());
    if (!pinned.empty()) {
        std::string joined;
        for (size_t i = 0; i < pinned.size(); i++) {
            if (i > 0) joined += ",";
            joined += pinned[i];
        }
        f << "pinned_contacts=" << joined << "\n";
    }
    for (const auto& [contact, note] : g_ContactNotes) {
        if (note.empty()) continue;
        std::string escaped;
        for (char c : note) {
            if (c == '\r') continue;          // strip carriage returns
            escaped += (c == '\n') ? "\\n" : std::string(1, c);
        }
        f << "note_" << contact << "=" << escaped << "\n";
    }
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
        else if (key == "show_timestamps") g_ShowTimestamps = (val == "1");
        else if (key == "sound_enabled") g_SoundEnabled = (val == "1");
        else if (key == "floating_icon_locked") g_FloatingIconLocked = (val == "1");
        else if (key == "show_qa_icon") g_ShowQAIcon = (val == "1");
        else if (key == "send_delay") try { g_SendDelay = std::stoi(val); if (g_SendDelay < 10) g_SendDelay = 10; if (g_SendDelay > 200) g_SendDelay = 200; } catch (...) {}
        else if (key == "restore_channel") g_RestoreChannel = (val == "1");
        else if (key == "url_toast_enabled") g_UrlToastEnabled = (val == "1");
        else if (key == "url_toast_locked") g_UrlToastLocked = (val == "1");
        else if (key == "url_toast_x") try { g_UrlToastX = std::stof(val); } catch (...) {}
        else if (key == "url_toast_y") try { g_UrlToastY = std::stof(val); } catch (...) {}
        else if (key == "floating_icon_only_on_unread") g_FloatingIconOnlyOnUnread = (val == "1");
        else if (key == "font_scale") try { g_FontScale = std::stof(val); if (g_FontScale < 0.8f) g_FontScale = 0.8f; if (g_FontScale > 2.0f) g_FontScale = 2.0f; } catch (...) {}
        else if (key == "icon_scale") try { g_FloatingIconScale = std::stof(val); if (g_FloatingIconScale < 0.5f) g_FloatingIconScale = 0.5f; if (g_FloatingIconScale > 3.0f) g_FloatingIconScale = 3.0f; } catch (...) {}
        else if (key == "active_theme") g_ActiveThemeName = val;
        else if (key == "selected_sound" || key == "custom_sound_path") {
            g_SelectedSound = val;
        }
        else if (key == "pinned_contacts") {
            g_PinnedContactNames.clear();
            std::istringstream ss(val);
            std::string token;
            while (std::getline(ss, token, ','))
                if (!token.empty()) g_PinnedContactNames.push_back(token);
        }
        else if (key.size() > 5 && key.substr(0, 5) == "note_") {
            std::string contact = key.substr(5);
            std::string note;
            for (size_t ni = 0; ni < val.size(); ni++) {
                if (val[ni] == '\\' && ni + 1 < val.size() && val[ni+1] == 'n') {
                    note += '\n'; ni++;
                } else {
                    note += val[ni];
                }
            }
            g_ContactNotes[contact] = note;
        }
    }
}

// --- GW2 theme (mirrors Alter Ego) ---
static std::vector<ImGuiStyle> g_StyleStack;
static TyrianTheme              g_ActiveTheme;
static std::vector<TyrianTheme> g_LoadedThemes;
static int                      g_ActiveThemeIndex = 0;

static void PushGW2Theme() {
    g_StyleStack.push_back(ImGui::GetStyle());
    if (!g_ActiveTheme.inherit_style)
        ImGui::GetStyle() = g_ActiveTheme.imgui_style;
}
static void PopGW2Theme() {
    if (!g_StyleStack.empty()) {
        ImGui::GetStyle() = g_StyleStack.back();
        g_StyleStack.pop_back();
    }
}
struct ThemeGuard {
    ThemeGuard()  { PushGW2Theme(); }
    ~ThemeGuard() { PopGW2Theme(); }
};
static ImGuiStyle BuildGW2Theme() {
    ImGuiStyle style = ImGui::GetStyle();
    ImGuiStyle& s = style;

    // Rounding
    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 4.0f;

    // Spacing & padding
    s.WindowPadding     = ImVec2(10, 10);
    s.FramePadding      = ImVec2(6, 4);
    s.ItemSpacing       = ImVec2(8, 5);
    s.ItemInnerSpacing  = ImVec2(6, 4);
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 8.0f;
    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;

    // Colors — dark slate base with warm gold accents
    ImVec4* c = s.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]             = ImVec4(0.08f, 0.08f, 0.10f, 0.96f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.07f, 0.07f, 0.09f, 0.80f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.10f, 0.12f, 0.96f);

    // Borders
    c[ImGuiCol_Border]               = ImVec4(0.28f, 0.25f, 0.18f, 0.50f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frames (input boxes, combos)
    c[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.13f, 0.11f, 0.80f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.20f, 0.14f, 0.80f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.25f, 0.16f, 0.90f);

    // Title bar
    c[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.09f, 0.07f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.16f, 0.14f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.08f, 0.07f, 0.05f, 0.75f);

    // Menu bar
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.12f, 0.11f, 0.09f, 1.00f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.06f, 0.07f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.27f, 0.18f, 0.80f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.36f, 0.22f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.44f, 0.26f, 1.00f);

    // Checkmark, slider
    c[ImGuiCol_CheckMark]            = ImVec4(0.90f, 0.75f, 0.25f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.70f, 0.58f, 0.20f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.85f, 0.70f, 0.25f, 1.00f);

    // Buttons — warm gold
    c[ImGuiCol_Button]               = ImVec4(0.22f, 0.20f, 0.12f, 0.80f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.45f, 0.38f, 0.16f, 1.00f);

    // Headers (selectables, collapsing headers)
    c[ImGuiCol_Header]               = ImVec4(0.18f, 0.16f, 0.10f, 0.70f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.24f, 0.12f, 0.80f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);

    // Separator
    c[ImGuiCol_Separator]            = ImVec4(0.28f, 0.25f, 0.18f, 0.40f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.50f, 0.42f, 0.20f, 0.70f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.65f, 0.55f, 0.25f, 1.00f);

    // Resize grip
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.27f, 0.18f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.50f, 0.44f, 0.26f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.65f, 0.55f, 0.25f, 0.90f);

    // Tabs — gold accent for active
    c[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.13f, 0.10f, 0.86f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);
    c[ImGuiCol_TabActive]            = ImVec4(0.28f, 0.24f, 0.10f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.10f, 0.09f, 0.07f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.18f, 0.16f, 0.10f, 1.00f);

    // Text
    c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.87f, 0.78f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.47f, 0.40f, 1.00f);

    // Modal dim background
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

    // Nav highlight
    c[ImGuiCol_NavHighlight]         = ImVec4(0.70f, 0.58f, 0.20f, 1.00f);

    // Table
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.13f, 0.10f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.28f, 0.25f, 0.18f, 0.60f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.22f, 0.20f, 0.15f, 0.40f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.10f, 0.10f, 0.08f, 0.30f);

    // Plot (progress bars)
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.65f, 0.55f, 0.15f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.80f, 0.68f, 0.20f, 1.00f);
    return style;
}

// ── GW2 Dark draw hook ────────────────────────────────────────────────────────

static void GW2DarkDrawChatBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    // Subtle diagonal crosshatch — textures the solid background without animation
    const ImU32 col    = IM_COL32(180, 140, 50, 18);  // very faint gold
    const float step   = 22.0f;                        // spacing between lines
    const float w      = mx.x - mn.x;
    const float h      = mx.y - mn.y;
    const float extent = w + h;                        // far enough to cross the full panel

    dl->PushClipRect(mn, mx, true);

    // SW→NE diagonals
    for (float off = -h; off < w; off += step) {
        dl->AddLine({mn.x + off,        mx.y},
                    {mn.x + off + extent, mn.y}, col, 1.0f);
    }
    // NW→SE diagonals
    for (float off = -h; off < w; off += step) {
        dl->AddLine({mn.x + off,         mn.y},
                    {mn.x + off + extent, mx.y}, col, 1.0f);
    }

    dl->PopClipRect();
}

static TyrianTheme BuildGW2DarkTheme() {
    TyrianTheme theme;
    theme.name        = "GW2 Dark";
    theme.author      = "TyrianIM";
    theme.description = "Default Guild Wars 2 inspired dark theme";
    // file_path left empty — compiled-in, no file needed

    // Build and copy the GW2 style into the theme
    theme.imgui_style = BuildGW2Theme();

    // Chat colors — warm gold palette to match the ImGui gold accents
    theme.bubble_self   = IM_COL32( 75,  55,  15, 210);  // dark amber (was blue)
    theme.active_bg     = IM_COL32( 80,  60,  15, 185);  // warm gold highlight (was blue)
    theme.avatar_bg     = IM_COL32(155, 115,  35, 255);  // warm gold avatar (was blue)
    theme.sender_self   = ImVec4(0.92f, 0.76f, 0.28f, 1.0f);  // gold name (was cyan)
    theme.bubble_self_top = theme.bubble_self_bot = theme.bubble_self;
    theme.bubble_other_top = theme.bubble_other_bot = theme.bubble_other;
    theme.bubble_rounding = 10.0f;
    theme.input_bg      = IM_COL32( 38,  30,  12, 230);
    theme.input_border  = IM_COL32(180, 140,  50, 130);
    theme.draw_chat_bg  = GW2DarkDrawChatBg;
    return theme;
}

// ── Charr Steel draw hooks ────────────────────────────────────────────────────

static void CharrSteelEmbers(ImDrawList* dl, ImVec2 mn, ImVec2 mx, int count) {
    float w = mx.x - mn.x, h = mx.y - mn.y;
    float t = (float)ImGui::GetTime();
    for (int i = 0; i < count; i++) {
        float phi   = i * 0.6180339f;
        float speed = 20.0f + phi * 38.0f;
        float drift = sinf(t * 0.6f + phi * 8.0f) * 10.0f;
        float px    = mn.x + fmodf(phi * w * 1.7f + drift, w);
        // Rise from bottom, wrap back to bottom
        float py    = mx.y - fmodf(t * speed + phi * h, h);
        float pulse = 0.5f + 0.5f * sinf(t * 2.4f + phi * 6.28f);
        float r     = 0.5f + pulse * 1.2f;
        int   a     = (int)(20 + pulse * 55);
        // Mix of orange, red, and bright yellow-white hot embers
        ImU32 col = (i % 3 == 0)
            ? IM_COL32(255, 120,  20, a)
            : (i % 3 == 1) ? IM_COL32(220,  50,  10, a)
                           : IM_COL32(255, 200,  60, a);
        dl->AddCircleFilled(ImVec2(px, py), r, col);
        // Occasional glow bloom on the brightest embers
        if (i % 4 == 0 && pulse > 0.80f)
            dl->AddCircleFilled(ImVec2(px, py), r * 3.0f,
                IM_COL32(255, 100, 20, (int)(a * 0.20f)));
    }
}

static void CharrSteelClawMarks(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float alpha) {
    float w = mx.x - mn.x, h = mx.y - mn.y;
    float t = (float)ImGui::GetTime();
    // Slow pulse — like the marks are still glowing with heat
    float heat = 0.40f + 0.60f * sinf(t * 0.18f);
    int   a    = (int)(12 * alpha * heat);
    ImU32 col  = IM_COL32(180, 30, 10, a);

    // Three parallel diagonal slashes — upper-left cluster
    struct Slash { float x0, y0, x1, y1; };
    const Slash slashes[] = {
        { 0.05f, 0.08f,  0.22f, 0.62f },
        { 0.09f, 0.06f,  0.26f, 0.60f },
        { 0.13f, 0.04f,  0.30f, 0.58f },
        // A second set — lower-right, going the other direction
        { 0.72f, 0.38f,  0.90f, 0.85f },
        { 0.76f, 0.36f,  0.94f, 0.82f },
        { 0.80f, 0.34f,  0.98f, 0.80f },
    };
    for (auto& s : slashes) {
        dl->AddLine(
            ImVec2(mn.x + w * s.x0, mn.y + h * s.y0),
            ImVec2(mn.x + w * s.x1, mn.y + h * s.y1),
            col, 1.0f);
    }
}

static void CharrSteelDrawChatBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    float h = mx.y - mn.y;
    // Forge heat rising from the floor
    dl->AddRectFilledMultiColor(
        ImVec2(mn.x, mx.y - h * 0.30f), mx,
        IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
        IM_COL32(70, 8, 2, 35), IM_COL32(70, 8, 2, 35));

    CharrSteelClawMarks(dl, mn, mx, 1.0f);
    CharrSteelEmbers(dl, mn, mx, 20);

    // Dark top vignette — oppressive ceiling
    dl->AddRectFilledMultiColor(
        mn, ImVec2(mx.x, mn.y + h * 0.07f),
        IM_COL32(8, 2, 2, 70), IM_COL32(8, 2, 2, 70),
        IM_COL32(0,0,0,0),     IM_COL32(0,0,0,0));
}

static void CharrSteelDrawContactsBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    CharrSteelClawMarks(dl, mn, mx, 0.7f);
    CharrSteelEmbers(dl, mn, mx, 9);
}

static TyrianTheme BuildCharrSteelTheme() {
    TyrianTheme t;
    t.name        = "Charr Steel";
    t.description = "Dark charcoal with blood-red accents";

    ImGuiStyle& s = t.imgui_style;
    // Copy base metrics from GW2 theme (rounding, padding etc)
    s = BuildGW2Theme();

    // Override colors
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.07f, 0.06f, 0.06f, 0.97f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.06f, 0.05f, 0.05f, 0.85f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.08f, 0.08f, 0.96f);
    c[ImGuiCol_Border]               = ImVec4(0.30f, 0.15f, 0.12f, 0.50f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.11f, 0.10f, 0.80f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.26f, 0.14f, 0.12f, 0.85f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.38f, 0.16f, 0.13f, 0.90f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.07f, 0.06f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.24f, 0.10f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.07f, 0.05f, 0.04f, 0.75f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.12f, 0.08f, 0.07f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.05f, 0.04f, 0.04f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.32f, 0.14f, 0.11f, 0.80f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.44f, 0.18f, 0.14f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.58f, 0.22f, 0.16f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.88f, 0.18f, 0.12f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.70f, 0.14f, 0.10f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.95f, 0.22f, 0.15f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.24f, 0.10f, 0.08f, 0.80f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.40f, 0.14f, 0.10f, 0.90f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.55f, 0.18f, 0.12f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.22f, 0.09f, 0.07f, 0.70f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.36f, 0.13f, 0.10f, 0.80f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.50f, 0.17f, 0.12f, 0.90f);
    c[ImGuiCol_Separator]            = ImVec4(0.28f, 0.12f, 0.09f, 0.40f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.44f, 0.18f, 0.13f, 0.70f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.60f, 0.22f, 0.16f, 1.00f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.12f, 0.09f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.48f, 0.18f, 0.13f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.65f, 0.22f, 0.16f, 0.90f);
    c[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.08f, 0.07f, 0.86f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.36f, 0.14f, 0.10f, 0.90f);
    c[ImGuiCol_TabActive]            = ImVec4(0.28f, 0.11f, 0.08f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.08f, 0.05f, 0.04f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.18f, 0.08f, 0.06f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.86f, 0.84f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.48f, 0.40f, 0.38f, 1.00f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.80f, 0.20f, 0.14f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.75f, 0.18f, 0.12f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.90f, 0.24f, 0.16f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.08f, 0.07f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.28f, 0.12f, 0.09f, 0.60f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.20f, 0.10f, 0.08f, 0.40f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

    // Chat-specific colors — gradients
    t.bubble_self   = IM_COL32( 85,  20,  14, 225);
    t.bubble_other  = IM_COL32( 46,  40,  40, 225);
    t.bubble_self_top  = IM_COL32(110,  28,  18, 230);  // bright dark-red top
    t.bubble_self_bot  = IM_COL32( 45,   8,   5, 240);  // near-black bottom
    t.bubble_other_top = IM_COL32( 62,  55,  54, 230);  // steel grey top
    t.bubble_other_bot = IM_COL32( 28,  24,  24, 240);  // deep charcoal bottom

    t.header_bg     = IM_COL32( 22,  16,  15, 245);
    t.active_bg     = IM_COL32( 72,  16,  12, 185);
    t.input_bg      = IM_COL32( 28,  18,  16, 230);
    t.input_border  = IM_COL32(160,  28,  18, 160);
    t.avatar_bg     = IM_COL32(165,  28,  20, 255);
    t.unread_dot    = IM_COL32(220,  40,  28, 255);
    t.pin_accent    = IM_COL32(195,  32,  22, 255);

    // Chat area background — subtle dark red vignette at bottom
    t.chat_bg_top = IM_COL32(  0,  0,  0,   0);   // transparent at top
    t.chat_bg_bot = IM_COL32( 60,  5,  3,  40);   // faint blood-red glow at bottom

    // Sharp industrial corners
    t.bubble_rounding = 2.0f;

    // Animation — slightly heavier bob, faster aggressive flash
    t.bob_amplitude   = 4.0f;
    t.bob_period_ms   = 1600.0f;  // slightly faster bob
    t.flash_period_ms = 700.0f;   // faster, more urgent flash
    t.fade_ms         = 200.0f;   // slightly slower fade-in for drama

    t.sender_self   = ImVec4(0.95f, 0.30f, 0.20f, 1.00f);
    t.sender_other  = ImVec4(0.86f, 0.80f, 0.78f, 1.00f);
    t.timestamp     = ImVec4(0.50f, 0.40f, 0.38f, 1.00f);
    t.unread_label  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    t.status_ok     = ImVec4(0.40f, 0.85f, 0.30f, 1.00f);
    t.status_warn   = ImVec4(1.00f, 0.72f, 0.08f, 1.00f);
    t.status_err    = ImVec4(1.00f, 0.30f, 0.25f, 1.00f);

    t.draw_chat_bg     = CharrSteelDrawChatBg;
    t.draw_contacts_bg = CharrSteelDrawContactsBg;

    return t;
}

// --- Theme management ---

static std::string ThemesDir() {
    return g_DataDir + "/themes";
}

static void ApplyTheme(const TyrianTheme& theme) {
    g_ActiveTheme = theme;
    // Reset lazily-resolved texture handles (will re-resolve on next render)
    g_ActiveTheme.bg_texture   = nullptr;
    g_ActiveTheme.icon_texture = nullptr;
    // No bubble cache invalidation — theme changes are purely visual (colours,
    // rounding, draw hooks) and don't affect bubble sizes or timestamp strings.
}

static void ApplyNamedTheme(const std::string& name) {
    for (int i = 0; i < (int)g_LoadedThemes.size(); ++i) {
        if (g_LoadedThemes[i].name == name) {
            g_ActiveThemeIndex = i;
            ApplyTheme(g_LoadedThemes[i]);
            return;
        }
    }
    // Not found — fall back to built-in (index 0)
    if (!g_LoadedThemes.empty()) {
        g_ActiveThemeIndex = 0;
        ApplyTheme(g_LoadedThemes[0]);
    }
}

// Derive TyrianIM-specific chat colors from the loaded ImGui style.
// Called when a TOML file has no [chat] section (e.g. raw ImThemes themes).
static void DeriveChatColorsFromStyle(TyrianTheme& t) {
    const ImVec4* c = t.imgui_style.Colors;

    auto toU32 = [](ImVec4 v) -> ImU32 {
        return IM_COL32((int)(v.x * 255.f), (int)(v.y * 255.f),
                        (int)(v.z * 255.f), (int)(v.w * 255.f));
    };
    auto clampW = [](ImVec4 v, float w) { v.w = w; return v; };

    // Bubbles — self uses the header accent, other uses the frame background
    t.bubble_self  = toU32(clampW(c[ImGuiCol_HeaderActive], 0.70f));
    ImVec4 otherBg = c[ImGuiCol_FrameBg];
    otherBg.w      = std::min(1.0f, otherBg.w + 0.25f);
    t.bubble_other = toU32(otherBg);
    t.bubble_self_top = t.bubble_self_bot = t.bubble_self;
    t.bubble_other_top = t.bubble_other_bot = t.bubble_other;

    // Chrome — map directly to ImGui equivalents
    t.header_bg = toU32(c[ImGuiCol_TitleBg]);
    t.active_bg = toU32(c[ImGuiCol_Header]);
    t.input_bg  = toU32(c[ImGuiCol_FrameBg]);

    // Accent color (CheckMark is the canonical accent in most themes)
    ImVec4 accent = clampW(c[ImGuiCol_CheckMark], 1.0f);
    t.avatar_bg  = toU32(accent);
    t.pin_accent = toU32(accent);

    // Text colors
    t.sender_self  = clampW(c[ImGuiCol_SliderGrabActive], 1.0f);
    t.sender_other = c[ImGuiCol_Text];
    t.timestamp    = c[ImGuiCol_TextDisabled];

    // Semantic colors (unread_dot, unread_label, status_*) keep their defaults
}

static std::optional<TyrianTheme> LoadThemeFromTOML(const std::string& path) {
    try {
        auto doc = toml::parse_file(path);

        // Support both native format (flat) and ImThemes format ([[themes]] array)
        // themeTable points to whichever toml::table we'll read from
        toml::table* themeTable = doc.as_table();
        toml::table  imThemesEntry; // storage for ImThemes first-entry copy
        if (doc.contains("themes") && doc["themes"].is_array()) {
            auto& arr = *doc["themes"].as_array();
            if (arr.empty()) return std::nullopt;
            if (auto* tbl = arr[0].as_table()) {
                imThemesEntry = *tbl;
                themeTable = &imThemesEntry;
            }
        }
        if (!themeTable) return std::nullopt;

        auto nodeView = [&](const char* k) { return (*themeTable)[k]; };

        TyrianTheme t = BuildGW2DarkTheme();
        t.file_path   = path;

        // Metadata
        if (auto v = nodeView("name").value<std::string>())         t.name        = *v;
        else t.name = std::filesystem::path(path).stem().string();
        if (auto v = nodeView("author").value<std::string>())       t.author      = *v;
        if (auto v = nodeView("description").value<std::string>())  t.description = *v;

        // Style metrics — check [style] sub-table first, then fall back to root level
        // (ImThemes files put these at root; our native format uses [style])
        {
            toml::table* styleTable = nodeView("style").as_table();
            if (!styleTable) styleTable = themeTable;  // root-level fallback (ImThemes format)

            auto readF = [&](const char* k, float& v) {
                if (auto val = (*styleTable)[k].value<double>()) v = (float)*val;
            };
            auto readV2 = [&](const char* k, ImVec2& v) {
                if (auto* arr = (*styleTable)[k].as_array(); arr && arr->size() == 2) {
                    if (auto x = (*arr)[0].value<double>(), y = (*arr)[1].value<double>(); x && y)
                        v = ImVec2((float)*x, (float)*y);
                }
            };
            readF("windowRounding",    t.imgui_style.WindowRounding);
            readF("childRounding",     t.imgui_style.ChildRounding);
            readF("frameRounding",     t.imgui_style.FrameRounding);
            readF("popupRounding",     t.imgui_style.PopupRounding);
            readF("scrollbarRounding", t.imgui_style.ScrollbarRounding);
            readF("grabRounding",      t.imgui_style.GrabRounding);
            readF("tabRounding",       t.imgui_style.TabRounding);
            readF("scrollbarSize",     t.imgui_style.ScrollbarSize);
            readF("grabMinSize",       t.imgui_style.GrabMinSize);
            readF("windowBorderSize",  t.imgui_style.WindowBorderSize);
            readF("frameBorderSize",   t.imgui_style.FrameBorderSize);
            readV2("windowPadding",    t.imgui_style.WindowPadding);
            readV2("framePadding",     t.imgui_style.FramePadding);
            readV2("itemSpacing",      t.imgui_style.ItemSpacing);
            readV2("itemInnerSpacing", t.imgui_style.ItemInnerSpacing);
        }

        // [colors] — ImGui style colors (ImThemes-compatible token names)
        if (auto* colors = nodeView("colors").as_table()) {
            static const std::unordered_map<std::string, int> kColorMap = {
                {"Text",                   ImGuiCol_Text},
                {"TextDisabled",           ImGuiCol_TextDisabled},
                {"WindowBg",               ImGuiCol_WindowBg},
                {"ChildBg",                ImGuiCol_ChildBg},
                {"PopupBg",                ImGuiCol_PopupBg},
                {"Border",                 ImGuiCol_Border},
                {"BorderShadow",           ImGuiCol_BorderShadow},
                {"FrameBg",                ImGuiCol_FrameBg},
                {"FrameBgHovered",         ImGuiCol_FrameBgHovered},
                {"FrameBgActive",          ImGuiCol_FrameBgActive},
                {"TitleBg",                ImGuiCol_TitleBg},
                {"TitleBgActive",          ImGuiCol_TitleBgActive},
                {"TitleBgCollapsed",       ImGuiCol_TitleBgCollapsed},
                {"MenuBarBg",              ImGuiCol_MenuBarBg},
                {"ScrollbarBg",            ImGuiCol_ScrollbarBg},
                {"ScrollbarGrab",          ImGuiCol_ScrollbarGrab},
                {"ScrollbarGrabHovered",   ImGuiCol_ScrollbarGrabHovered},
                {"ScrollbarGrabActive",    ImGuiCol_ScrollbarGrabActive},
                {"CheckMark",              ImGuiCol_CheckMark},
                {"SliderGrab",             ImGuiCol_SliderGrab},
                {"SliderGrabActive",       ImGuiCol_SliderGrabActive},
                {"Button",                 ImGuiCol_Button},
                {"ButtonHovered",          ImGuiCol_ButtonHovered},
                {"ButtonActive",           ImGuiCol_ButtonActive},
                {"Header",                 ImGuiCol_Header},
                {"HeaderHovered",          ImGuiCol_HeaderHovered},
                {"HeaderActive",           ImGuiCol_HeaderActive},
                {"Separator",              ImGuiCol_Separator},
                {"SeparatorHovered",       ImGuiCol_SeparatorHovered},
                {"SeparatorActive",        ImGuiCol_SeparatorActive},
                {"ResizeGrip",             ImGuiCol_ResizeGrip},
                {"ResizeGripHovered",      ImGuiCol_ResizeGripHovered},
                {"ResizeGripActive",       ImGuiCol_ResizeGripActive},
                {"Tab",                    ImGuiCol_Tab},
                {"TabHovered",             ImGuiCol_TabHovered},
                {"TabActive",              ImGuiCol_TabActive},
                {"TabUnfocused",           ImGuiCol_TabUnfocused},
                {"TabUnfocusedActive",     ImGuiCol_TabUnfocusedActive},
                {"PlotHistogram",          ImGuiCol_PlotHistogram},
                {"PlotHistogramHovered",   ImGuiCol_PlotHistogramHovered},
                {"TableHeaderBg",          ImGuiCol_TableHeaderBg},
                {"TableBorderStrong",      ImGuiCol_TableBorderStrong},
                {"TableBorderLight",       ImGuiCol_TableBorderLight},
                {"TableRowBg",             ImGuiCol_TableRowBg},
                {"TableRowBgAlt",          ImGuiCol_TableRowBgAlt},
                {"NavHighlight",           ImGuiCol_NavHighlight},
                {"ModalWindowDimBg",       ImGuiCol_ModalWindowDimBg},
            };
            for (auto& [name, val] : *colors) {
                auto it = kColorMap.find(std::string(name.str()));
                if (it == kColorMap.end()) continue;

                ImVec4 col;
                bool parsed = false;

                if (auto* arr = val.as_array(); arr && arr->size() == 4) {
                    // Native format: [r, g, b, a] floats 0-1
                    auto r = (*arr)[0].value<double>(), g = (*arr)[1].value<double>(),
                         b = (*arr)[2].value<double>(), a = (*arr)[3].value<double>();
                    if (r && g && b && a) {
                        col = ImVec4((float)*r,(float)*g,(float)*b,(float)*a);
                        parsed = true;
                    }
                } else if (auto s = val.value<std::string>()) {
                    // ImThemes format: "rgba(R, G, B, A)" where RGB are 0-255, A is 0-1
                    float r, g, b, a;
                    if (sscanf(s->c_str(), "rgba(%f, %f, %f, %f)", &r, &g, &b, &a) == 4) {
                        col = ImVec4(r / 255.f, g / 255.f, b / 255.f, a);
                        parsed = true;
                    }
                }

                if (parsed)
                    t.imgui_style.Colors[it->second] = col;
            }
        }

        // [chat] — TyrianIM-specific colors
        // If no [chat] section, derive sensible colors from the loaded ImGui style
        if (!nodeView("chat").as_table()) DeriveChatColorsFromStyle(t);
        if (auto* chat = nodeView("chat").as_table()) {
            auto readU32 = [&](const char* k, ImU32& v) {
                auto* arr = (*chat)[k].as_array();
                if (!arr || arr->size() != 4) return;
                auto r = (*arr)[0].value<int64_t>(), g = (*arr)[1].value<int64_t>(),
                     b = (*arr)[2].value<int64_t>(), a = (*arr)[3].value<int64_t>();
                if (r && g && b && a)
                    v = IM_COL32((int)*r,(int)*g,(int)*b,(int)*a);
            };
            auto readV4 = [&](const char* k, ImVec4& v) {
                auto* arr = (*chat)[k].as_array();
                if (!arr || arr->size() != 4) return;
                auto r = (*arr)[0].value<double>(), g = (*arr)[1].value<double>(),
                     b = (*arr)[2].value<double>(), a = (*arr)[3].value<double>();
                if (r && g && b && a)
                    v = ImVec4((float)*r,(float)*g,(float)*b,(float)*a);
            };
            readU32("bubble_self",  t.bubble_self);
            readU32("bubble_other", t.bubble_other);
            readU32("header_bg",    t.header_bg);
            readU32("active_bg",    t.active_bg);
            readU32("input_bg",     t.input_bg);
            readU32("input_border", t.input_border);
            readU32("avatar_bg",    t.avatar_bg);
            readU32("unread_dot",   t.unread_dot);
            readU32("pin_accent",   t.pin_accent);
            readV4("sender_self",   t.sender_self);
            readV4("sender_other",  t.sender_other);
            readV4("timestamp",     t.timestamp);
            readV4("unread_label",  t.unread_label);
            readV4("status_ok",     t.status_ok);
            readV4("status_warn",   t.status_warn);
            readV4("status_err",    t.status_err);

            // Gradient bubble colors
            readU32("bubble_self_top",   t.bubble_self_top);
            readU32("bubble_self_bot",   t.bubble_self_bot);
            readU32("bubble_other_top",  t.bubble_other_top);
            readU32("bubble_other_bot",  t.bubble_other_bot);

            // Chat background overlay
            readU32("chat_bg_top", t.chat_bg_top);
            readU32("chat_bg_bot", t.chat_bg_bot);

            // Shape and animation
            auto readF = [&](const char* k, float& v) {
                if (auto val = (*chat)[k].value<double>()) v = (float)*val;
            };
            readF("bubble_rounding",  t.bubble_rounding);
            readF("bob_amplitude",    t.bob_amplitude);
            readF("bob_period_ms",    t.bob_period_ms);
            readF("flash_period_ms",  t.flash_period_ms);
            readF("fade_ms",          t.fade_ms);

            // Texture paths
            if (auto v = (*chat)["bg_texture"].value<std::string>())   t.bg_texture_path   = *v;
            if (auto v = (*chat)["icon_texture"].value<std::string>())  t.icon_texture_path = *v;
        }

        return t;
    } catch (...) {
        return std::nullopt;
    }
}

// ── Asuran Lab draw hooks ─────────────────────────────────────────────────────

static void AsuranLabDrawGrid(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float gridSize,
                               ImU32 colFaint, ImU32 colBright) {
    // Faint grid lines aligned to grid coordinates (stable as window scrolls)
    for (float x = mn.x - fmodf(mn.x, gridSize); x < mx.x; x += gridSize)
        dl->AddLine(ImVec2(x, mn.y), ImVec2(x, mx.y), colFaint, 0.5f);
    for (float y = mn.y - fmodf(mn.y, gridSize); y < mx.y; y += gridSize)
        dl->AddLine(ImVec2(mn.x, y), ImVec2(mx.x, y), colFaint, 0.5f);

    // Every 4th line slightly brighter
    float bigGrid = gridSize * 4.0f;
    for (float x = mn.x - fmodf(mn.x, bigGrid); x < mx.x; x += bigGrid)
        dl->AddLine(ImVec2(x, mn.y), ImVec2(x, mx.y), colBright, 0.5f);
    for (float y = mn.y - fmodf(mn.y, bigGrid); y < mx.y; y += bigGrid)
        dl->AddLine(ImVec2(mn.x, y), ImVec2(mx.x, y), colBright, 0.5f);
}

static void AsuranLabChatBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    float w = mx.x - mn.x, h = mx.y - mn.y;

    // Grid
    AsuranLabDrawGrid(dl, mn, mx, 24.0f,
        IM_COL32(0, 170, 230, 14), IM_COL32(0, 200, 255, 28));

    // Animated horizontal scan line descending continuously
    float scanT = (float)fmod(ImGui::GetTime() * 0.25, 1.0);
    float scanY = mn.y + scanT * h;
    ImU32 scanA = IM_COL32(0, 220, 255, 45);
    ImU32 scanFade = IM_COL32(0, 220, 255, 0);
    dl->AddRectFilledMultiColor(
        ImVec2(mn.x, scanY - 18.0f), ImVec2(mx.x, scanY),
        scanFade, scanFade, scanA, scanA);
    dl->AddLine(ImVec2(mn.x, scanY), ImVec2(mx.x, scanY), IM_COL32(0, 230, 255, 70), 1.0f);

    // Secondary faster pulse line
    float pulseT = (float)fmod(ImGui::GetTime() * 0.7 + 0.4, 1.0);
    float pulseY = mn.y + pulseT * h;
    dl->AddLine(ImVec2(mn.x, pulseY), ImVec2(mx.x, pulseY), IM_COL32(180, 80, 255, 22), 0.5f);

    // Corner circuit traces — top-left
    float cr = 18.0f;
    ImU32 trace = IM_COL32(0, 200, 255, 55);
    dl->AddLine(ImVec2(mn.x + cr, mn.y + 6),  ImVec2(mn.x + cr + 40, mn.y + 6),  trace, 1.0f);
    dl->AddLine(ImVec2(mn.x + cr + 40, mn.y + 6), ImVec2(mn.x + cr + 40, mn.y + 18), trace, 1.0f);
    dl->AddCircleFilled(ImVec2(mn.x + cr + 40, mn.y + 18), 2.5f, IM_COL32(0, 220, 255, 120));

    // Bottom-right corner trace
    dl->AddLine(ImVec2(mx.x - cr, mx.y - 6),  ImVec2(mx.x - cr - 40, mx.y - 6),  trace, 1.0f);
    dl->AddLine(ImVec2(mx.x - cr - 40, mx.y - 6), ImVec2(mx.x - cr - 40, mx.y - 18), trace, 1.0f);
    dl->AddCircleFilled(ImVec2(mx.x - cr - 40, mx.y - 18), 2.5f, IM_COL32(0, 220, 255, 120));

    // Subtle vignette (darker edges)
    float vw = w * 0.12f;
    dl->AddRectFilledMultiColor(mn, ImVec2(mn.x + vw, mx.y),
        IM_COL32(0,0,0,55), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), IM_COL32(0,0,0,55));
    dl->AddRectFilledMultiColor(ImVec2(mx.x - vw, mn.y), mx,
        IM_COL32(0,0,0,0), IM_COL32(0,0,0,55), IM_COL32(0,0,0,55), IM_COL32(0,0,0,0));
}

static void AsuranLabContactsBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    // Sparser grid for the narrower contacts panel
    AsuranLabDrawGrid(dl, mn, mx, 24.0f,
        IM_COL32(0, 140, 200, 10), IM_COL32(0, 170, 230, 20));

    // Single slow scan line
    float t = (float)fmod(ImGui::GetTime() * 0.18, 1.0);
    float y = mn.y + t * (mx.y - mn.y);
    dl->AddLine(ImVec2(mn.x, y), ImVec2(mx.x, y), IM_COL32(0, 200, 255, 30), 0.8f);
}

static TyrianTheme BuildAsuranLabTheme() {
    TyrianTheme t;
    t.name        = "Asuran Lab";
    t.description = "High-tech holographic display with animated grid and circuit traces";

    ImGuiStyle& s = t.imgui_style;
    s = BuildGW2Theme();  // start from GW2 metrics (rounding, padding)

    // Override all colors to electric cyan/purple on near-black
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.03f, 0.03f, 0.06f, 0.97f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.03f, 0.03f, 0.06f, 0.80f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.05f, 0.05f, 0.10f, 0.96f);
    c[ImGuiCol_Border]               = ImVec4(0.00f, 0.65f, 0.90f, 0.35f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.00f, 0.18f, 0.28f, 0.60f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.00f, 0.28f, 0.42f, 0.75f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.00f, 0.40f, 0.58f, 0.85f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.02f, 0.04f, 0.09f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.00f, 0.22f, 0.36f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.02f, 0.04f, 0.08f, 0.75f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.02f, 0.05f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.02f, 0.03f, 0.06f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.00f, 0.50f, 0.72f, 0.70f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 0.65f, 0.88f, 0.85f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.00f, 0.80f, 1.00f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.00f, 0.72f, 0.90f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.00f, 0.28f, 0.42f, 0.75f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.00f, 0.48f, 0.68f, 0.85f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.00f, 0.65f, 0.88f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.00f, 0.35f, 0.55f, 0.60f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.00f, 0.50f, 0.72f, 0.75f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.00f, 0.65f, 0.88f, 0.85f);
    c[ImGuiCol_Separator]            = ImVec4(0.00f, 0.50f, 0.72f, 0.35f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.00f, 0.70f, 0.92f, 0.60f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.00f, 0.55f, 0.78f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.00f, 0.72f, 0.92f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.00f, 0.90f, 1.00f, 0.90f);
    c[ImGuiCol_Tab]                  = ImVec4(0.02f, 0.10f, 0.18f, 0.86f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.00f, 0.48f, 0.68f, 0.90f);
    c[ImGuiCol_TabActive]            = ImVec4(0.00f, 0.35f, 0.55f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.02f, 0.06f, 0.10f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.02f, 0.18f, 0.28f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.82f, 0.95f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.28f, 0.50f, 0.60f, 1.00f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.00f, 0.82f, 1.00f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.00f, 0.82f, 1.00f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.20f, 0.92f, 1.00f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.02f, 0.10f, 0.18f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.00f, 0.45f, 0.65f, 0.60f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.00f, 0.30f, 0.45f, 0.40f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.02f, 0.05f, 0.65f);

    // Sharp corners — lab equipment aesthetic
    s.WindowRounding    = 2.0f;
    s.ChildRounding     = 2.0f;
    s.FrameRounding     = 2.0f;
    s.PopupRounding     = 2.0f;
    s.ScrollbarRounding = 2.0f;
    s.GrabRounding      = 1.0f;
    s.TabRounding       = 2.0f;

    // Chat colors — deep teal/cyan palette
    t.bubble_self     = IM_COL32(  0,  55,  85, 225);
    t.bubble_self_top = IM_COL32(  0,  72, 108, 230);
    t.bubble_self_bot = IM_COL32(  0,  36,  58, 240);
    t.bubble_other    = IM_COL32( 18,  20,  36, 225);
    t.bubble_other_top= IM_COL32( 24,  26,  46, 230);
    t.bubble_other_bot= IM_COL32( 10,  12,  24, 240);
    t.bubble_rounding = 2.0f;

    t.header_bg    = IM_COL32(  2,   8,  18, 245);
    t.active_bg    = IM_COL32(  0,  55,  85, 180);
    t.input_bg     = IM_COL32(  4,  26,  42, 235);
    t.input_border = IM_COL32(  0, 190, 220, 150);
    t.avatar_bg    = IM_COL32(  0, 165, 215, 255);
    t.unread_dot   = IM_COL32(220,  40,  40, 255);
    t.pin_accent   = IM_COL32(  0, 210, 255, 255);

    t.sender_self   = ImVec4(0.00f, 0.88f, 1.00f, 1.0f);
    t.sender_other  = ImVec4(0.55f, 0.82f, 0.95f, 1.0f);
    t.timestamp     = ImVec4(0.25f, 0.48f, 0.60f, 1.0f);
    t.unread_label  = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    t.status_ok     = ImVec4(0.10f, 0.92f, 0.65f, 1.0f);
    t.status_warn   = ImVec4(1.00f, 0.80f, 0.00f, 1.0f);
    t.status_err    = ImVec4(1.00f, 0.30f, 0.30f, 1.0f);

    // Subtle blue tint at bottom of chat area (depth effect)
    t.chat_bg_top = IM_COL32(0, 0, 0, 0);
    t.chat_bg_bot = IM_COL32(0, 30, 50, 25);

    // Animation — snappy/urgent like a system alert
    t.bob_amplitude   = 5.0f;
    t.bob_period_ms   = 1200.0f;
    t.flash_period_ms = 500.0f;
    t.fade_ms         = 80.0f;

    // Draw hooks
    t.draw_chat_bg     = AsuranLabChatBg;
    t.draw_contacts_bg = AsuranLabContactsBg;

    return t;
}

static TyrianTheme BuildNexusTheme() {
    TyrianTheme t;
    t.name          = "Nexus";
    t.description   = "Inherits the ImGui style set by Nexus";
    t.inherit_style = true;
    // Snapshot the live Nexus style so DeriveChatColorsFromStyle can read it;
    // imgui_style is otherwise unused at render time (inherit_style skips the swap).
    t.imgui_style   = ImGui::GetStyle();
    DeriveChatColorsFromStyle(t);
    return t;
}

// ── Nyan Cat draw hooks ───────────────────────────────────────────────────────

static void NyanCatStars(ImDrawList* dl, ImVec2 mn, ImVec2 mx, int count) {
    float t = (float)ImGui::GetTime();
    float w = mx.x - mn.x;
    float h = mx.y - mn.y;
    constexpr float kCycle    = 2.5f;
    constexpr float kBaseSpeed = 123.75f; // px/sec base scroll speed (right to left)
    constexpr ImU32 kWhite    = IM_COL32(255, 255, 255, 220);
    constexpr float kAngles[8] = {
        0.000f, 0.785f, 1.571f, 2.356f,
        3.142f, 3.927f, 4.712f, 5.498f
    };

    for (int i = 0; i < count; i++) {
        // Low-discrepancy sequences in [0,1) — gives even spread across the panel
        float phi  = fmodf(i * 0.6180339f, 1.0f);  // golden ratio sequence
        float psi  = fmodf(i * 0.7548777f, 1.0f);  // plastic constant sequence

        // Fixed y per star, scrolling x (right to left, wraps at left edge)
        float speed  = kBaseSpeed * (0.6f + 0.8f * fmodf(i * 0.1234f, 1.0f));
        float sx     = mx.x - fmodf(t * speed + phi * w, w);
        float sy     = mn.y + psi * h;

        // Twinkling cycle — independent of scroll position
        float phase = i * (kCycle / count);
        float frac  = fmodf(t * 0.7f + phase, kCycle) / kCycle;

        if (frac < 0.15f) {
            // Stage 1: 4×4 dot
            dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + 4, sy + 4), kWhite);
        } else if (frac < 0.35f) {
            // Stage 2: 6×6 centre + 2px arms
            dl->AddRectFilled(ImVec2(sx - 2, sy - 2), ImVec2(sx + 4, sy + 4), kWhite);
            dl->AddRectFilled(ImVec2(sx,     sy - 4), ImVec2(sx + 2, sy - 2), kWhite);
            dl->AddRectFilled(ImVec2(sx,     sy + 4), ImVec2(sx + 2, sy + 6), kWhite);
            dl->AddRectFilled(ImVec2(sx - 4, sy    ), ImVec2(sx - 2, sy + 2), kWhite);
            dl->AddRectFilled(ImVec2(sx + 4, sy    ), ImVec2(sx + 6, sy + 2), kWhite);
        } else if (frac < 0.60f) {
            // Stage 3: 6×6 centre + 8px arms
            dl->AddRectFilled(ImVec2(sx - 2, sy - 2), ImVec2(sx + 4, sy + 4), kWhite);
            for (int a = 4; a <= 10; a += 2) {
                dl->AddRectFilled(ImVec2(sx,         sy - a    ), ImVec2(sx + 2, sy - a + 2), kWhite);
                dl->AddRectFilled(ImVec2(sx,         sy + a - 2), ImVec2(sx + 2, sy + a    ), kWhite);
                dl->AddRectFilled(ImVec2(sx - a,     sy        ), ImVec2(sx - a + 2, sy + 2), kWhite);
                dl->AddRectFilled(ImVec2(sx + a - 2, sy        ), ImVec2(sx + a,     sy + 2), kWhite);
            }
        } else if (frac < 0.80f) {
            // Stage 4: ring at radius 8, 2×2 dots
            for (float ang : kAngles) {
                float rx = sx + cosf(ang) * 8.0f;
                float ry = sy + sinf(ang) * 8.0f;
                dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + 2, ry + 2), kWhite);
            }
        }
        // Stage 5 (frac 0.80–1.0): invisible
    }
}

static void NyanCatRainbow(ImDrawList* dl, ImVec2 mn,
                            float cat_left_x, float cat_center_y) {
    static const ImU32 kBands[6] = {
        IM_COL32(255,  30,   0, 255),
        IM_COL32(255, 153,   0, 255),
        IM_COL32(255, 240,   0, 255),
        IM_COL32( 51, 210,   0, 255),
        IM_COL32( 51, 102, 255, 255),
        IM_COL32(160,   0, 255, 255),
    };
    constexpr float kBandH      = 8.0f;
    constexpr float kWaveAmp    = 6.0f;
    constexpr float kWaveLen    = 180.0f;
    constexpr float kWavePeriod = 0.8f;
    constexpr float kStripW     = 3.0f;

    float t          = (float)ImGui::GetTime();
    float k          = 2.0f * 3.14159265f / kWaveLen;
    float omega      = 2.0f * 3.14159265f / kWavePeriod;
    float rainbowTop = cat_center_y - 3.0f * kBandH;
    float span       = cat_left_x - mn.x;
    if (span <= 0.0f) return;

    for (float x = mn.x; x < cat_left_x; x += kStripW) {
        float wave     = kWaveAmp * sinf(k * (x - mn.x) + omega * t);
        float progress = (x - mn.x) / span;
        int   ia       = (int)((0.30f + progress * 0.65f) * 255.0f);

        for (int b = 0; b < 6; b++) {
            ImU32 col = (kBands[b] & 0x00FFFFFFu) | ((ImU32)ia << 24);
            float y0  = rainbowTop + wave + b * kBandH;
            dl->AddRectFilled(ImVec2(x, y0),
                              ImVec2(x + kStripW + 1.0f, y0 + kBandH + 1.0f), col);
        }
    }
}

static void NyanCatDrawChatBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    float w = mx.x - mn.x;
    float h = mx.y - mn.y;

    // Fit the sprite within the panel maintaining 498:280 aspect ratio
    float scale  = w / 498.0f;
    float cat_w  = w;
    float cat_h  = 280.0f * scale;
    if (cat_h > h * 0.55f) {
        cat_h = h * 0.55f;
        cat_w = cat_h * (498.0f / 280.0f);
    }
    float cat_left = mn.x + (w - cat_w) * 0.5f;
    float cat_top  = mn.y + h * 0.5f - cat_h * 0.5f;
    float cx = cat_left + cat_w * 0.5f;
    float cy = cat_top  + cat_h * 0.5f;

    // Radial glow — triangle fan from cat centre, fades to transparent at edge
    constexpr int   kSeg        = 32;
    constexpr ImU32 kGlowCentre = IM_COL32(210, 80, 220, 55);
    constexpr ImU32 kGlowOuter  = IM_COL32(210, 80, 220,  0);
    float radius = fminf(w, h) * 0.45f;
    ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    dl->PrimReserve(kSeg * 3, kSeg * 3);
    for (int j = 0; j < kSeg; j++) {
        float a0 = (j    ) * (2.0f * 3.14159265f / kSeg);
        float a1 = (j + 1) * (2.0f * 3.14159265f / kSeg);
        dl->PrimVtx(ImVec2(cx,                     cy                    ), uv, kGlowCentre);
        dl->PrimVtx(ImVec2(cx + cosf(a0) * radius, cy + sinf(a0) * radius), uv, kGlowOuter);
        dl->PrimVtx(ImVec2(cx + cosf(a1) * radius, cy + sinf(a1) * radius), uv, kGlowOuter);
    }

    NyanCatStars(dl, mn, mx, 20);

    int        frame = (int)((float)ImGui::GetTime() / 0.05f) % NYAN_FRAME_COUNT;
    Texture_t* tex   = s_NyanFrames[frame];
    if (tex && tex->Resource) {
        dl->AddImage(
            (ImTextureID)tex->Resource,
            ImVec2(cat_left, cat_top),
            ImVec2(cat_left + cat_w, cat_top + cat_h));
    }
}

static void NyanCatDrawContactsBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    NyanCatStars(dl, mn, mx, 8);
}

// ── Sylvari Grove draw hooks ──────────────────────────────────────────────────

static void SylvariVines(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float alpha) {
    float w = mx.x - mn.x, h = mx.y - mn.y;
    // Gently sway vine tips using a slow sine
    float sway = sinf((float)ImGui::GetTime() * 0.4f) * 6.0f;
    ImU32 vine  = IM_COL32(40, 160,  80, (int)(38 * alpha));
    ImU32 vine2 = IM_COL32(60, 200, 100, (int)(28 * alpha));
    ImU32 dot   = IM_COL32(80, 255, 140, (int)(90 * alpha));

    // Bottom-left: two branching vines rising from the corner
    ImVec2 bl(mn.x, mx.y);
    dl->AddBezierCubic(bl,
        ImVec2(mn.x + w*0.04f, mx.y - h*0.18f),
        ImVec2(mn.x + w*0.10f, mx.y - h*0.36f),
        ImVec2(mn.x + w*0.14f + sway, mx.y - h*0.52f),
        vine, 1.2f);
    // Branch
    dl->AddBezierCubic(
        ImVec2(mn.x + w*0.08f, mx.y - h*0.28f),
        ImVec2(mn.x + w*0.18f, mx.y - h*0.32f),
        ImVec2(mn.x + w*0.22f, mx.y - h*0.40f),
        ImVec2(mn.x + w*0.20f + sway*0.7f, mx.y - h*0.48f),
        vine2, 0.8f);
    // Leaf dot at tip
    dl->AddCircleFilled(ImVec2(mn.x + w*0.14f + sway, mx.y - h*0.52f), 2.8f, dot);
    dl->AddCircleFilled(ImVec2(mn.x + w*0.20f + sway*0.7f, mx.y - h*0.48f), 2.0f, dot);

    // Top-right: hanging vines descending
    float sway2 = sinf((float)ImGui::GetTime() * 0.3f + 1.2f) * 5.0f;
    ImVec2 tr(mx.x, mn.y);
    dl->AddBezierCubic(tr,
        ImVec2(mx.x - w*0.04f, mn.y + h*0.14f),
        ImVec2(mx.x - w*0.12f, mn.y + h*0.28f),
        ImVec2(mx.x - w*0.16f + sway2, mn.y + h*0.42f),
        vine, 1.2f);
    dl->AddBezierCubic(
        ImVec2(mx.x - w*0.07f, mn.y + h*0.20f),
        ImVec2(mx.x - w*0.20f, mn.y + h*0.22f),
        ImVec2(mx.x - w*0.26f, mn.y + h*0.30f),
        ImVec2(mx.x - w*0.24f + sway2*0.6f, mn.y + h*0.38f),
        vine2, 0.8f);
    dl->AddCircleFilled(ImVec2(mx.x - w*0.16f + sway2, mn.y + h*0.42f), 2.8f, dot);
    dl->AddCircleFilled(ImVec2(mx.x - w*0.24f + sway2*0.6f, mn.y + h*0.38f), 2.0f, dot);
}

static void SylvariParticles(ImDrawList* dl, ImVec2 mn, ImVec2 mx, int count) {
    float w = mx.x - mn.x, h = mx.y - mn.y;
    float t = (float)ImGui::GetTime();
    for (int i = 0; i < count; i++) {
        float phi   = i * 0.6180339f;                          // golden ratio spacing
        float speed = 12.0f + phi * 22.0f;                     // varied drift speed
        float drift = sinf(t * 0.5f + phi * 4.0f) * 8.0f;     // gentle horizontal sway
        float px = mn.x + fmodf(phi * w * 2.3f + drift, w);
        float py = mx.y - fmodf(t * speed + phi * h, h);       // wrap top to bottom
        float pulse = 0.45f + 0.55f * sinf(t * 1.8f + phi * 6.28f);
        float radius = 1.0f + pulse * 1.4f;
        int   a      = (int)(18 + pulse * 52);
        // Alternate green and cyan-green particles
        ImU32 col = (i % 3 == 0)
            ? IM_COL32(60, 255, 160, a)
            : (i % 3 == 1) ? IM_COL32(100, 220, 255, a)
                           : IM_COL32(40, 200, 100, a);
        dl->AddCircleFilled(ImVec2(px, py), radius, col);
        // Occasional larger glow bloom
        if (i % 5 == 0 && pulse > 0.75f)
            dl->AddCircleFilled(ImVec2(px, py), radius * 2.8f,
                IM_COL32(60, 220, 140, (int)(a * 0.25f)));
    }
}

static void SylvariGroveDrawChatBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    float h = mx.y - mn.y;

    // Ground mist: faint green gradient rising from the bottom
    dl->AddRectFilledMultiColor(
        ImVec2(mn.x, mx.y - h * 0.30f), mx,
        IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
        IM_COL32(10, 50, 20, 40), IM_COL32(10, 50, 20, 40));

    // Bioluminescent particles
    SylvariParticles(dl, mn, mx, 22);

    // Swaying vines
    SylvariVines(dl, mn, mx, 1.0f);

    // Soft top vignette (canopy shadow)
    dl->AddRectFilledMultiColor(
        mn, ImVec2(mx.x, mn.y + h * 0.08f),
        IM_COL32(2,12,5,80), IM_COL32(2,12,5,80),
        IM_COL32(0,0,0,0),   IM_COL32(0,0,0,0));
}

static void SylvariGroveDrawContactsBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    SylvariParticles(dl, mn, mx, 10);
    SylvariVines(dl, mn, mx, 0.7f);
}

static TyrianTheme BuildSylvariGroveTheme() {
    TyrianTheme t;
    t.name        = "Sylvari Grove";
    t.description = "Bioluminescent forest — rising particles, swaying vines, organic shapes";

    ImGuiStyle& s = t.imgui_style;
    s = BuildGW2Theme();

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.03f, 0.06f, 0.04f, 0.97f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.03f, 0.06f, 0.03f, 0.82f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.04f, 0.08f, 0.05f, 0.96f);
    c[ImGuiCol_Border]               = ImVec4(0.15f, 0.55f, 0.28f, 0.40f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.06f, 0.18f, 0.09f, 0.65f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.08f, 0.26f, 0.13f, 0.75f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.10f, 0.36f, 0.18f, 0.85f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.03f, 0.08f, 0.04f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.06f, 0.20f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.03f, 0.07f, 0.04f, 0.75f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.04f, 0.09f, 0.05f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.02f, 0.05f, 0.03f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.15f, 0.55f, 0.28f, 0.70f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.20f, 0.72f, 0.38f, 0.85f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.25f, 0.88f, 0.46f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.25f, 0.92f, 0.50f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.20f, 0.72f, 0.38f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.28f, 0.92f, 0.52f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.08f, 0.26f, 0.13f, 0.75f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.12f, 0.40f, 0.20f, 0.85f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.16f, 0.55f, 0.28f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.10f, 0.32f, 0.16f, 0.65f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.14f, 0.46f, 0.23f, 0.75f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.18f, 0.60f, 0.30f, 0.85f);
    c[ImGuiCol_Separator]            = ImVec4(0.12f, 0.42f, 0.20f, 0.40f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.18f, 0.60f, 0.30f, 0.60f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.25f, 0.88f, 0.46f, 1.00f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.15f, 0.50f, 0.24f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.20f, 0.68f, 0.34f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.25f, 0.88f, 0.46f, 0.90f);
    c[ImGuiCol_Tab]                  = ImVec4(0.04f, 0.14f, 0.07f, 0.86f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.12f, 0.40f, 0.20f, 0.90f);
    c[ImGuiCol_TabActive]            = ImVec4(0.09f, 0.30f, 0.15f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.03f, 0.08f, 0.04f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.06f, 0.18f, 0.09f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.80f, 0.95f, 0.84f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.28f, 0.52f, 0.35f, 1.00f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.25f, 0.88f, 0.46f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.25f, 0.88f, 0.46f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.35f, 1.00f, 0.58f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.04f, 0.14f, 0.07f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.14f, 0.46f, 0.22f, 0.60f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.09f, 0.30f, 0.15f, 0.40f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.04f, 0.02f, 0.65f);

    // Organic rounding — soft living shapes
    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 6.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.GrabRounding      = 6.0f;
    s.TabRounding       = 6.0f;

    // Chat colors — deep forest palette
    t.bubble_self     = IM_COL32( 15,  65,  30, 225);
    t.bubble_self_top = IM_COL32( 20,  82,  38, 230);
    t.bubble_self_bot = IM_COL32(  8,  42,  18, 240);
    t.bubble_other    = IM_COL32( 18,  35,  22, 215);
    t.bubble_other_top= IM_COL32( 22,  44,  26, 220);
    t.bubble_other_bot= IM_COL32( 10,  22,  14, 235);
    t.bubble_rounding = 14.0f;

    t.header_bg    = IM_COL32(  5,  16,   8, 245);
    t.active_bg    = IM_COL32( 18,  70,  32, 185);
    t.input_bg     = IM_COL32( 12,  30,  16, 235);
    t.input_border = IM_COL32( 45, 200,  85, 140);
    t.avatar_bg    = IM_COL32( 30, 180,  80, 255);
    t.unread_dot   = IM_COL32(220,  40,  40, 255);
    t.pin_accent   = IM_COL32( 55, 230, 110, 255);

    t.sender_self   = ImVec4(0.28f, 0.96f, 0.54f, 1.0f);
    t.sender_other  = ImVec4(0.62f, 0.88f, 0.68f, 1.0f);
    t.timestamp     = ImVec4(0.28f, 0.50f, 0.34f, 1.0f);
    t.unread_label  = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    t.status_ok     = ImVec4(0.28f, 0.96f, 0.54f, 1.0f);
    t.status_warn   = ImVec4(0.90f, 0.82f, 0.10f, 1.0f);
    t.status_err    = ImVec4(1.00f, 0.35f, 0.30f, 1.0f);

    // Rising ground mist
    t.chat_bg_top = IM_COL32(0, 0, 0, 0);
    t.chat_bg_bot = IM_COL32(8, 35, 14, 30);

    // Slow, breathing animation — like a living forest
    t.bob_amplitude   = 3.5f;
    t.bob_period_ms   = 3200.0f;
    t.flash_period_ms = 1600.0f;
    t.fade_ms         = 280.0f;

    t.draw_chat_bg     = SylvariGroveDrawChatBg;
    t.draw_contacts_bg = SylvariGroveDrawContactsBg;

    return t;
}

// ── Divinity's Reach draw hooks ───────────────────────────────────────────────

static void DivinityDustMotes(ImDrawList* dl, ImVec2 mn, ImVec2 mx, int count) {
    float w = mx.x - mn.x, h = mx.y - mn.y;
    float t = (float)ImGui::GetTime();
    for (int i = 0; i < count; i++) {
        float phi   = i * 0.6180339f;
        float speed = 6.0f + phi * 10.0f;
        float drift = sinf(t * 0.3f + phi * 5.0f) * 14.0f;
        float px    = mn.x + fmodf(phi * w * 2.1f + drift, w);
        float py    = mx.y - fmodf(t * speed + phi * h, h);
        float pulse = 0.35f + 0.65f * sinf(t * 0.9f + phi * 6.28f);
        float r     = 0.6f + pulse * 0.9f;
        int   a     = (int)(10 + pulse * 28);
        // warm golden dust
        ImU32 col = (i % 2 == 0)
            ? IM_COL32(255, 220, 140, a)
            : IM_COL32(255, 200,  90, a);
        dl->AddCircleFilled(ImVec2(px, py), r, col);
    }
}

static void DivinityLightRays(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    float w = mx.x - mn.x, h = mx.y - mn.y;
    float t = (float)ImGui::GetTime();
    // Gentle breathe: rays pulse slowly in opacity
    float breathe = 0.50f + 0.50f * sinf(t * 0.22f);

    // Source: upper-right corner (like light through a high arched window)
    ImVec2 src(mx.x - w * 0.08f, mn.y - h * 0.05f);

    struct Ray { float angL, angR, alpha; };
    const Ray rays[] = {
        { -0.62f, -0.48f, 0.045f },
        { -0.52f, -0.40f, 0.060f },
        { -0.44f, -0.34f, 0.048f },
        { -0.36f, -0.26f, 0.038f },
        { -0.72f, -0.60f, 0.030f },
    };
    float len = sqrtf(w * w + h * h) * 1.2f;
    for (auto& ray : rays) {
        float a1 = ray.angL + (float)M_PI, a2 = ray.angR + (float)M_PI;
        ImVec2 p1(src.x + cosf(a1) * len, src.y + sinf(a1) * len);
        ImVec2 p2(src.x + cosf(a2) * len, src.y + sinf(a2) * len);
        int alpha = (int)(ray.alpha * breathe * 255.0f);
        ImU32 col = IM_COL32(255, 230, 160, alpha);
        dl->AddTriangleFilled(src, p1, p2, col);
    }
}

static void DivinityReachDrawChatBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    float h = mx.y - mn.y;
    // Warm ambient glow rising from below
    dl->AddRectFilledMultiColor(
        ImVec2(mn.x, mx.y - h * 0.25f), mx,
        IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
        IM_COL32(60, 40, 10, 28), IM_COL32(60, 40, 10, 28));

    DivinityLightRays(dl, mn, mx);
    DivinityDustMotes(dl, mn, mx, 18);

    // Subtle top vignette
    dl->AddRectFilledMultiColor(
        mn, ImVec2(mx.x, mn.y + h * 0.06f),
        IM_COL32(20, 14, 4, 60), IM_COL32(20, 14, 4, 60),
        IM_COL32(0,0,0,0),       IM_COL32(0,0,0,0));
}

static void DivinityReachDrawContactsBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    DivinityLightRays(dl, mn, mx);
    DivinityDustMotes(dl, mn, mx, 8);
}

static TyrianTheme BuildDivinityReachTheme() {
    TyrianTheme t;
    t.name        = "Divinity's Reach";
    t.description = "Warm ivory and gold — cathedral light rays, drifting dust, royal human elegance";

    ImGuiStyle& s = t.imgui_style;
    s = BuildGW2Theme();

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.08f, 0.05f, 0.97f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.09f, 0.07f, 0.04f, 0.85f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.13f, 0.10f, 0.06f, 0.96f);
    c[ImGuiCol_Border]               = ImVec4(0.55f, 0.42f, 0.18f, 0.45f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.12f, 0.06f, 0.75f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.24f, 0.18f, 0.08f, 0.85f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.34f, 0.25f, 0.10f, 0.90f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.08f, 0.04f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.22f, 0.16f, 0.06f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.08f, 0.06f, 0.03f, 0.75f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.12f, 0.09f, 0.04f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.04f, 0.02f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.50f, 0.38f, 0.14f, 0.75f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.65f, 0.50f, 0.18f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.80f, 0.62f, 0.22f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.90f, 0.72f, 0.20f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.72f, 0.55f, 0.16f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.92f, 0.74f, 0.22f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.22f, 0.16f, 0.06f, 0.80f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.36f, 0.26f, 0.09f, 0.90f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.52f, 0.38f, 0.12f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.20f, 0.14f, 0.05f, 0.70f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.34f, 0.24f, 0.08f, 0.80f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.48f, 0.34f, 0.11f, 0.90f);
    c[ImGuiCol_Separator]            = ImVec4(0.44f, 0.32f, 0.10f, 0.40f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.60f, 0.44f, 0.15f, 0.70f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.80f, 0.60f, 0.20f, 1.00f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.40f, 0.28f, 0.08f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.60f, 0.44f, 0.14f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.80f, 0.60f, 0.20f, 0.90f);
    c[ImGuiCol_Tab]                  = ImVec4(0.13f, 0.09f, 0.04f, 0.86f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.36f, 0.26f, 0.09f, 0.90f);
    c[ImGuiCol_TabActive]            = ImVec4(0.26f, 0.19f, 0.07f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.08f, 0.06f, 0.03f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.16f, 0.12f, 0.04f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.94f, 0.90f, 0.80f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.52f, 0.46f, 0.32f, 1.00f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.88f, 0.70f, 0.20f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.80f, 0.62f, 0.18f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.95f, 0.76f, 0.24f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.10f, 0.04f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.40f, 0.28f, 0.08f, 0.60f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.26f, 0.18f, 0.06f, 0.40f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.02f, 0.01f, 0.00f, 0.60f);

    // Elegant rounding — refined but not as soft as Sylvari
    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 6.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;

    // Chat colors — warm stone and royal blue
    t.bubble_self     = IM_COL32( 30,  50,  95, 220);
    t.bubble_self_top = IM_COL32( 42,  66, 122, 225);
    t.bubble_self_bot = IM_COL32( 16,  30,  62, 235);
    t.bubble_other    = IM_COL32( 52,  40,  20, 215);
    t.bubble_other_top= IM_COL32( 66,  50,  24, 220);
    t.bubble_other_bot= IM_COL32( 30,  22,  10, 235);
    t.bubble_rounding = 10.0f;

    t.header_bg    = IM_COL32( 18,  14,   6, 245);
    t.active_bg    = IM_COL32( 38,  28,  80, 185);
    t.input_bg     = IM_COL32( 22,  17,   8, 235);
    t.input_border = IM_COL32(180, 138,  45, 130);
    t.avatar_bg    = IM_COL32(180, 140,  40, 255);
    t.unread_dot   = IM_COL32(220,  40,  40, 255);
    t.pin_accent   = IM_COL32(220, 175,  50, 255);

    t.sender_self   = ImVec4(0.55f, 0.75f, 1.00f, 1.0f);
    t.sender_other  = ImVec4(0.94f, 0.88f, 0.72f, 1.0f);
    t.timestamp     = ImVec4(0.50f, 0.44f, 0.30f, 1.0f);
    t.unread_label  = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    t.status_ok     = ImVec4(0.40f, 0.88f, 0.35f, 1.0f);
    t.status_warn   = ImVec4(1.00f, 0.75f, 0.10f, 1.0f);
    t.status_err    = ImVec4(1.00f, 0.32f, 0.28f, 1.0f);

    // Warm amber glow at the bottom
    t.chat_bg_top = IM_COL32(0, 0, 0, 0);
    t.chat_bg_bot = IM_COL32(40, 25, 5, 30);

    // Measured, stately animation
    t.bob_amplitude   = 3.0f;
    t.bob_period_ms   = 2800.0f;
    t.flash_period_ms = 1200.0f;
    t.fade_ms         = 220.0f;

    t.draw_chat_bg     = DivinityReachDrawChatBg;
    t.draw_contacts_bg = DivinityReachDrawContactsBg;

    return t;
}

// ── Hoelbrak draw hooks ───────────────────────────────────────────────────────

static void HoelbrakSnowflakes(ImDrawList* dl, ImVec2 mn, ImVec2 mx, int count) {
    float w = mx.x - mn.x, h = mx.y - mn.y;
    float t = (float)ImGui::GetTime();
    for (int i = 0; i < count; i++) {
        float phi    = i * 0.6180339f;
        float speed  = 18.0f + phi * 28.0f;
        float wobble = sinf(t * 0.4f + phi * 7.0f) * 12.0f;
        float px     = mn.x + fmodf(phi * w * 1.9f + wobble, w);
        float py     = mn.y + fmodf(t * speed + phi * h, h);
        float pulse  = 0.4f + 0.6f * sinf(t * 1.2f + phi * 5.0f);
        float r      = 0.7f + pulse * 1.0f;
        int   a      = (int)(14 + pulse * 36);
        ImU32 col    = (i % 3 == 0)
            ? IM_COL32(180, 220, 255, a)
            : (i % 3 == 1) ? IM_COL32(220, 240, 255, a)
                           : IM_COL32(140, 200, 255, a);
        dl->AddCircleFilled(ImVec2(px, py), r, col);
        // Occasional slightly larger flake
        if (i % 4 == 0 && pulse > 0.70f)
            dl->AddCircleFilled(ImVec2(px, py), r * 2.2f,
                IM_COL32(200, 230, 255, (int)(a * 0.30f)));
    }
}

static void HoelbrakAurora(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    float w = mx.x - mn.x, h = mx.y - mn.y;
    float t = (float)ImGui::GetTime();

    // Three overlapping aurora bands rippling across the top ~30% of the panel
    struct Band { float yBase; ImU32 col; float amp; float freq; float speed; };
    const Band bands[] = {
        { 0.08f, IM_COL32( 40, 200, 180,  22), 0.04f, 2.2f, 0.28f },
        { 0.16f, IM_COL32( 60, 140, 255,  18), 0.05f, 1.8f, 0.20f },
        { 0.24f, IM_COL32(120,  80, 220,  14), 0.04f, 2.6f, 0.35f },
    };

    constexpr int segs = 24;
    for (auto& b : bands) {
        float segW = w / (float)segs;
        for (int i = 0; i < segs; i++) {
            float x0 = mn.x + i * segW;
            float x1 = x0 + segW + 1.0f;  // +1 to avoid gaps
            float phase0 = (float)i / segs;
            float phase1 = (float)(i + 1) / segs;
            float y0top = mn.y + h * (b.yBase + b.amp * sinf(b.freq * phase0 * (float)M_PI * 2.0f + t * b.speed));
            float y1top = mn.y + h * (b.yBase + b.amp * sinf(b.freq * phase1 * (float)M_PI * 2.0f + t * b.speed));
            float bandH  = h * 0.10f;
            // Each segment: thin quad — approximate with two triangles
            ImVec2 tl(x0, y0top),          tr(x1, y1top);
            ImVec2 bl(x0, y0top + bandH),  br(x1, y1top + bandH);
            dl->AddTriangleFilled(tl, tr, br, b.col);
            dl->AddTriangleFilled(tl, br, bl, b.col);
        }
    }
}

static void HoelbrakDrawChatBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    float h = mx.y - mn.y;
    // Deep cold vignette at the bottom — like frost on stone floor
    dl->AddRectFilledMultiColor(
        ImVec2(mn.x, mx.y - h * 0.20f), mx,
        IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
        IM_COL32(10, 25, 50, 35), IM_COL32(10, 25, 50, 35));

    HoelbrakAurora(dl, mn, mx);
    HoelbrakSnowflakes(dl, mn, mx, 20);

    // Soft bottom vignette
    dl->AddRectFilledMultiColor(
        mn, ImVec2(mx.x, mn.y + h * 0.05f),
        IM_COL32(5, 10, 22, 50), IM_COL32(5, 10, 22, 50),
        IM_COL32(0,0,0,0),       IM_COL32(0,0,0,0));
}

static void HoelbrakDrawContactsBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    HoelbrakAurora(dl, mn, mx);
    HoelbrakSnowflakes(dl, mn, mx, 9);
}

static TyrianTheme BuildHoelbrakTheme() {
    TyrianTheme t;
    t.name        = "Hoelbrak";
    t.description = "Icy navy and silver — aurora shimmer, falling snow, Norn mountain strength";

    ImGuiStyle& s = t.imgui_style;
    s = BuildGW2Theme();

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.05f, 0.07f, 0.12f, 0.97f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.04f, 0.06f, 0.10f, 0.85f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.07f, 0.09f, 0.16f, 0.96f);
    c[ImGuiCol_Border]               = ImVec4(0.28f, 0.46f, 0.68f, 0.45f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.14f, 0.22f, 0.75f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.14f, 0.20f, 0.32f, 0.85f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.18f, 0.28f, 0.44f, 0.90f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.05f, 0.07f, 0.13f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.16f, 0.28f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.04f, 0.06f, 0.10f, 0.75f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.06f, 0.08f, 0.14f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.03f, 0.04f, 0.08f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.46f, 0.68f, 0.75f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.60f, 0.85f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.55f, 0.82f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.40f, 0.65f, 0.92f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.58f, 0.84f, 1.00f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.10f, 0.16f, 0.28f, 0.80f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.18f, 0.28f, 0.46f, 0.90f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.26f, 0.40f, 0.64f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.12f, 0.18f, 0.30f, 0.70f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.28f, 0.46f, 0.80f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.24f, 0.38f, 0.60f, 0.90f);
    c[ImGuiCol_Separator]            = ImVec4(0.22f, 0.36f, 0.54f, 0.40f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.34f, 0.54f, 0.78f, 0.70f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.50f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.22f, 0.36f, 0.54f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.36f, 0.56f, 0.80f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.50f, 0.75f, 1.00f, 0.90f);
    c[ImGuiCol_Tab]                  = ImVec4(0.06f, 0.09f, 0.16f, 0.86f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.18f, 0.28f, 0.46f, 0.90f);
    c[ImGuiCol_TabActive]            = ImVec4(0.12f, 0.20f, 0.34f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.04f, 0.06f, 0.10f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.08f, 0.12f, 0.20f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.88f, 0.92f, 0.98f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.36f, 0.48f, 0.62f, 1.00f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.55f, 0.82f, 1.00f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.46f, 0.72f, 0.96f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.60f, 0.86f, 1.00f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.07f, 0.10f, 0.18f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.24f, 0.38f, 0.56f, 0.60f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.14f, 0.22f, 0.36f, 0.40f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.02f, 0.06f, 0.65f);

    // Sturdy corners — heavy but not industrial
    s.WindowRounding    = 4.0f;
    s.ChildRounding     = 3.0f;
    s.FrameRounding     = 3.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 3.0f;

    // Chat colors — deep navy with icy blue
    t.bubble_self     = IM_COL32( 20,  44,  90, 225);
    t.bubble_self_top = IM_COL32( 28,  58, 115, 230);
    t.bubble_self_bot = IM_COL32( 10,  26,  58, 240);
    t.bubble_other    = IM_COL32( 28,  38,  60, 215);
    t.bubble_other_top= IM_COL32( 36,  48,  76, 220);
    t.bubble_other_bot= IM_COL32( 16,  22,  40, 235);
    t.bubble_rounding = 5.0f;

    t.header_bg    = IM_COL32( 10,  14,  26, 245);
    t.active_bg    = IM_COL32( 22,  48,  90, 185);
    t.input_bg     = IM_COL32( 12,  18,  34, 235);
    t.input_border = IM_COL32( 85, 165, 230, 145);
    t.avatar_bg    = IM_COL32( 70, 150, 220, 255);
    t.unread_dot   = IM_COL32(220,  40,  40, 255);
    t.pin_accent   = IM_COL32(100, 190, 255, 255);

    t.sender_self   = ImVec4(0.55f, 0.82f, 1.00f, 1.0f);
    t.sender_other  = ImVec4(0.82f, 0.90f, 0.98f, 1.0f);
    t.timestamp     = ImVec4(0.36f, 0.50f, 0.66f, 1.0f);
    t.unread_label  = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    t.status_ok     = ImVec4(0.36f, 0.90f, 0.60f, 1.0f);
    t.status_warn   = ImVec4(1.00f, 0.78f, 0.12f, 1.0f);
    t.status_err    = ImVec4(1.00f, 0.34f, 0.30f, 1.0f);

    // Cold blue-indigo at the bottom
    t.chat_bg_top = IM_COL32(0, 0, 0, 0);
    t.chat_bg_bot = IM_COL32(8, 18, 40, 32);

    // Slow, heavy — a giant's pace
    t.bob_amplitude   = 4.5f;
    t.bob_period_ms   = 3600.0f;
    t.flash_period_ms = 1400.0f;
    t.fade_ms         = 260.0f;

    t.draw_chat_bg     = HoelbrakDrawChatBg;
    t.draw_contacts_bg = HoelbrakDrawContactsBg;

    return t;
}

static TyrianTheme BuildNyanCatTheme() {
    TyrianTheme t;
    t.name        = "Nyan Cat";
    t.description = "Pop-Tart cat flies through space — animated rainbow trail, pixel-art stars";
    t.author      = "TyrianIM";

    ImGuiStyle& s = t.imgui_style;
    s = BuildGW2DarkTheme().imgui_style;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.19f, 0.38f, 0.97f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.08f, 0.15f, 0.30f, 0.85f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.08f, 0.15f, 0.30f, 0.97f);
    c[ImGuiCol_Border]               = ImVec4(0.50f, 0.20f, 0.60f, 0.45f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.05f, 0.10f, 0.22f, 0.90f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.12f, 0.20f, 0.40f, 0.90f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.15f, 0.25f, 0.50f, 0.90f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.15f, 0.30f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.22f, 0.45f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.05f, 0.10f, 0.22f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.45f, 0.15f, 0.50f, 0.80f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.20f, 0.65f, 0.80f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.75f, 0.25f, 0.80f, 0.90f);
    c[ImGuiCol_Button]               = ImVec4(0.50f, 0.15f, 0.55f, 0.75f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.65f, 0.20f, 0.70f, 0.85f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.75f, 0.25f, 0.80f, 0.95f);
    c[ImGuiCol_Header]               = ImVec4(0.50f, 0.15f, 0.55f, 0.60f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.60f, 0.18f, 0.65f, 0.70f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.70f, 0.22f, 0.75f, 0.80f);
    c[ImGuiCol_Separator]            = ImVec4(0.40f, 0.15f, 0.50f, 0.50f);
    c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.55f, 0.70f, 1.00f);

    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 6.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 6.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 6.0f;
    s.TabRounding       = 6.0f;

    t.bubble_self_top  = IM_COL32(210,  80, 180, 215);
    t.bubble_self_bot  = IM_COL32(130,  35, 175, 225);
    t.bubble_self      = t.bubble_self_top;
    t.bubble_other_top = IM_COL32( 28,  52, 120, 200);
    t.bubble_other_bot = IM_COL32( 14,  28,  70, 215);
    t.bubble_other     = t.bubble_other_top;
    t.bubble_rounding  = 6.0f;
    t.header_bg        = IM_COL32( 10,  18,  48, 245);
    t.active_bg        = IM_COL32(190,  55, 170, 180);
    t.input_bg         = IM_COL32( 12,  22,  58, 220);
    t.input_border     = IM_COL32(190,  70, 210, 160);
    t.avatar_bg        = IM_COL32(170,  50, 200, 255);
    t.pin_accent       = IM_COL32(255,  80,  80, 255);
    t.unread_dot       = IM_COL32(255,  80,  80, 255);
    t.sender_self      = ImVec4(1.00f, 0.55f, 0.85f, 1.0f);
    t.sender_other     = ImVec4(0.45f, 0.80f, 1.00f, 1.0f);
    t.timestamp        = ImVec4(0.50f, 0.60f, 0.80f, 0.65f);

    t.draw_chat_bg     = NyanCatDrawChatBg;
    t.draw_contacts_bg = NyanCatDrawContactsBg;

    return t;
}

using ThemeBuilder = TyrianTheme(*)();
static const ThemeBuilder kBuiltinThemes[] = {
    BuildNexusTheme,
    BuildGW2DarkTheme,
    BuildCharrSteelTheme,
    BuildAsuranLabTheme,
    BuildSylvariGroveTheme,
    BuildDivinityReachTheme,
    BuildHoelbrakTheme,
    BuildNyanCatTheme,
};

static void ScanThemes() {
    g_LoadedThemes.clear();
    for (auto builder : kBuiltinThemes)
        g_LoadedThemes.push_back(builder());

    std::string dir = ThemesDir();
    try { std::filesystem::create_directories(dir); } catch (...) {}
    try {
        for (auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".toml") continue;
            auto theme = LoadThemeFromTOML(entry.path().string());
            if (theme.has_value())
                g_LoadedThemes.push_back(std::move(*theme));
        }
    } catch (...) {}

    // Sort file-loaded entries (after builtins) alphabetically
    constexpr size_t kNumBuiltins = std::size(kBuiltinThemes);
    if (g_LoadedThemes.size() > kNumBuiltins)
        std::sort(g_LoadedThemes.begin() + kNumBuiltins, g_LoadedThemes.end(),
                  [](const TyrianTheme& a, const TyrianTheme& b){ return a.name < b.name; });
}

// --- Floating notification icon ---

static void RenderFloatingIcon() {
    if (!g_ShowFloatingIcon) return;

    uint64_t now_ms = NowEpochMs();

    // Flash continuously while there are unread messages from any contact
    int unread_contact_count = TyrianIM::ChatManager::GetUnreadContactCount();
    bool is_flashing = (unread_contact_count > 0);

    // "Only show on message" — hide when not flashing
    if (g_FloatingIconOnlyOnUnread && !is_flashing) return;
    float flash_alpha = 0.0f;
    if (is_flashing) {
        float t = (float)(now_ms % (uint64_t)g_ActiveTheme.flash_period_ms) / g_ActiveTheme.flash_period_ms;
        flash_alpha = 0.5f + 0.5f * sinf(t * 2.0f * 3.14159f);
    }

    float bobOffset = 0.0f;
    if (is_flashing) {
        float bobT = (float)(now_ms % (uint64_t)g_ActiveTheme.bob_period_ms) / g_ActiveTheme.bob_period_ms;
        bobOffset = sinf(bobT * 2.0f * 3.14159f) * g_ActiveTheme.bob_amplitude;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoBackground;
    if (g_FloatingIconLocked) {
        flags |= ImGuiWindowFlags_NoMove;
    }

    // Apply bob every frame so the animation is visible.
    // ImGuiCond_Always is used, so ImGui's built-in drag is disabled — we handle it manually below.
    ImGui::SetNextWindowPos(ImVec2(g_FloatingIconX, g_FloatingIconY + bobOffset), ImGuiCond_Always);

    // Fully transparent window — bubble is drawn directly, no window background
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    float s = g_FloatingIconScale;
    ImGui::SetNextWindowSize(ImVec2(60 * s, 60 * s));
    ImGui::Begin("##TyrianIM_FloatingIcon", nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImFont* font = ImGui::GetFont();

    // Invisible button over the full icon — owns the input via ImGui's item system,
    // which reliably distinguishes click from drag.
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##FloatIconBtn", ImGui::GetWindowSize());

    static bool s_IconWasDragged = false;

    if (!g_FloatingIconLocked && ImGui::IsItemActive()) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            g_FloatingIconX += delta.x;
            g_FloatingIconY += delta.y;
            s_IconWasDragged = true;
        }
    }

    if (ImGui::IsItemDeactivated()) {
        if (s_IconWasDragged) {
            SaveSettings();
        } else {
            // Pure click — toggle main window and navigate to unread
            g_WindowVisible = !g_WindowVisible;
            if (g_WindowVisible) {
                g_ScrollToBottom = true;
                if (unread_contact_count > 0) {
                    for (auto* c : TyrianIM::ChatManager::GetConversations()) {
                        if (c->unread_count > 0) {
                            // Persist current draft before switching
                            if (!g_SelectedContact.empty()) {
                                if (g_InputBuf[0])
                                    g_Drafts[g_SelectedContact] = g_InputBuf;
                                else
                                    g_Drafts.erase(g_SelectedContact);
                            }
                            g_SelectedContact = c->contact;
                            // Restore draft for the new contact
                            auto dit = g_Drafts.find(c->contact);
                            if (dit != g_Drafts.end()) {
                                strncpy(g_InputBuf, dit->second.c_str(), sizeof(g_InputBuf) - 1);
                                g_InputBuf[sizeof(g_InputBuf) - 1] = '\0';
                            } else {
                                g_InputBuf[0] = '\0';
                            }
                            g_FocusInput = true;
                            break;
                        }
                    }
                }
            }
        }
        s_IconWasDragged = false;
    }

    // Resolve theme icon (if set)
    if (!g_ActiveTheme.icon_texture && !g_ActiveTheme.icon_texture_path.empty() && APIDefs) {
        std::string id   = "TYRIANIMTHEME_ICON_" + g_ActiveTheme.name;
        std::string full = ThemesDir() + "/" + g_ActiveTheme.icon_texture_path;
        g_ActiveTheme.icon_texture = APIDefs->Textures_GetOrCreateFromFile(id.c_str(), full.c_str());
    }
    // Resolve default floating icon
    if (!g_FloatIconTexture && APIDefs)
        g_FloatIconTexture = APIDefs->Textures_Get(TEX_FLOAT_ICON);

    // Use theme icon if available, otherwise fall back to default
    Texture_t* activeIcon = (g_ActiveTheme.icon_texture && g_ActiveTheme.icon_texture->Resource)
                          ? g_ActiveTheme.icon_texture : g_FloatIconTexture;

    // Image bounds — small margin inside the window
    float bx0 = wp.x + 4.0f * s, by0 = wp.y + 4.0f * s;
    float bx1 = wp.x + 56.0f * s, by1 = wp.y + 56.0f * s;

    // Compute tint: white when idle, blends toward accent colour when flashing.
    // AddImage multiplies each pixel by the tint, so transparent PNG areas stay
    // transparent — the flash naturally follows the icon's exact shape.
    ImVec4 accent = ImGui::ColorConvertU32ToFloat4(g_ActiveTheme.pin_accent);
    ImVec4 tint(
        1.0f - flash_alpha * 0.6f * (1.0f - accent.x),
        1.0f - flash_alpha * 0.6f * (1.0f - accent.y),
        1.0f - flash_alpha * 0.6f * (1.0f - accent.z),
        1.0f);

    if (activeIcon && activeIcon->Resource) {
        dl->AddImage((ImTextureID)activeIcon->Resource,
                     ImVec2(bx0, by0), ImVec2(bx1, by1),
                     ImVec2(0, 0), ImVec2(1, 1),
                     ImGui::ColorConvertFloat4ToU32(tint));
    } else {
        // Fallback until texture loads
        dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1, by1),
                          g_ActiveTheme.bubble_self, 8.0f * s);
    }

    // Unread contact count badge
    if (unread_contact_count > 0) {
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        float badgeRadius = 8.0f * s;
        ImVec2 badgeCenter(bx1 - 2.0f * s, by0 - 2.0f * s);
        fdl->AddCircleFilled(badgeCenter, badgeRadius, IM_COL32(220, 40, 40, 255), 16);
        char badgeBuf[8];
        int displayCount = (unread_contact_count > 99) ? 99 : unread_contact_count;
        snprintf(badgeBuf, sizeof(badgeBuf), "%d", displayCount);
        float badgeFs = font->FontSize * s * 0.75f;
        ImVec2 badgeTextSize = font->CalcTextSizeA(badgeFs, FLT_MAX, 0.0f, badgeBuf);
        fdl->AddText(font, badgeFs,
            ImVec2(badgeCenter.x - badgeTextSize.x * 0.5f, badgeCenter.y - badgeTextSize.y * 0.5f),
            IM_COL32(255, 255, 255, 255), badgeBuf);
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
    ImGui::BeginChild("##Contacts", ImVec2(width, 0), true);

    if (g_ActiveTheme.draw_contacts_bg) {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImVec2 cMin = ImGui::GetWindowPos();
        ImVec2 cMax = ImVec2(cMin.x + ImGui::GetWindowWidth(), cMin.y + ImGui::GetWindowHeight());
        g_ActiveTheme.draw_contacts_bg(cdl, cMin, cMax);
    }

    ImFont* font = ImGui::GetFont();
    float fs = font->FontSize * g_FontScale;

    ImGui::Spacing();
    ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], "  CONVERSATIONS");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto conversations = TyrianIM::ChatManager::GetConversations();

    if (conversations.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("No conversations yet.");
        ImGui::Spacing();
        ImGui::TextColored(g_ActiveTheme.timestamp, "Whispers will appear here.");
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (const auto* convo : conversations) {

        bool is_selected = (g_SelectedContact == convo->contact);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float itemHeight = 30.0f + 18.0f * g_FontScale;

        if (is_selected) {
            dl->AddRectFilled(
                ImVec2(cursor.x, cursor.y),
                ImVec2(cursor.x + width - 16, cursor.y + itemHeight),
                g_ActiveTheme.active_bg, 6.0f);
        }

        if (convo->pinned) {
            dl->AddRectFilled(
                ImVec2(cursor.x + 2, cursor.y + 3),
                ImVec2(cursor.x + 4, cursor.y + itemHeight - 3),
                g_ActiveTheme.pin_accent);
        }

        if (ImGui::Selectable(("##contact_" + convo->contact).c_str(), false, 0, ImVec2(0, itemHeight))) {
            // Persist current draft before switching
            if (!g_SelectedContact.empty()) {
                if (g_InputBuf[0])
                    g_Drafts[g_SelectedContact] = g_InputBuf;
                else
                    g_Drafts.erase(g_SelectedContact);
            }
            g_SelectedContact = convo->contact;
            // Restore draft for the new contact
            auto dit = g_Drafts.find(convo->contact);
            if (dit != g_Drafts.end()) {
                strncpy(g_InputBuf, dit->second.c_str(), sizeof(g_InputBuf) - 1);
                g_InputBuf[sizeof(g_InputBuf) - 1] = '\0';
            } else {
                g_InputBuf[0] = '\0';
            }
            g_ScrollToBottom = true;
            g_FocusInput = true;
            double now = ImGui::GetTime();
            for (auto it = g_MessageArrivalTimes.begin(); it != g_MessageArrivalTimes.end(); ) {
                if (now - it->second > 2.0) it = g_MessageArrivalTimes.erase(it);
                else ++it;
            }
        }

        // Right-click context menu
        std::string popupId = "##ctx_" + convo->contact;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            g_ContextMenuContactPinned = TyrianIM::ChatManager::IsPinned(convo->contact);
            ImGui::OpenPopup(popupId.c_str());
        }
        if (ImGui::BeginPopup(popupId.c_str())) {
            if (ImGui::MenuItem("Edit note\xe2\x80\xa6")) {
                g_NoteEditContact = convo->contact;
                auto nit2 = g_ContactNotes.find(convo->contact);
                const std::string& n = (nit2 != g_ContactNotes.end()) ? nit2->second : std::string();
                strncpy(g_NoteEditBuf, n.c_str(), sizeof(g_NoteEditBuf) - 1);
                g_NoteEditBuf[sizeof(g_NoteEditBuf) - 1] = '\0';
                g_NoteEditOpen = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(g_ContextMenuContactPinned ? "Unpin" : "Pin")) {
                TyrianIM::ChatManager::TogglePin(convo->contact);
                SaveSettings();
            }
            ImGui::Separator();
            bool srcIsAccount = IsAccountName(convo->contact);
            if (ImGui::BeginMenu("Merge into...")) {
                for (const auto* target : conversations) {
                    if (target->contact == convo->contact) continue;
                    if (srcIsAccount && IsAccountName(target->contact)) continue;
                    std::string targetLabel = !target->display_name.empty() ? target->display_name : target->contact;
                    if (!target->display_name.empty() && target->display_name != target->contact)
                        targetLabel += " (" + target->contact + ")";
                    if (ImGui::MenuItem(targetLabel.c_str())) {
                        if (g_SelectedContact == convo->contact)
                            g_SelectedContact = target->contact;
                        TyrianIM::ChatManager::MergeConversation(convo->contact, target->contact);
                        TyrianIM::ChatManager::SaveHistory();
                        g_ScrollToBottom = true;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
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
        float letterHue = (nameHash % 360) / 360.0f;
        float baseR = ((g_ActiveTheme.avatar_bg >> IM_COL32_R_SHIFT) & 0xFF) / 255.f;
        float baseG = ((g_ActiveTheme.avatar_bg >> IM_COL32_G_SHIFT) & 0xFF) / 255.f;
        float baseB = ((g_ActiveTheme.avatar_bg >> IM_COL32_B_SHIFT) & 0xFF) / 255.f;
        float avH, avS, avV;
        ImGui::ColorConvertRGBtoHSV(baseR, baseG, baseB, avH, avS, avV);
        avH = fmodf(avH + letterHue, 1.0f);
        float avR, avG, avB;
        ImGui::ColorConvertHSVtoRGB(avH, avS, avV, avR, avG, avB);
        ImU32 avatarFill = IM_COL32((int)(avR*255), (int)(avG*255), (int)(avB*255), 255);
        dl->AddCircleFilled(avatarCenter, avatarRadius, avatarFill);

        {
            ImVec4 ringColor;
            ImGui::ColorConvertHSVtoRGB(avH, 0.9f, 1.0f, ringColor.x, ringColor.y, ringColor.z);
            ringColor.w = 1.0f;
            float ringRadius = std::min(avatarRadius + 2.0f, itemHeight * 0.5f - 1.0f);
            dl->AddCircle(avatarCenter, ringRadius,
                          ImGui::ColorConvertFloat4ToU32(ringColor), 32, 1.5f);
        }

        // Initial letter (scaled)
        char initial[2] = { 0, 0 };
        std::string displayStr = !convo->display_name.empty() ? convo->display_name : convo->contact;
        if (!displayStr.empty()) initial[0] = (char)toupper(displayStr[0]);
        ImVec2 initSize = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, initial);
        dl->AddText(font, fs,
            ImVec2(avatarCenter.x - initSize.x * 0.5f, avatarCenter.y - initSize.y * 0.5f),
            IM_COL32(255, 255, 255, 230), initial);

        // Text content (scaled)
        float textX = cursor.x + 8 + avatarRadius * 2 + 10;
        ImVec4 nameColor = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        std::string primaryName = !convo->display_name.empty() ? convo->display_name : convo->contact;
        dl->AddText(font, fs, ImVec2(textX, cursor.y + 6),
            ImGui::ColorConvertFloat4ToU32(nameColor), primaryName.c_str());

        // Unread count badge next to name
        if (convo->unread_count > 0) {
            char unreadBuf[8];
            snprintf(unreadBuf, sizeof(unreadBuf), "%d", convo->unread_count > 99 ? 99 : convo->unread_count);
            ImVec2 nameSize = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, primaryName.c_str());
            float badgeRadius = fs * 0.55f;
            ImVec2 badgeCenter(textX + nameSize.x + badgeRadius + 5.0f, cursor.y + 6.0f + fs * 0.5f);
            dl->AddCircleFilled(badgeCenter, badgeRadius, IM_COL32(220, 40, 40, 255), 12);
            float badgeFs = fs * 0.72f;
            ImVec2 countSize = font->CalcTextSizeA(badgeFs, FLT_MAX, 0.0f, unreadBuf);
            dl->AddText(font, badgeFs,
                ImVec2(badgeCenter.x - countSize.x * 0.5f, badgeCenter.y - countSize.y * 0.5f),
                IM_COL32(255, 255, 255, 255), unreadBuf);
        }


        // Subtitle: note takes priority over account name
        {
            float subFs = font->FontSize * (g_FontScale * 0.85f);
            auto nit = g_ContactNotes.find(convo->contact);
            if (nit != g_ContactNotes.end() && !nit->second.empty()) {
                const std::string& noteText = nit->second;
                size_t nl = noteText.find('\n');
                size_t firstLineLen = (nl == std::string::npos) ? noteText.size() : nl;
                bool truncated = firstLineLen > 32;
                std::string snippet = noteText.substr(0, std::min(firstLineLen, (size_t)32));
                if (truncated) snippet += "\xe2\x80\xa6"; // … only when first line exceeded 32 chars
                dl->AddText(font, subFs, ImVec2(textX, cursor.y + 6 + fs + 2),
                    IM_COL32(160, 200, 160, 180), snippet.c_str());
            } else if (!convo->display_name.empty() && convo->display_name != convo->contact) {
                dl->AddText(font, subFs, ImVec2(textX, cursor.y + 6 + fs + 2),
                    ImGui::ColorConvertFloat4ToU32(g_ActiveTheme.timestamp), convo->contact.c_str());
            }
        }
    }

    // Confirmation modal (rendered inside the Contacts child window)
    if (!g_PendingDeleteContact.empty()) {
        ImGui::OpenPopup("Confirm Delete");
    }
    if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Delete conversation with");
        ImGui::TextColored(g_ActiveTheme.sender_other, "%s", g_PendingDeleteContact.c_str());
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

    if (g_NoteEditOpen) {
        ImGui::OpenPopup("Edit Note##NoteEdit");
        g_NoteEditOpen = false;
    }
    if (ImGui::BeginPopupModal("Edit Note##NoteEdit", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(g_ActiveTheme.sender_other, "%s", g_NoteEditContact.c_str());
        ImGui::SetNextItemWidth(320.0f);
        ImGui::InputTextMultiline("##notetext", g_NoteEditBuf, sizeof(g_NoteEditBuf),
            ImVec2(320, 80));
        ImGui::Spacing();
        if (ImGui::Button("Save", ImVec2(100, 0))) {
            if (g_NoteEditBuf[0])
                g_ContactNotes[g_NoteEditContact] = g_NoteEditBuf;
            else
                g_ContactNotes.erase(g_NoteEditContact);
            SaveSettings();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::EndChild();
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
        ImGui::TextColored(g_ActiveTheme.sender_self, "%s", title);

        ImGui::Spacing();
        const char* subtitle = "Select a conversation or wait for messages";
        float sub_w = ImGui::CalcTextSize(subtitle).x;
        ImGui::SetCursorPosX((avail.x - sub_w) * 0.5f);
        ImGui::TextColored(g_ActiveTheme.timestamp, "%s", subtitle);

        ImGui::Spacing();
        ImGui::Spacing();
        auto status = TyrianIM::WhisperHook::GetStatus();
        const char* status_label = "Whisper Hook: ";
        float status_w = ImGui::CalcTextSize(status_label).x + ImGui::CalcTextSize(TyrianIM::WhisperHook::GetStatusString()).x;
        ImGui::SetCursorPosX((avail.x - status_w) * 0.5f);
        ImGui::Text("%s", status_label);
        ImGui::SameLine();
        ImVec4 status_color = (status == TyrianIM::HookStatus::Hooked) ? g_ActiveTheme.status_ok
                            : (status == TyrianIM::HookStatus::Failed) ? g_ActiveTheme.status_err
                            : g_ActiveTheme.status_warn;
        ImGui::TextColored(status_color, "%s", TyrianIM::WhisperHook::GetStatusString());

    } else {
        auto* convo = TyrianIM::ChatManager::GetConversation(g_SelectedContact);
        if (!convo) {
            ImGui::Text("Conversation not found.");
            ImGui::EndChild();
            return;
        }

        // Clear unread badge while this conversation is visible (guard avoids mutex lock every frame when already read)
        {
            auto* convo = TyrianIM::ChatManager::GetConversation(g_SelectedContact);
            if (convo && convo->unread_count > 0)
                TyrianIM::ChatManager::MarkAsRead(g_SelectedContact);
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
                g_ActiveTheme.header_bg, 4.0f);

            // Avatar in header
            float hAvatarR = 8.0f + 6.0f * g_FontScale;
            ImVec2 hAvatarC(hPos.x + 14 + hAvatarR, hPos.y + headerH * 0.5f);
            unsigned int hHash = 0;
            for (char c : convo->contact) hHash = hHash * 31 + (unsigned char)c;
            float hLetterHue = (hHash % 360) / 360.0f;
            float hBaseR = ((g_ActiveTheme.avatar_bg >> IM_COL32_R_SHIFT) & 0xFF) / 255.f;
            float hBaseG = ((g_ActiveTheme.avatar_bg >> IM_COL32_G_SHIFT) & 0xFF) / 255.f;
            float hBaseB = ((g_ActiveTheme.avatar_bg >> IM_COL32_B_SHIFT) & 0xFF) / 255.f;
            float hAvH, hAvS, hAvV;
            ImGui::ColorConvertRGBtoHSV(hBaseR, hBaseG, hBaseB, hAvH, hAvS, hAvV);
            hAvH = fmodf(hAvH + hLetterHue, 1.0f);
            float hAvR, hAvG, hAvB;
            ImGui::ColorConvertHSVtoRGB(hAvH, hAvS, hAvV, hAvR, hAvG, hAvB);
            ImU32 hAvatarFill = IM_COL32((int)(hAvR*255), (int)(hAvG*255), (int)(hAvB*255), 255);
            hdl->AddCircleFilled(hAvatarC, hAvatarR, hAvatarFill);

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
                    ImGui::ColorConvertFloat4ToU32(g_ActiveTheme.timestamp), convo->contact.c_str());
            }

            ImGui::Dummy(ImVec2(0, headerH + 4));
        }

        // --- Messages area ---
        float input_height = ImGui::GetFrameHeightWithSpacing() + 8;
        ImGui::BeginChild("##Messages", ImVec2(0, -input_height), false);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Background texture (lazy-loaded per frame until resolved)
        if (!g_ActiveTheme.bg_texture && !g_ActiveTheme.bg_texture_path.empty() && APIDefs) {
            std::string id   = "TYRIANIMTHEME_BG_" + g_ActiveTheme.name;
            std::string full = ThemesDir() + "/" + g_ActiveTheme.bg_texture_path;
            g_ActiveTheme.bg_texture = APIDefs->Textures_GetOrCreateFromFile(id.c_str(), full.c_str());
        }
        if (g_ActiveTheme.bg_texture && g_ActiveTheme.bg_texture->Resource) {
            ImVec2 bgMin = ImGui::GetWindowPos();
            ImVec2 bgMax = ImVec2(bgMin.x + ImGui::GetWindowWidth(), bgMin.y + ImGui::GetWindowHeight());
            dl->AddImage((ImTextureID)g_ActiveTheme.bg_texture->Resource, bgMin, bgMax);
        }

        // Chat area background overlay
        if ((g_ActiveTheme.chat_bg_top | g_ActiveTheme.chat_bg_bot) & 0xFF000000u) {
            ImVec2 bgMin = ImGui::GetWindowPos();
            ImVec2 bgMax = ImVec2(bgMin.x + ImGui::GetWindowWidth(), bgMin.y + ImGui::GetWindowHeight());
            dl->AddRectFilledMultiColor(bgMin, bgMax,
                g_ActiveTheme.chat_bg_top, g_ActiveTheme.chat_bg_top,
                g_ActiveTheme.chat_bg_bot, g_ActiveTheme.chat_bg_bot);
        }

        // Theme draw hook (compiled-in themes: custom patterns/lines/effects)
        if (g_ActiveTheme.draw_chat_bg) {
            ImVec2 bgMin = ImGui::GetWindowPos();
            ImVec2 bgMax = ImVec2(bgMin.x + ImGui::GetWindowWidth(), bgMin.y + ImGui::GetWindowHeight());
            g_ActiveTheme.draw_chat_bg(dl, bgMin, bgMax);
        }

        ImFont* font = ImGui::GetFont();
        float fs = font->FontSize * g_FontScale;
        float areaWidth = ImGui::GetContentRegionAvail().x;
        float padding = 10.0f;
        float bubbleRound = g_ActiveTheme.bubble_rounding;

        // --- Bubble layout cache management ---
        bool cacheInvalid = (g_BubbleCache.contact != convo->contact);
        bool layoutParamsChanged = (g_BubbleCache.areaWidth != areaWidth || g_BubbleCache.fontScale != g_FontScale);
        bool newMessages = (convo->messages.size() != g_BubbleCache.messageCount);

        if (cacheInvalid) {
            // Switched conversation — full rebuild synchronously (user action, acceptable)
            g_BubbleCache.contact = convo->contact;
            g_BubbleCache.areaWidth = areaWidth;
            g_BubbleCache.fontScale = g_FontScale;
            g_BubbleCache.messageCount = convo->messages.size();
            g_BubbleCache.layouts.resize(convo->messages.size());
            g_LayoutQueue.clear();
            for (size_t i = 0; i < convo->messages.size(); i++) {
                ComputeBubbleLayout(convo, i, areaWidth, g_FontScale, font);
            }
            // Request API resolution for all pending links in the newly viewed conversation
            RequestConversationLinks(convo);
        } else if (layoutParamsChanged) {
            // Width or scale changed — queue all for amortized recompute
            g_BubbleCache.areaWidth = areaWidth;
            g_BubbleCache.fontScale = g_FontScale;
            g_BubbleCache.messageCount = convo->messages.size();
            g_BubbleCache.layouts.resize(convo->messages.size());
            g_LayoutQueue.clear();
            for (size_t i = 0; i < convo->messages.size(); i++) {
                g_LayoutQueue.push_back(i);
            }
        } else if (newMessages) {
            // New messages appended — compute only the new ones synchronously
            size_t oldCount = g_BubbleCache.messageCount;
            g_BubbleCache.messageCount = convo->messages.size();
            g_BubbleCache.layouts.resize(convo->messages.size());
            for (size_t i = oldCount; i < convo->messages.size(); i++) {
                ComputeBubbleLayout(convo, i, areaWidth, g_FontScale, font);
                // Request resolution for any links in new messages
                const auto& nmsg = convo->messages[i];
                if (nmsg.has_links) {
                    for (size_t si = 0; si < nmsg.segments.size(); si++) {
                        const auto& seg = nmsg.segments[si];
                        if (seg.is_link && seg.link.state == TyrianIM::LinkState::Pending)
                            TyrianIM::GW2API::RequestLink(convo->contact, i, si, seg.link);
                    }
                }
            }
        }

        // Link resolved on the API worker thread — queue all layouts for recompute
        if (g_LinksDirty.exchange(false)) {
            for (size_t i = 0; i < g_BubbleCache.layouts.size(); i++)
                g_LayoutQueue.push_back(i);
        }

        // Process amortized batch (for resize/scale changes)
        TickLayoutQueue(convo, areaWidth, g_FontScale, font);

        ImGui::Spacing();

        // Static buffer for InvisibleButton IDs (avoid per-message heap allocation)
        char bubbleIdBuf[32];

        auto applyAlpha = [](ImU32 col, float a) -> ImU32 {
            ImU32 origA = (col >> 24) & 0xFF;
            return (col & 0x00FFFFFF) | ((ImU32)(origA * a) << 24);
        };

        for (size_t mi = 0; mi < convo->messages.size() && mi < g_BubbleCache.layouts.size(); mi++) {
            const auto& msg = convo->messages[mi];
            const auto& layout = g_BubbleCache.layouts[mi];

            float msgAlpha = 1.0f;
            if (msg.epoch_ms >= g_SessionStartMs && g_SessionStartMs > 0) {
                uint64_t mapKey = msg.epoch_ms ^ ((uint64_t)mi << 32);
                double& arrivalTime = g_MessageArrivalTimes[mapKey];
                if (arrivalTime == 0.0) arrivalTime = ImGui::GetTime();
                float elapsed = (float)(ImGui::GetTime() - arrivalTime);
                msgAlpha = std::min(1.0f, elapsed / (g_ActiveTheme.fade_ms / 1000.0f));
            }

            // System/error messages — centered, distinct style
            if (layout.isSystem) {
                float sysFs = font->FontSize * (g_FontScale * 0.85f);
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                float sysX = cursor.x + (areaWidth - layout.sysW) * 0.5f;
                dl->AddRectFilled(
                    ImVec2(sysX, cursor.y), ImVec2(sysX + layout.sysW, cursor.y + layout.sysH),
                    applyAlpha(g_ActiveTheme.bubble_other, 0.7f), 8.0f);
                font->RenderText(dl, sysFs,
                    ImVec2(sysX + padding, cursor.y + padding),
                    ImGui::ColorConvertFloat4ToU32(g_ActiveTheme.status_err),
                    ImVec4(sysX + padding, cursor.y + padding, sysX + layout.sysW - padding, cursor.y + layout.sysH),
                    msg.text.c_str(), msg.text.c_str() + msg.text.size(), areaWidth * 0.8f);
                ImGui::Dummy(ImVec2(0, layout.sysH + 6));
                continue;
            }

            bool is_self = (msg.direction == TyrianIM::MessageDirection::Outgoing);

            // Position: right-aligned for self, left-aligned for other
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            float bubbleX = is_self ? (cursor.x + areaWidth - layout.bubbleW - 8) : (cursor.x + 8);

            // Draw bubble
            ImVec2 bubbleTL(bubbleX, cursor.y);
            ImVec2 bubbleBR(bubbleX + layout.bubbleW, cursor.y + layout.bubbleH);
            ImU32 topCol = applyAlpha(is_self ? g_ActiveTheme.bubble_self_top : g_ActiveTheme.bubble_other_top, msgAlpha);
            ImU32 botCol = applyAlpha(is_self ? g_ActiveTheme.bubble_self_bot : g_ActiveTheme.bubble_other_bot, msgAlpha);
            if (topCol == botCol) {
                dl->AddRectFilled(bubbleTL, bubbleBR, topCol, bubbleRound);
            } else {
                // AddRectFilledMultiColor doesn't support rounding; gradient themes use sharp corners
                dl->AddRectFilledMultiColor(bubbleTL, bubbleBR, topCol, topCol, botCol, botCol);
            }

            // Failed message indicator: red border + red "!" to the left
            if (msg.failed) {
                dl->AddRect(
                    ImVec2(bubbleX, cursor.y),
                    ImVec2(bubbleX + layout.bubbleW, cursor.y + layout.bubbleH),
                    applyAlpha(ImGui::ColorConvertFloat4ToU32(g_ActiveTheme.status_err), msgAlpha), bubbleRound, 0, 2.0f);
                const char* failIcon = "!";
                ImVec2 failSize = font->CalcTextSizeA(fs * 1.2f, FLT_MAX, 0.0f, failIcon);
                float failX = bubbleX - failSize.x - 6;
                float failY = cursor.y + (layout.bubbleH - failSize.y) * 0.5f;
                dl->AddText(font, fs * 1.2f, ImVec2(failX, failY),
                    applyAlpha(ImGui::ColorConvertFloat4ToU32(g_ActiveTheme.status_err), msgAlpha), failIcon);
            }

            // Sender name (left of bubble)
            ImVec4 nameCol = is_self ? g_ActiveTheme.sender_self : g_ActiveTheme.sender_other;
            nameCol.w *= msgAlpha;
            dl->AddText(font, fs, ImVec2(bubbleX + padding, cursor.y + padding),
                ImGui::ColorConvertFloat4ToU32(nameCol), layout.senderLabel.c_str());

            // Timestamp (right of name line, slightly smaller)
            if (g_ShowTimestamps) {
                float timeFs = font->FontSize * (g_FontScale * 0.85f);
                ImVec4 tsCol = g_ActiveTheme.timestamp;
                tsCol.w *= msgAlpha;
                dl->AddText(font, timeFs,
                    ImVec2(bubbleX + layout.bubbleW - padding - layout.timeSize.x, cursor.y + padding + (layout.nameSize.y - layout.timeSize.y)),
                    ImGui::ColorConvertFloat4ToU32(tsCol), layout.displayTimestamp.c_str());
            }

            // Message text (wrapped) — plain or segmented
            float textY = cursor.y + padding + layout.nameSize.y + 4;
            ImU32 textCol = applyAlpha(
                ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), msgAlpha);
            ImVec4 clipRect(bubbleX + padding, textY,
                            bubbleX + layout.bubbleW - padding, textY + layout.msgSize.y + 10);
            ImVec2 textOrigin(bubbleX + padding, textY);

            if (!msg.has_links) {
                font->RenderText(dl, fs, textOrigin, textCol, clipRect,
                    msg.text.c_str(), msg.text.c_str() + msg.text.size(), layout.textWrapWidth);
            } else {
                for (int si = 0; si < (int)msg.segments.size() && si < (int)layout.seg_layouts.size(); si++) {
                    const auto& seg = msg.segments[si];
                    const auto& sl  = layout.seg_layouts[si];
                    ImVec2 sp(textOrigin.x + sl.pos.x, textOrigin.y + sl.pos.y);
                    if (!seg.is_link) {
                        font->RenderText(dl, fs, sp, textCol, clipRect,
                            seg.text.c_str(), seg.text.c_str() + seg.text.size(), layout.textWrapWidth);
                    } else {
                        ImU32 lc = applyAlpha(seg.link.colour, msgAlpha);
                        dl->AddText(font, fs, sp, lc, seg.link.display.c_str());
                        // Underline
                        dl->AddLine(ImVec2(sp.x, sp.y + sl.size.y - 2),
                                    ImVec2(sp.x + sl.size.x, sp.y + sl.size.y - 2), lc);
                        // Spinner while the API fetch is in progress
                        if (seg.link.state == TyrianIM::LinkState::Pending) {
                            static const char* kSpinFrames[] = { "|", "/", "-", "\\" };
                            int frame = static_cast<int>(ImGui::GetTime() * 8.0) & 3;
                            ImU32 sc = applyAlpha(IM_COL32(180, 180, 180, 200), msgAlpha);
                            dl->AddText(font, fs,
                                ImVec2(sp.x + sl.size.x + 3.0f, sp.y),
                                sc, kSpinFrames[frame]);
                        }
                    }
                }
            }

            // Whole-bubble invisible button (copy text on click; lower priority than link buttons)
            ImGui::SetCursorScreenPos(ImVec2(bubbleX, cursor.y));
            snprintf(bubbleIdBuf, sizeof(bubbleIdBuf), "##b_%zu", mi);
            if (ImGui::InvisibleButton(bubbleIdBuf, ImVec2(layout.bubbleW, layout.bubbleH))) {
                ImGui::SetClipboardText(NormalizeGW2Text(msg.text).c_str());
                g_ClipboardMsg = "Copied to clipboard";
                g_ClipboardMsgExpiry = ImGui::GetTime() + 2.0;
            }
            bool bubbleHovered = ImGui::IsItemHovered();
            if (bubbleHovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (msg.failed)
                    ImGui::SetTooltip("Message not delivered — player is offline");
            }

            // Per-link buttons rendered on top of the bubble button — they win hover/click in their area
            if (msg.has_links) {
                for (int si = 0; si < (int)msg.segments.size() && si < (int)layout.seg_layouts.size(); si++) {
                    const auto& seg = msg.segments[si];
                    const auto& sl  = layout.seg_layouts[si];
                    if (!seg.is_link) continue;
                    ImVec2 sp(textOrigin.x + sl.pos.x, textOrigin.y + sl.pos.y);
                    ImGui::SetCursorScreenPos(sp);
                    char linkIdBuf[48];
                    snprintf(linkIdBuf, sizeof(linkIdBuf), "##lnk_%zu_%d", mi, si);
                    ImGui::InvisibleButton(linkIdBuf, sl.size);
                    bool lHovered = ImGui::IsItemHovered();
                    bool lClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                    if (lHovered) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::BeginTooltip();

                        const std::string& tt  = seg.link.tooltip_text;
                        ImVec4 rarityCol = ImGui::ColorConvertU32ToFloat4(seg.link.colour);
                        ImVec4 grayCol   = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);

                        // Split tooltip_text into lines, handle icon prefix on first line
                        int tt_start = 0;
                        if (!tt.empty() && tt[0] == '\x03') {
                            // Format: \x03TexID|remote|endpoint\n
                            size_t nl = tt.find('\n');
                            std::string icon_line = tt.substr(1, nl == std::string::npos ? std::string::npos : nl - 1);
                            size_t p1 = icon_line.find('|');
                            size_t p2 = (p1 != std::string::npos) ? icon_line.find('|', p1 + 1) : std::string::npos;
                            if (p1 != std::string::npos && p2 != std::string::npos) {
                                std::string tex_id   = icon_line.substr(0, p1);
                                std::string remote   = icon_line.substr(p1 + 1, p2 - p1 - 1);
                                std::string endpoint = icon_line.substr(p2 + 1);
                                Texture_t* tex = APIDefs ? APIDefs->Textures_Get(tex_id.c_str()) : nullptr;
                                if (!tex && APIDefs)
                                    APIDefs->Textures_LoadFromURL(tex_id.c_str(), remote.c_str(), endpoint.c_str(), nullptr);
                                if (tex && tex->Resource) {
                                    ImGui::Image((ImTextureID)tex->Resource, ImVec2(32.0f, 32.0f));
                                    ImGui::SameLine();
                                }
                            }
                            tt_start = (nl == std::string::npos) ? (int)tt.size() : (int)nl + 1;
                        }

                        // Item name in rarity colour (beside icon if present)
                        ImGui::TextColored(rarityCol, "%s", seg.link.display.c_str());

                        // Remaining lines — extract each line, handle prefix markers
                        size_t pos = (size_t)tt_start;
                        while (pos < tt.size()) {
                            size_t nl  = tt.find('\n', pos);
                            size_t end = (nl == std::string::npos) ? tt.size() : nl;
                            if (end == pos) {
                                ImGui::Spacing();
                            } else {
                                char prefix = tt[pos];
                                if (prefix == '\x01') {
                                    std::string line(tt.begin() + pos + 1, tt.begin() + end);
                                    ImGui::TextColored(rarityCol, "%s", line.c_str());
                                } else if (prefix == '\x02') {
                                    std::string line(tt.begin() + pos + 1, tt.begin() + end);
                                    ImGui::TextColored(grayCol, "%s", line.c_str());
                                } else {
                                    ImGui::TextUnformatted(tt.c_str() + pos, tt.c_str() + end);
                                }
                            }
                            pos = (nl == std::string::npos) ? tt.size() : nl + 1;
                        }

                        ImGui::EndTooltip();
                    }
                    if (lClicked) {
                        if (seg.link.type == TyrianIM::GW2LinkType::Url) {
                            const std::string& url = seg.link.raw;
                            bool safe = url.compare(0, 7, "http://")  == 0 ||
                                        url.compare(0, 8, "https://") == 0;
                            if (safe) {
                                ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                                g_ClipboardMsg = "Opening in browser\xe2\x80\xa6";
                                g_ClipboardMsgExpiry = ImGui::GetTime() + 2.0;
                            }
                        } else {
                            std::string ctxt =
                                (seg.link.type == TyrianIM::GW2LinkType::AE2Build &&
                                 !seg.link.ae2_chat_link.empty())
                                ? seg.link.ae2_chat_link : NormalizeGW2Text(seg.link.raw);
                            ImGui::SetClipboardText(ctxt.c_str());
                            if (seg.link.type == TyrianIM::GW2LinkType::MapLink)
                                g_ClipboardMsg = "Waypoint link copied \xe2\x80\x94 paste in chat to use";
                            else if (seg.link.type == TyrianIM::GW2LinkType::AE2Build)
                                g_ClipboardMsg = "Build link copied";
                            else
                                g_ClipboardMsg = "Link copied";
                            g_ClipboardMsgExpiry = ImGui::GetTime() + 2.5;
                        }
                    }
                }
            }

            // Advance cursor
            ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + layout.bubbleH + 6));
        }

        if (g_ScrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            g_ScrollToBottom = false;
        }

        ImGui::EndChild();

        // --- Input area ---
        {
            float sendBtnW = 60.0f;
            float availW = ImGui::GetContentRegionAvail().x;
            ImGui::PushItemWidth(availW - sendBtnW - 8);
            bool hasBorder = (g_ActiveTheme.input_border >> 24) > 0;
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(g_ActiveTheme.input_bg));
            if (hasBorder) {
                ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(g_ActiveTheme.input_border));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            }
            if (g_FocusInput) {
                ImGui::SetKeyboardFocusHere();
                g_FocusInput = false;
            }
            bool enter_pressed = ImGui::InputText("##WhisperInput", g_InputBuf, sizeof(g_InputBuf),
                ImGuiInputTextFlags_EnterReturnsTrue);
            if (hasBorder) {
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleColor();
            ImGui::PopItemWidth();

            ImGui::SameLine();
            bool can_send = !g_IsSending && g_InputBuf[0] != '\0';
            if (!can_send) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            bool send_clicked = ImGui::Button("Send", ImVec2(sendBtnW, 0));
            if (!can_send) {
                ImGui::PopStyleVar();
                send_clicked = false;
            }

            if ((enter_pressed || send_clicked) && can_send) {
                std::string text(g_InputBuf);

                // Use display_name (character name) for /w if available, otherwise account name
                std::string whisper_target = !convo->display_name.empty() ? convo->display_name : convo->contact;

                // Set flag on render thread before spawning so it's visible immediately next frame
                g_IsSending = true;
                SendWhisperViaChatbox(whisper_target, text);
                g_InputBuf[0] = '\0';
                g_Drafts.erase(g_SelectedContact);
                g_FocusInput = true;
            }
        }

        ImGui::SetWindowFontScale(1.0f);
    }


    ImGui::EndChild();
}

static const char* ChanTypeName(ChatMessageType t) {
    switch (t) {
        case ChatMsg_Local:   return "Say";
        case ChatMsg_Map:     return "Map";
        case ChatMsg_Party:   return "Party";
        case ChatMsg_Guild:   return "Guild";
        case ChatMsg_Squad:   return "Squad";
        case ChatMsg_TeamPvP: return "Team";
        case ChatMsg_TeamWvW: return "Team";
        default:              return "Chat";
    }
}

static void RenderUrlToast() {
    if (!g_UrlToastEnabled) return;

    std::unique_lock<std::mutex> lk(g_UrlToastMutex);
    bool hasReal = !g_UrlToastQueue.empty();
    UrlToast toast;
    if (hasReal) {
        toast = g_UrlToastQueue.front();
        g_UrlToastShowDummy = false; // real toast supersedes dummy
    }
    lk.unlock();

    if (!hasReal && !g_UrlToastShowDummy) return;

    ImGuiIO& io = ImGui::GetIO();
    constexpr float kW = 320.0f, kH = 94.0f, kMargin = 16.0f;

    float px = (g_UrlToastX >= 0.0f) ? g_UrlToastX : io.DisplaySize.x - kW - kMargin;
    float py = (g_UrlToastY >= 0.0f) ? g_UrlToastY : io.DisplaySize.y - kH - kMargin;
    ImGui::SetNextWindowPos(ImVec2(px, py),
        g_UrlToastLocked ? ImGuiCond_Always : ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(kW, kH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoNav
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoBringToFrontOnFocus
                           | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoTitleBar;
    if (g_UrlToastLocked)
        flags |= ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##UrlToast", nullptr, flags)) {
        if (!hasReal) {
            // Dummy toast for position adjustment
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextUnformatted("[Map] Example.1234 shared a link");
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            ImGui::TextUnformatted("example.com");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            if (ImGui::Button("Done — Lock Position")) {
                g_UrlToastLocked    = true;
                g_UrlToastShowDummy = false;
                SaveSettings();
            }
        } else {
        std::string header = std::string("[") + ChanTypeName(toast.chanType) + "] "
                           + toast.charName + " shared a link";
        ImGui::TextUnformatted(header.c_str());

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
        ImGui::TextUnformatted(toast.domain.c_str());
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", toast.url.c_str());

        ImGui::Spacing();

        if (ImGui::Button("Open in Browser")) {
            ShellExecuteA(NULL, "open", toast.url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            std::lock_guard<std::mutex> lk2(g_UrlToastMutex);
            g_UrlToastQueue.pop_front();
        }
        ImGui::SameLine();
        if (ImGui::Button("Dismiss")) {
            std::lock_guard<std::mutex> lk2(g_UrlToastMutex);
            g_UrlToastQueue.pop_front();
        }
        } // end else (real toast)

        if (!g_UrlToastLocked) {
            ImVec2 pos = ImGui::GetWindowPos();
            if (pos.x != g_UrlToastX || pos.y != g_UrlToastY) {
                g_UrlToastX = pos.x;
                g_UrlToastY = pos.y;
                SaveSettings();
            }
        }
    }
    ImGui::End();
}

// Main render function
void AddonRender() {
    ThemeGuard themeGuard;

    // Check if the player is logged into a character (not on character select)
    bool isInGame = g_NexusLink && g_NexusLink->IsGameplay;

    if (!isInGame) {
        // On character select — hide window and icon
        g_WindowVisible = false;
        return;
    }

    // Play deferred notification sound on render thread (Wine compatibility)
    if (g_PendingSound) {
        g_PendingSound = false;
        PlayNotificationSound(g_SelectedSound);
    }

    // Always render floating icon (independent of main window)
    RenderFloatingIcon();
    RenderUrlToast();

    if (!g_WindowVisible) return;

    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 350), ImVec2(1200, 900));
    if (!ImGui::Begin("Tyrian Instant Messenger##TyrianIM", &g_WindowVisible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::SetWindowFontScale(g_FontScale);

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
        ImGui::TextColored(g_ActiveTheme.status_ok, "%s", g_ClipboardMsg.c_str());
    } else {
        g_ClipboardMsg.clear();
        ImGui::TextColored(g_ActiveTheme.timestamp, " ");
    }

    ImGui::End();

    TyrianIM::WhisperHook::RenderDebugWindow();

}

// Options panel (shown in Nexus addon settings)
void AddonOptions() {
    ThemeGuard themeGuard;
    ImGui::Text("Tyrian Instant Messaging");
    if (ImGui::SmallButton("Homepage")) {
        ShellExecuteA(NULL, "open", "https://pie.rocks.cc/", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Buy me a coffee!")) {
        ShellExecuteA(NULL, "open", "https://ko-fi.com/pieorcake", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::Spacing();

    // --- Status ---
    auto status = TyrianIM::WhisperHook::GetStatus();
    ImVec4 sc = (status == TyrianIM::HookStatus::Hooked) ? g_ActiveTheme.status_ok
              : (status == TyrianIM::HookStatus::Failed) ? g_ActiveTheme.status_err
              : g_ActiveTheme.status_warn;
    ImGui::Text("Status:");
    ImGui::SameLine();
    ImGui::TextColored(sc, "%s", TyrianIM::WhisperHook::GetStatusString());
    ImGui::TextColored(g_ActiveTheme.timestamp, "Requires 'Events: Chat' addon from the Nexus library.");

    bool settings_changed = false;

    // --- Appearance ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Appearance");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(150);
    if (ImGui::SliderFloat("Font scale", &g_FontScale, 0.8f, 2.0f, "%.1f")) {
        settings_changed = true;
    }

    if (ImGui::Checkbox("Show message timestamps", &g_ShowTimestamps)) {
        InvalidateBubbleCache();
        settings_changed = true;
    }

    if (ImGui::Checkbox("Show quick access icon", &g_ShowQAIcon)) {
        if (g_ShowQAIcon) {
            APIDefs->QuickAccess_Add(QA_ID, TEX_ICON, TEX_ICON_HOVER, "KB_TYRIANIM_TOGGLE", "Tyrian IM");
        } else {
            APIDefs->QuickAccess_Remove(QA_ID);
        }
        settings_changed = true;
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(200);
    const char* themePreview = g_LoadedThemes.empty() ? "GW2 Dark"
                             : g_LoadedThemes[g_ActiveThemeIndex].name.c_str();
    if (ImGui::BeginCombo("Theme##ThemeSelect", themePreview)) {
        for (int i = 0; i < (int)g_LoadedThemes.size(); ++i) {
            bool sel = (i == g_ActiveThemeIndex);
            if (ImGui::Selectable(g_LoadedThemes[i].name.c_str(), sel)) {
                if (i != g_ActiveThemeIndex) {
                    g_ActiveThemeIndex = i;
                    g_ActiveThemeName  = g_LoadedThemes[i].name;
                    ApplyTheme(g_LoadedThemes[i]);
                    settings_changed = true;
                }
            }
            if (sel) ImGui::SetItemDefaultFocus();
            if (ImGui::IsItemHovered() && !g_LoadedThemes[i].description.empty()) {
                ImGui::BeginTooltip();
                if (!g_LoadedThemes[i].author.empty())
                    ImGui::TextDisabled("by %s", g_LoadedThemes[i].author.c_str());
                ImGui::TextUnformatted(g_LoadedThemes[i].description.c_str());
                ImGui::EndTooltip();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh##Themes")) {
        ScanThemes();
        ApplyNamedTheme(g_ActiveThemeName);
    }

    // --- Notifications ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Notifications");
    ImGui::Spacing();

    if (ImGui::Checkbox("Show floating icon", &g_ShowFloatingIcon)) {
        settings_changed = true;
    }
    if (g_ShowFloatingIcon) {
        ImGui::Indent();
        if (ImGui::Checkbox("Lock position", &g_FloatingIconLocked)) {
            settings_changed = true;
        }
        if (ImGui::Checkbox("Only show on message", &g_FloatingIconOnlyOnUnread)) {
            settings_changed = true;
        }
        ImGui::SetNextItemWidth(150);
        if (ImGui::SliderFloat("Size", &g_FloatingIconScale, 0.5f, 3.0f, "%.1fx")) {
            settings_changed = true;
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();
    if (ImGui::Checkbox("Show URL toast notifications", &g_UrlToastEnabled)) {
        settings_changed = true;
    }
    if (g_UrlToastEnabled) {
        ImGui::Indent();
        if (ImGui::Checkbox("Lock position##UrlToast", &g_UrlToastLocked)) {
            if (!g_UrlToastLocked) {
                std::lock_guard<std::mutex> lk(g_UrlToastMutex);
                if (g_UrlToastQueue.empty())
                    g_UrlToastShowDummy = true;
            } else {
                g_UrlToastShowDummy = false;
            }
            settings_changed = true;
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();
    if (ImGui::Checkbox("Play sound on incoming whisper", &g_SoundEnabled)) {
        settings_changed = true;
    }
    if (g_SoundEnabled) {
        ImGui::Indent();
        ImGui::PushItemWidth(200);
        const char* preview = g_SelectedSound.empty() ? "(Default)" : g_SelectedSound.c_str();
        if (ImGui::BeginCombo("Sound file##SoundSelect", preview)) {
            if (ImGui::Selectable("(Default)", g_SelectedSound.empty())) {
                g_SelectedSound.clear();
                settings_changed = true;
            }
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
        if (ImGui::SmallButton("Refresh##Sound")) {
            ScanSoundFiles();
        }
        if (ImGui::SmallButton("Test Sound")) {
            PlayNotificationSound(g_SelectedSound);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Place .wav or .mp3 files in: addons/TyrianIM/sounds/");
        ImGui::Unindent();
    }

    // --- Sending ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Sending");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(150);
    if (ImGui::SliderInt("Send delay (ms)", &g_SendDelay, 10, 200)) {
        settings_changed = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Delay between each keystroke action.\nIncrease if sends fail.");
    ImGui::Separator();
    if (ImGui::Button("Toggle Chat Debug Window"))
        TyrianIM::WhisperHook::s_ShowDebugWindow = !TyrianIM::WhisperHook::s_ShowDebugWindow;

    // --- Data ---
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Data");
    ImGui::Spacing();

    if (ImGui::Button("Clear Chat History")) {
        g_ConfirmClearHistoryOpen = true;
    }

    if (g_ConfirmClearHistoryOpen) {
        ImGui::OpenPopup("Clear Chat History?##ConfirmClear");
        g_ConfirmClearHistoryOpen = false;
    }
    if (ImGui::BeginPopupModal("Clear Chat History?##ConfirmClear", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("This will permanently delete all conversation history.");
        ImGui::Text("This cannot be undone.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Delete Everything", ImVec2(140, 0))) {
            TyrianIM::ChatManager::Shutdown();
            std::string dir = APIDefs ? std::string(APIDefs->Paths_GetAddonDirectory("TyrianIM")) : "";
            TyrianIM::ChatManager::Initialize(dir);
            g_SelectedContact.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
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
    g_ActiveTheme = BuildGW2DarkTheme();

    // Initialize data directory
    g_DataDir = std::string(APIDefs->Paths_GetAddonDirectory("TyrianIM"));

    // Load settings first (icon position, sound prefs, etc.)
    LoadSettings();
    ScanThemes();
    ApplyNamedTheme(g_ActiveThemeName);
    g_SessionStartMs = NowEpochMs();
    ScanSoundFiles();

    // Initialize subsystems
    TyrianIM::GW2API::Initialize();
    TyrianIM::ChatManager::Initialize(g_DataDir);
    TyrianIM::ChatManager::LoadHistory();
    TyrianIM::ChatManager::SetPinnedContacts(g_PinnedContactNames);

    TyrianIM::WhisperHook::Initialize(APIDefs);
    TyrianIM::WhisperHook::SetCallback(OnWhisperIntercepted);
    TyrianIM::WhisperHook::SetErrorCallback(OnWhisperError);
    TyrianIM::WhisperHook::SetNamePairCallback([](const std::string& char_name, const std::string& account_name) {
        TyrianIM::ChatManager::MergeConversation(char_name, account_name);
    });
    TyrianIM::WhisperHook::SetChannelCallback([](ChatMessageType type, ChatMetadataFlags flags,
                                                  const std::string& name, int guildIndex,
                                                  const std::string& content) {
        if (name == GetOwnCharName()) {
            g_LastChanType.store((int)type);
            g_LastChanFlags.store((int)flags);
            g_LastGuildIndex.store(guildIndex);
            return; // don't toast own messages
        }

        if (!g_UrlToastEnabled || content.empty()) return;

        auto segments = TyrianIM::ParseSegments(content);
        for (auto& seg : segments) {
            if (!seg.is_link) continue;
            if (seg.link.type != TyrianIM::GW2LinkType::Url) continue;
            UrlToast toast;
            toast.url      = seg.link.raw;
            toast.domain   = seg.link.display;
            toast.charName = name;
            toast.chanType = type;
            std::lock_guard<std::mutex> lk(g_UrlToastMutex);
            g_UrlToastQueue.push_back(std::move(toast));
        }
    });

    // Attempt to install whisper hooks (will log warning that patterns aren't defined yet)
    TyrianIM::WhisperHook::InstallHooks();

    // Cache data link pointers (avoid per-frame DataLink_Get calls)
    g_MumbleLink = (Mumble::Data*)APIDefs->DataLink_Get(DL_MUMBLE_LINK);
    g_NexusLink = (NexusLinkData_t*)APIDefs->DataLink_Get(DL_NEXUS_LINK);

    // Subscribe to squad/party chat events from Unofficial Extras (testbed)
    APIDefs->Events_Subscribe(EV_UNOFFICIAL_EXTRAS_CHAT_MESSAGE, OnChatMessage);

    // Register render callbacks
    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);

    // Register close-on-escape
    APIDefs->GUI_RegisterCloseOnEscape("Tyrian IM##TyrianIM", &g_WindowVisible);

    // Register keybind
    APIDefs->InputBinds_RegisterWithString("KB_TYRIANIM_TOGGLE", ProcessKeybind, "CTRL+SHIFT+M");

    // Load icon textures — QA icons use synchronous load so textures are ready before QuickAccess_Add
    APIDefs->Textures_GetOrCreateFromMemory(TEX_ICON,       (void*)ICON_DATA,       ICON_DATA_SIZE);
    APIDefs->Textures_GetOrCreateFromMemory(TEX_ICON_HOVER,  (void*)ICON_HOVER_DATA, ICON_HOVER_DATA_SIZE);
    APIDefs->Textures_LoadFromMemory(TEX_FLOAT_ICON,  (void*)FLOAT_ICON_DATA, FLOAT_ICON_DATA_SIZE, nullptr);

    // Load Nyan Cat animation frames (LoadFromMemory always reloads, bypassing stale cache)
    for (int i = 0; i < NYAN_FRAME_COUNT; i++) {
        char id[32];
        snprintf(id, sizeof(id), "TEX_TYRIAN_NYAN_%02d", i);
        APIDefs->Textures_LoadFromMemory(id, (void*)kNyanFrameData[i], kNyanFrameSize[i], NyanFrameCallback);
    }

    // Register quick access shortcut
    if (g_ShowQAIcon) {
        APIDefs->QuickAccess_Add(QA_ID, TEX_ICON, TEX_ICON_HOVER, "KB_TYRIANIM_TOGGLE", "Tyrian IM");
    }

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
    TyrianIM::GW2API::Shutdown();
    TyrianIM::WhisperHook::Shutdown();
    TyrianIM::ChatManager::Shutdown();
    g_MessageArrivalTimes.clear();

    APIDefs->Log(LOGL_INFO, "TyrianIM", "Tyrian Instant Messaging unloaded");
    APIDefs = nullptr;
}

void ProcessKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;

    if (strcmp(aIdentifier, "KB_TYRIANIM_TOGGLE") == 0) {
        g_WindowVisible = !g_WindowVisible;
    }
}

// Export function
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature = 0x584b6b2f;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Tyrian Instant Messenger";
    AddonDef.Version.Major = V_MAJOR;
    AddonDef.Version.Minor = V_MINOR;
    AddonDef.Version.Build = V_BUILD;
    AddonDef.Version.Revision = V_REVISION;
    AddonDef.Author = "PieOrCake.7635";
    AddonDef.Description = "Instant messaging window for Guild Wars 2 whispers";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/PieOrCake/tim";

    return &AddonDef;
}
