# Documentation Index

This file provides a guide to all project documentation.

## Primary Documentation

### CLAUDE.md (11.4 KB)
**Location**: `/CLAUDE.md`
**Purpose**: Consolidated project documentation for Claude Code agents
**Last Updated**: 2025-11-14
**Contains**:
- Hardware setup and I2C device map
- Project status and completion tracking (NOW/NEXT/LATER phasing)
- Development workflow and build commands
- Architecture (state machine, sensor_manager, storage_manager)
- Data formats (QR with test_id, CSV tabular, metadata.json)
- Critical gotchas (SD card, ISR constraints, IMU timing, path sanitization)
- Key learnings from all completed tasks (M3L-57 through M3L-64)
- File locations organized by category
- Common tasks (QR generation with venv, sampling rate changes)

**This is the single source of truth for active development.**

## Supporting Documentation

### Product Requirements (PRD.md)
**Location**: `/PRD.md` (9.9 KB)
**Purpose**: High-level product vision and business requirements
**Contains**:
- User personas and jobs-to-be-done
- Business goals and success metrics
- Feature roadmap (NOW/NEXT/LATER)

### Original Specification (init_prompt.md)
**Location**: `/init_prompt.md` (1.9 KB)
**Purpose**: Original user requirements that started the project
**Contains**:
- Hardware list
- Initial workflow requirements
- Feature wish list

### Hardware Quick Reference
**Location**: `/.serena/memories/hardware_architecture.md` (1.5 KB)
**Purpose**: Critical hardware gotchas for serena-mcp agent
**Contains**:
- SD card level shifter critical setup
- I2C device addresses
- Pin assignments
- References to full docs in CLAUDE.md

## Archived Documentation

### Completed Task Documentation
**Location**: `/plans/archive/` (160 KB total)
**Contains**: PRDs and design briefs for completed Linear tasks
- M3L-57: State machine architecture
- M3L-58: Button interrupt handler
- M3L-60: QR code scanner integration

### Active Task Documentation
**Location**: `/plans/` directory
**Contains**: Documentation for in-progress tasks
- `prd-data-logging-core.md`: PRD for M3L-61/63/64 (IMU, storage, session management)
- `debugging-imu-zeros.md`: Active debugging plan for IMU zero values issue
- `manual-testing.md`: Manual test procedures

**Note**: All critical learnings from completed docs are extracted and consolidated into CLAUDE.md.

## Tests Documentation

### Test Plans and Reports
**Location**: `/tests/` directory
**Contains**:
- Manual test procedures
- Integration test plans
- Test reports with results
- Quick start guides for testing

See `/tests/README.md` for complete testing documentation index.

## File Size Summary

| File | Size | Status |
|------|------|--------|
| CLAUDE.md | 11.4 KB | ⚠️ Active development (consolidate when M3L-62 completes) |
| .serena/memories/hardware_architecture.md | 1.5 KB | ✅ Consolidated |
| PRD.md | 9.9 KB | ℹ️ High-level spec |
| init_prompt.md | 1.9 KB | ℹ️ Historical context |
| plans/archive/ | 160 KB | 📦 Archived |
| plans/debugging-imu-zeros.md | 8.9 KB | 🔧 Active debugging |
| plans/prd-data-logging-core.md | 7.8 KB | 📋 Active PRD |

## Documentation Update History

**2025-11-14**: M3L-61/63/64 Core Logging Consolidation
- Updated CLAUDE.md from 6.3KB to 11.4KB (+5.1KB)
- Added comprehensive M3L-61/63/64 Key Learnings section
- Updated data formats (QR with test_id, CSV tabular, metadata.json)
- Added Critical Gotchas #7 (IMU timing) and #8 (path sanitization)
- Reorganized File Locations into categories
- Updated Architecture section with sensor_manager/storage_manager
- Expanded Common Tasks with QR generator venv setup
- Updated Project Status to NOW phase (M3L-61/63/64 in progress)

**2025-11-12**: Major consolidation
- Reduced CLAUDE.md from 45KB to 6.3KB (86% reduction)
- Archived 160KB of completed task documentation
- Updated serena memory to reference CLAUDE.md
- Established archive structure for completed work

## Best Practices

1. **For active development**: Reference CLAUDE.md only
2. **For historical context**: Check init_prompt.md and PRD.md
3. **For task-specific details**: See archived PRDs in `/plans/archive/completed/`
4. **For testing procedures**: See `/tests/` directory
5. **When adding new learnings**: Append to "Key Learnings" in CLAUDE.md

## Maintenance

**When to update CLAUDE.md**:
- After completing a major task (add to Key Learnings)
- When discovering critical hardware/software gotchas
- When project status or phasing changes
- When file structure changes significantly

**When to archive documentation**:
- After a Linear task is complete and deployed
- Move PRDs and design briefs to `/plans/archive/completed/`
- Extract key learnings to CLAUDE.md before archiving

**Keep CLAUDE.md manageable** by:
- Using concise bullet points over prose
- Archiving completed task details after deployment
- Focusing on "what developers need to know now"
- Linking to archived docs for historical details
- Consolidating similar learnings (avoid duplication)
- Moving debugging plans to /plans/ when resolved

**Note**: CLAUDE.md may temporarily exceed 10KB during active development phases with multiple in-progress features. Consolidate when tasks complete.
