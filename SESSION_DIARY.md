# Session Diary

## Session: 2026-04-21

### Agenda
- Integrazione OctoMap per la generazione di Occupancy Grid 2D da FAST-LIO2
- Aggiornamento Dockerfile con dipendenze OctoMap
- Rebuild immagine Docker ARM64 per il robot
- Configurazione Nav2 per utilizzare la mappa proiettata di OctoMap

### Progress
- **OctoMap Integration**: Creato `src/go2_nav_bridge/launch/octomap.launch.py` per configurare il server OctoMap (risoluzione 5cm, range 5m).
- **Launch Files**: Aggiornato `bringup.launch.py` per includere l'avvio di OctoMap.
- **Dockerfile**: Aggiornate le sezioni `builder` e `runtime` con `ros-humble-octomap-server` e `ros-humble-octomap-ros`.
- **Package Config**: Aggiunte dipendenze OctoMap in `src/go2_nav_bridge/package.xml`.
- **Nav2 Config**: Aggiornato `nav2_params.yaml` per includere un `static_layer` nel `global_costmap` che sottoscrive `/projected_map`.
- **Deployment Strategy**: Interrotta build emulata su laptop. Si procederà con build nativa sull'Orin tramite `docker compose up --build -d` dopo il sync dei sorgenti, per massimizzare la velocità e affidabilità del build.

### Status of Key Components
| Component | Status | Notes |
|---|---|---|
| Docker ARM64 | Ready to Build | Native build on robot chosen as primary path |
| OctoMap | Done | Configured and integrated in launch/params |
| Nav2 | Done | Updated to use OctoMap projected grid |

### Open Items / Next Steps
- [ ] Eseguire `./sync_to_dog.sh` per inviare sorgenti e Dockerfile al robot
- [ ] Testare l'intero stack sul robot: `cd docker && docker compose up --build -d`
- [ ] Verificare la generazione della mappa 2D in RViz2 tramite `/projected_map`

---

## Session: 2026-04-19

### Agenda
- Allineamento ai mandati di progetto: attivazione e configurazione di `graphify` e `mempalace`
- Risoluzione discrepanze TF statico tra `GEMINI.md` e `CLAUDE.md`
- Verifica e finalizzazione configurazione Docker (Dockerfile, Compose, CycloneDDS)
- Analisi architettura tramite Graphify Wiki

### Progress
- **MemPalace**: Inizializzato e completato il mining del progetto (`mempalace init . && mempalace mine .`). Creata memoria semantica locale per persistenza decisionale.
- **Graphify**: Aggiornato grafo AST (7824 nodi). Wiki generato con successo per navigazione strutturata. Configurato per operare in modalità `--no-viz` causa dimensioni elevate.
- **TF Statico**: Verificato in `src/go2_nav_bridge/launch/bringup.launch.py`. Task confermato come COMPLETATO (extrinsics T=[0.171, 0, 0.0908]).
- **Docker**: Verificato `docker/Dockerfile` e `docker/docker-compose.yml`. Le dipendenze PCL (`libpcl-dev`, `ros-humble-pcl-ros`) e CycloneDDS (`ros-humble-rmw-cyclonedds-cpp`) sono già state integrate. TTY e volumi per `cyclonedds.xml` configurati correttamente.
- **Status Sync**: Sincronizzati i file di contesto `GEMINI.md` e `SESSION_DIARY.md` con lo stato reale del progetto.

### Status of Key Components
| Component | Status | Notes |
|---|---|---|
| MemPalace | Active | Local semantic memory indexed (167 files) |
| Graphify | Active | Wiki ready in `graphify-out/wiki/` |
| `bringup.launch.py` | Done | Static TF, Hesai, FAST-LIO2, Nav2, Bridge integrated |
| Dockerfile | Done | Build and Runtime stages updated with all dependencies |
| CycloneDDS | Done | XML mounted and environment variables set |

### Open Items / Next Steps
- [ ] Rebuild immagine Docker ARM64: `docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load docker/`
- [ ] Trasferire l'immagine sul robot e verificare il deployment
- [ ] Testare l'intero stack `bringup.launch.py` su hardware reale
- [ ] Investigare eventuali errori residui nei plugin di Claude Code

---

## Session: 2026-04-04

### Agenda
- Ricerca documentazione ROS 2 Humble e Gemini CLI via Context7
- Esplorazione delle capacità plugin di Claude Code nell'estensione VS Code
- Installazione e configurazione di nuovi plugin Claude Code

### Progress
- Consultata documentazione ROS 2 Humble via Context7 (library ID: `/ros2/ros2_documentation`) — recuperati aggiornamenti Humble Hawksbill: lifecycle nodes in rclpy, QoS overrides CLI, composable nodes frontend, launch_pytest
- Consultata documentazione Gemini CLI via Context7 (library ID: `/google-gemini/gemini-cli`) — confermato supporto MCP server via `settings.json` con stessa sintassi di Claude Code
- Chiarito che i plugin/skill di Claude Code sono accessibili con la stessa interfaccia `/plugin:skill` sia nel terminale CLI che nell'estensione VS Code
- Installati due nuovi plugin: `documentation-generator` e `experienced-engineer`
- Ricaricato sistema plugin: 11 plugins, 14 skills, 25 agents; 2 errori di caricamento da investigare

### Status of Key Components
| Component | Status | Notes |
|---|---|---|
| CLAUDE.md / GEMINI.md | In Sync | Nessuna modifica necessaria ai contenuti |
| Context7 MCP server | Funzionante | Verificato per ROS 2 e Gemini CLI docs |
| Claude Code plugins | Aggiornati | `documentation-generator` + `experienced-engineer` installati; 2 errori pending |
| `go2_nav_bridge` | In Progress | Codice completato (sessione precedente), test su robot in attesa |
| Docker ARM64 image | Blocked | Rebuild e trasferimento su Orin ancora da fare |

### Open Items / Next Steps
- [ ] Investigare i 2 errori di caricamento plugin (identificare quale plugin e causa)
- [ ] Rebuild immagine Docker: `docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .`
- [ ] Trasferire la nuova immagine Docker sul robot e verificare il build su Orin
- [ ] Testare `go2_nav_bridge` sul robot fisico
- [ ] Pubblicare static TF `base_link → hesai_lidar` nel launch file del bridge

---

## Session: 2026-04-02

### Agenda
- Create a Claude `docs-writer` skill for professional README/documentation writing
- Rewrite `README.md` to meet professional open-source standards
- Sync CLAUDE.md and GEMINI.md (integrate Dev Containers workflow differences)
- Set up agent memory system for session-closure-sync

### Progress
- Created `.claude/skills/docs-writer/SKILL.md` — based on the existing Gemini `docs-writer` skill, improved with analysis of real documentation projects (ETH Zurich `ros_package_template`, `ros2_control`, `articubot_one`)
- Rewrote `README.md` from scratch:
  - Added requirements table (ROS 2 version, platform, container, LiDAR, IMU)
  - Added ROS Interfaces table (`/lidar_points`, `/Odometry`, `/cmd_vel`, `/sportmodestate`, TF)
  - Added BibTeX citation block and lead researcher footer
  - Added architecture narrative paragraph before the ASCII diagram
  - Added Deployment section with `buildx` prerequisite link and `docker compose` post-deploy step
  - Added Known Issues table (container exit, livox build, PCL, CycloneDDS)
  - All submodule links point to upstream repositories with short descriptions
- Fixed README.md affiliation: corrected from "Politecnico di Milano Master's Thesis" to "UNIMORE Bachelor's Thesis in Mechatronics Engineering"
- Removed redundant Launch section from README (docker compose handles node startup automatically)
- Integrated GEMINI.md differences into CLAUDE.md: VS Code Dev Containers as primary workflow, post-deploy `docker compose up -d` step
- Fixed `ln -s` path in README Setup section, added `buildx` prerequisite link, removed double separator
- Created agent memory: `user_profile.md` (UNIMORE triennale meccatronica) and `MEMORY.md` index

### Status of Key Components
| Component | Status | Notes |
|---|---|---|
| `README.md` | Done | Full rewrite with requirements, interfaces, architecture narrative, citation, known issues |
| `.claude/skills/docs-writer/SKILL.md` | Done | New skill for documentation writing |
| `.gemini/skills/docs-writer/SKILL.md` | Done | Already existed; Claude version created from it |
| CLAUDE.md / GEMINI.md | In Sync | Dev Containers workflow + post-deploy compose step integrated into both |
| `go2_nav_bridge` | In Progress | Bridge node implemented (prev session), awaiting on-robot testing |
| Dockerfile PCL deps | Blocked | Next priority — `libpcl-dev` + `ros-humble-pcl-ros` |

### Open Items / Next Steps
- [ ] Update `docker/Dockerfile` to include `libpcl-dev` and `ros-humble-pcl-ros`
- [ ] Symlink `src/livox_ros_driver2/package_ROS2.xml` to `package.xml`
- [ ] Rebuild ARM64 Docker image and transfer to robot
- [ ] Test `go2_nav_bridge` on physical robot
- [ ] Create new Gemini skills: `go2-hardware-expert`, `go2-deployment-pro` (already created for Claude)

---

## Session: 2026-04-01

### Agenda
- Begin working on Docker build fixes (Dockerfile dependencies, livox package.xml symlink)
- Continue development toward end-to-end navigation baseline

### Progress
- Diagnosed Docker daemon not running on the development laptop (not the robot)
  - `docker.service` and `docker.socket` both `inactive (dead)` -- socket activation was disabled
  - No crash or error -- simply never started in this session
  - Resolution: `sudo systemctl start docker`, then verify with `journalctl -u docker.service`
- Session was very brief; no source code or configuration changes were made
- CLAUDE.md and GEMINI.md confirmed already in sync (no updates needed)

### Status of Key Components
| Component | Status | Notes |
|---|---|---|
| Docker (laptop) | Resolved | Daemon was inactive; fix is `systemctl start docker` |
| Docker (robot/Orin) | Not tested | Pending laptop Docker fix to rebuild ARM64 image |
| `go2_nav_bridge` | In Progress | cmd_vel -> SportModeCmd implemented (prev session), needs on-robot testing |
| FAST-LIO2 XT16 config | Done | `src/fast_lio_ros2/config/hesai_xt16.yaml` created 2026-03-31 |
| Dockerfile PCL deps | Blocked | Cannot rebuild until Docker daemon is running; next priority |
| CLAUDE.md / GEMINI.md | In Sync | No content changes needed this session |

### Open Items / Next Steps
- [ ] Start Docker daemon on laptop: `sudo systemctl start docker`
- [ ] Update `docker/Dockerfile` to include `libpcl-dev` and `ros-humble-pcl-ros`
- [ ] Symlink `src/livox_ros_driver2/package_ROS2.xml` to `package.xml`
- [ ] Rebuild ARM64 Docker image and transfer to robot
- [ ] Test `go2_nav_bridge` on physical robot

---

## Session: 2026-03-31 (retroactive)

### Agenda
- Sync CLAUDE.md with GEMINI.md (master content)
- Update TODO/Roadmap tags
- Add VS Code and clangd configuration for laptop synchronization

### Progress
- Synchronized CLAUDE.md with GEMINI.md and updated TODO/Roadmap tags (commits `ead7b8a`, `085981f`)
- Added VS Code `settings.json` and `.clangd` configuration for laptop development (commit `8f7ea05`)
- Parameterized velocity limits in `go2_nav_bridge` and improved safety watchdog (commit `c0a4f72`)
- Implemented `cmd_vel` to `SportModeCmd` bridge with watchdog (commit `026c1c6`)

### Status of Key Components
| Component | Status | Notes |
|---|---|---|
| `go2_nav_bridge` | In Progress | Bridge node implemented with parameterized limits and watchdog; not yet tested on robot |
| CLAUDE.md / GEMINI.md | Done | Synchronized |
| VS Code / clangd config | Done | `.clangd` and `.vscode/settings.json` added |

### Open Items / Next Steps
- [ ] Fix Docker build (PCL dependencies, livox package.xml)
- [ ] Test bridge node on physical robot

---

## Session: 2026-03-28

### Agenda
- Investigate Gemini CLI skill files and understand how `GEMINI.md` functions as project context
- Synchronize `CLAUDE.md` and `GEMINI.md` so both AI assistants share the same project knowledge
- Update the `session-closure-sync` agent with a GitHub push step

### Progress
- Confirmed that `GEMINI.md` at repo root is Gemini CLI's project context file (no separate skill files needed for project-level instructions)
- Identified content gaps in `CLAUDE.md`: missing alternative networking strategies, development conventions, FAST-LIO2 reference, external resources, thesis analysis section, and TODO/Roadmap
- Ported all missing sections from `GEMINI.md` into `CLAUDE.md`, translating Italian content to English where needed
- Synchronized `GEMINI.md` back to match `CLAUDE.md` canonical content (both files now identical in substance, differing only in the first-line header)
- Updated `~/.claude/agents/session-closure-sync.md` with Step 6: commit and push to GitHub at session end

### Status of Key Components
| Component | Status | Notes |
|---|---|---|
| `CLAUDE.md` | Done | Now contains all sections from both files, fully in English |
| `GEMINI.md` | Done | Synchronized to match `CLAUDE.md` content |
| `go2_nav_bridge` | Not started | Still a stub; `Twist -> SportModeCmd` not implemented |
| FAST-LIO2 XT16 config | Not started | Needs custom YAML with extrinsics and IMU noise params |
| Wireless telemetry | Blocked | Pending Wi-Fi dongle hardware acquisition (Alfa AWUS036ACM) |
| Docker ARM64 image | Not started | Final deployment step, after navigation stack is working |

### Open Items / Next Steps
- [ ] Implement `go2_nav_bridge` node: subscribe to `/cmd_vel`, publish `SportModeCmd`
- [ ] Create FAST-LIO2 config YAML for Hesai XT16 (adapt from existing config in `src/fast_lio_ros2/config/`)
- [ ] Analyze previous student's thesis for calibration data and networking solutions
- [ ] Acquire and test Alfa AWUS036ACM Wi-Fi dongle on Expansion Dock
- [ ] Verify Gemini CLI picks up the updated `GEMINI.md` correctly

---
