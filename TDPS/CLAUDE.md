# CLAUDE.md

## Language

Always respond in Chinese (中文).

## Project state

The firmware startup path currently uses `LF_Config_ApplyDebugProfile()` by default. Use the low-speed conservative debug profile unless the user explicitly asks to switch to `LF_Config_ApplyCompetitionProfile()`.
