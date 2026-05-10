#pragma once
#include "ChatLinks.h"
#include <string>
#include <cstddef>

// Called from GW2API worker thread after any link resolves.
// Implementation is in dllmain.cpp; sets g_LinksDirty = true.
extern void TIM_NotifyLinkResolved();

namespace TyrianIM {

class GW2API {
public:
    static void Initialize();
    static void Shutdown();

    // Queue an API fetch for one link segment.
    // contact/msg_idx/seg_idx identify where to write the result.
    // No-op if the type has no API endpoint (MapLink, BuildTemplate, AE2Build)
    // or if an identical request is already queued/cached.
    static void RequestLink(const std::string& contact, size_t msg_idx, size_t seg_idx,
                             const LinkSegment& link);
};

} // namespace TyrianIM
