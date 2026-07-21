# Wiki Schema — Go2 Autonomous Navigation

## Purpose
This is the LLM behavior specification for the Go2 wiki. It governs how sources are ingested, how pages are created and maintained, and how queries are answered. Read this before any wiki operation.

---

## Directory Layout

```
docs/wiki/
├── raw/          # Immutable source documents — LLM reads, never writes
├── pages/        # LLM-owned wiki (Obsidian vault root)
│   ├── _index.md         # Master index — always update on ingest
│   ├── _log.md           # Chronological ingest log — always append on ingest
│   ├── overview.md       # System architecture overview
│   ├── entities/         # Hardware and software components
│   ├── concepts/         # Technical concepts, algorithms, protocols
│   └── configs/          # Configuration reference pages
└── SCHEMA.md     # This file — LLM instructions
```

---

## Page Types

| Type | Location | Purpose |
|------|----------|---------|
| `entity` | `entities/` | A discrete hw/sw component (sensor, node, robot) |
| `concept` | `concepts/` | Algorithm, protocol, coordinate frame, mathematical concept |
| `config` | `configs/` | Configuration reference for a specific system |
| `summary` | any | Summary of an ingested source document |
| `analysis` | any | Answer to a query that is worth preserving |

---

## Frontmatter Template

Every page must begin with:

```yaml
---
type: entity|concept|config|summary|analysis
tags: [tag1, tag2]
aliases: [alternative name]
sources: [filename from raw/ if applicable]
updated: YYYY-MM-DD
---
```

---

## Naming Conventions

- Filenames: `kebab-case.md`
- Internal links: Obsidian wikilinks `[[page-name]]` (shortest unique name, no path prefix)
- Always use aliases for long names: alias `FAST-LIO2` → file `fast-lio2.md`
- Tags: use existing tags before creating new ones. Core tags: `hardware`, `software`, `slam`, `navigation`, `ros2`, `config`, `sensor`, `imu`

---

## Operation: Ingest

When the user drops a file in `raw/` and says "ingest [filename]":

1. Read the source file
2. Discuss key takeaways with the user (2-3 bullet points)
3. Write a summary page in `pages/` (usually `entities/` or `concepts/`)
4. Update `_index.md` — add entry under correct section
5. Update all related existing pages (add cross-references, update facts)
6. Append entry to `_log.md`:
   ```
   - YYYY-MM-DD: [source filename] → [pages created/updated] — [one-line summary]
   ```

A single source may touch 5–15 pages. Update all that are relevant.

---

## Operation: Query

When the user asks a question about the project:

1. Search `pages/` for relevant pages (read `_index.md` first for orientation)
2. Read the relevant pages
3. Synthesize answer with `[[wikilink]]` citations
4. If the answer is non-trivial and reusable, ask the user: "Vale la pena salvare questa analisi come pagina wiki?"
5. If yes, write it as a new `analysis` page

---

## Operation: Lint

When the user says "lint wiki":

1. Read all pages in `pages/`
2. Check for:
   - Contradictions between pages (flag with `> ⚠️ Contradiction:`)
   - Stale claims superseded by newer sources
   - Orphan pages (no inbound links)
   - Missing cross-references (entity mentioned but not linked)
   - TODOs or placeholder sections
3. Report findings as a checklist
4. Fix issues with user approval

---

## Cross-Reference Rules

- Always link entity names on first mention per page: `[[fast-lio2]]`, `[[hesai-xt16]]`
- Configs page must link back to the entity that uses it
- Concepts pages must link to entities that implement them
- Never duplicate facts across pages — keep canonical fact on one page and cross-link

---

## Update Policy

- When a fact changes (e.g. calibration updated), update the canonical page + all pages that reference it
- Add a changelog comment at the bottom of updated pages: `<!-- updated YYYY-MM-DD: reason -->`
- Outdated facts: strike through `~~old value~~` + note new value + source

---

## Scope

This wiki covers: Unitree Go2 robot, Hesai XT-16 LiDAR, FAST-LIO2 SLAM, Nav2 navigation, go2_nav_bridge control, ROS 2 Humble, Docker deployment, DDS configuration, IMU calibration, extrinsic calibration.

Out of scope: general ROS 2 tutorials, Unitree SDK internals not directly used.

---

## Content Strategy — Wiki vs NotebookLM

This wiki and NotebookLM serve different roles. Do not conflate them.

### What belongs in this wiki

Project-specific knowledge that **evolves with the codebase**, has **interdependencies**, and needs to be **consulted inline** by the LLM without RAG:

| Category | Examples |
|----------|---------|
| Architectural decisions | Why FAST-LIO2 over SLAM Toolbox, why MPPI |
| TF tree & frame conventions | `map → odom → base_link → hesai_lidar` rationale |
| Parameter rationale | Extrinsics T=[0.171, 0, 0.0908], IMU noise covariance values and why |
| Operational procedures | `sync_to_dog.sh` flow, LiDAR-IMU calibration steps |
| Known bugs & workarounds | PTP timestamp truncation fix, IMU starvation resolution |
| Component interfaces | `go2_nav_bridge` ↔ `SportModeCmd` contract, topic/QoS conventions |
| Lab findings | Test results, measured performance, calibration sessions |

### What belongs in NotebookLM

External, long, unstructured documents that do not change with the project:

| Category | Examples |
|----------|---------|
| Vendor datasheets | Hesai XT-16 manual, Unitree Go2 hardware guide |
| Academic papers | FAST-LIO2 paper, LIO-SAM, MPPI original publications |
| Framework documentation | Nav2 docs, ROS 2 REP-105/REP-155 |
| Historical debug sessions | rosbag analysis logs, long diagnostic transcripts |

### Decision rule

> **Wiki** = "what the project knows and why it decided so"
> **NotebookLM** = "what external documents say"

If a future collaborator needs to understand a design decision → wiki.
If they need to read the sensor datasheet → NotebookLM.

When ingesting a new source, apply this rule before creating any page. If the source is a vendor document or academic paper with no project-specific annotation, redirect to NotebookLM rather than ingesting here.
