# Documentation Index

This file provides a guide to all project documentation.

## Primary Documentation

### CLAUDE.md (6.3 KB)
**Location**: `/CLAUDE.md`
**Purpose**: Consolidated project documentation for Claude Code agents
**Contains**:
- Hardware setup and I2C device map
- Project status and completion tracking
- Development workflow and build commands
- Architecture (state machine, QR metadata, data formats)
- Critical gotchas (SD card, ISR constraints, timing)
- Key learnings from all completed tasks
- File locations and project phasing

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
**Location**: `/plans/archive/completed/` (160 KB total)
**Contains**: PRDs and design briefs for completed Linear tasks
- M3L-57: State machine architecture
- M3L-58: Button interrupt handler
- M3L-60: QR code scanner integration

**Note**: All critical learnings from these archived docs have been extracted and consolidated into CLAUDE.md.

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
| CLAUDE.md | 6.3 KB | ✅ Under 10KB target |
| .serena/memories/hardware_architecture.md | 1.5 KB | ✅ Consolidated |
| PRD.md | 9.9 KB | ℹ️ High-level spec |
| init_prompt.md | 1.9 KB | ℹ️ Historical context |
| plans/archive/completed/ | 160 KB | 📦 Archived |

## Documentation Update History

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

**Keep CLAUDE.md under 10KB** by:
- Using concise bullet points over prose
- Archiving completed task details
- Focusing on "what developers need to know now"
- Linking to archived docs for historical details
