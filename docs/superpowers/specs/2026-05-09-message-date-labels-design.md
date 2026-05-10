# Message Date Labels Design

**Date:** 2026-05-09

## Problem

Message bubbles currently show only `HH:MM` in the timestamp area. For messages from previous days, the time alone gives no context — the user cannot tell if a message is from yesterday or last week.

## Goal

Messages from days before today show the full date and time in the bubble timestamp area:

- **Today:** `21:30`
- **Previous days:** `Monday, 13 Mar 26 @ 21:30`

## Approach

Extend the bubble layout cache (`BubbleLayout`) with a `displayTimestamp` string computed at layout time from `epoch_ms`. The layout cache already handles invalidation (conversation switch, resize, font scale change, new messages). The raw `msg.timestamp` and TSV history format are untouched.

## Changes

### `FormatDisplayTime(epoch_ms)` (new function, `dllmain.cpp`)

Returns a display string based on whether the message date matches today (local time):

- Same calendar day → `"HH:MM"` (unchanged)
- Any earlier day → `"Weekday, DD Mon YY @ HH:MM"` (e.g. `"Monday, 13 Mar 26 @ 21:30"`)

Uses `localtime_s` (already in use by `FormatTime`). "Today" is evaluated at layout-computation time — if the layout is not invalidated at midnight the string will update on the next natural invalidation (conversation switch, resize, etc.). This is acceptable.

### `BubbleLayout` struct

Add `std::string displayTimestamp`.

### `ComputeBubbleLayout`

- Call `FormatDisplayTime(msg.epoch_ms)` and store in `layout.displayTimestamp`.
- Compute `layout.timeSize` from `layout.displayTimestamp` (not `msg.timestamp`).
- `layout.bubbleW` calculation already uses `layout.timeSize`, so wider date strings automatically expand the bubble.

### Render loop

Draw `layout.displayTimestamp` in place of `msg.timestamp`. No other render changes.

## Data integrity

`msg.timestamp`, `msg.epoch_ms`, and the TSV history file format are unchanged. `epoch_ms` is confirmed present for all messages including history loaded from disk.
