# CLAUDE.md

## Language

Always respond in Chinese (中文).

## Project state

The firmware startup path currently uses `LF_Config_ApplyTrackProfile()`, not the low-speed debug profile. Track profile enables straight boost, curve preparation slowdown, line-stability filtering, stable direction recovery, and fork detection by default.
