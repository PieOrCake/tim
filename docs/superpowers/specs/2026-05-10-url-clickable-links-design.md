# URL Clickable Links — Design Spec

**Date:** 2026-05-10  
**Status:** Approved

## Summary

When a chat message contains an `http://` or `https://` URL, it is rendered as a clickable inline segment showing only the domain name. Clicking opens the URL in the system browser. Hovering shows the full URL as a tooltip.

## Approach

Extend the existing `GW2LinkType` enum with a `Url` value. URL segments reuse the `Segment` / `LinkSegment` structs and the `Pending → Resolved` rendering pipeline without modification to layout, ChatManager, GW2API, WhisperHook, settings, or history format.

## Parsing (`ChatLinks.cpp` — `ParseSegments`)

- Scan the message text in the existing `while` loop for `http://` or `https://` prefixes.
- Consume characters until whitespace or end of string to form the full URL.
- Extract domain: substring between `://` and the next `/` (or end of URL).
- Create a `Segment`:
  - `is_link = true`
  - `link.type = GW2LinkType::Url`
  - `link.raw` = full URL
  - `link.display` = domain only (e.g. `wiki.guildwars2.com`)
  - `link.tooltip_text` = full URL
  - `link.state = LinkState::Resolved`
  - `link.colour = IM_COL32(100, 160, 255, 255)` (hyperlink blue)
- URL detection runs before the plain-text fallback, consistent with GW2 link detection order.

## Layout (`dllmain.cpp` — `ComputeBubbleLayout`)

No changes. URL segments have `is_link = true` and a `display` string (the domain), so they flow inline identically to GW2 link segments. Sizing uses `display` text width.

## Rendering & Interaction (`dllmain.cpp` — render loop)

The existing per-segment invisible button loop handles hover and click. One new branch in the click handler:

- If `seg.link.type == GW2LinkType::Url`: call `ShellExecuteA(NULL, "open", seg.link.raw.c_str(), NULL, NULL, SW_SHOWNORMAL)`.
- Tooltip on hover: render `tooltip_text` (the full URL) via the existing tooltip path.
- Domain text drawn in `link.colour` (hyperlink blue).

`ShellExecuteA` is already used in the codebase for the website and donation buttons.

## Out of Scope

- Page title fetching (async HTTP lookup) — domain-only display is sufficient.
- Right-click context menu (copy URL) — not required for initial implementation.
- URL shortener expansion — not required.
