# Event Journal

The Event Journal records important network events into a database file while Hive is running, so they can be analyzed after the fact (typically after an audio show where Hive was left running in the background). It is aimed at the sound engineer: only user-relevant events are recorded (entities going online/offline, stream connections, error counters, media clock lock, gPTP changes, ...), not internal debugging information (which belongs to the Logger window).

## Architecture

### Recording module

`hive::modelsLibrary::EventJournal` (in `libs/modelsLibrary`) is a singleton that listens to `ControllerManager` signals and normalizes them into journal events. It is UI-independent.

- A recording session automatically starts when the controller goes online (`controllerOnline` signal) and stops when it goes offline. Each session creates a new timestamped file in `<AppDataLocation>/EventJournals/` (eg. `~/Library/Application Support/KikiSoft/Hive/EventJournals/` on macOS).
- The oldest session files are automatically removed, keeping at most 50 files.
- The application attaches context metadata to the session (network interface) right after creating the controller.

### Storage: SQLite (via Qt Sql)

Events are persisted in an SQLite database (using the `QSQLITE` driver shipped with Qt, so no additional third party dependency).

Reasons for this choice (over a timeseries database like InfluxDB, or a flat JSON/CSV file):

- **Single self-contained file**: exporting is a file copy, importing is opening a file. No server to run.
- **Crash safety**: the database is written in WAL mode with `synchronous=NORMAL`; if Hive or the computer crashes during a show, all events written so far are recoverable (this is the primary use case of the feature).
- **Tooling**: journal files can also be inspected with any SQLite tool (`sqlite3`, DB Browser for SQLite, Python, ...) for advanced analysis.

File extension: `.hej` (Hive Event Journal). Writing details:

- While recording, the database is in WAL mode (crash-safe, low latency writes on the GUI thread, where all `ControllerManager` signals are emitted).
- On clean session stop, the file is converted back to `journal_mode=DELETE` so it becomes a single file (no `-wal`/`-shm` companions) that can be opened read-only from anywhere.
- Live export uses `VACUUM INTO`, which produces a consistent, self-contained snapshot without interrupting the recording.
- `EventJournal::loadSession()` opens files read-only, and falls back to read-write open (which performs WAL recovery) for sessions that were interrupted by a crash.

### Schema (version 1)

```sql
CREATE TABLE metadata (key TEXT PRIMARY KEY NOT NULL, value TEXT);
CREATE TABLE entities (entity_id TEXT PRIMARY KEY NOT NULL, name TEXT);
CREATE TABLE events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL, -- milliseconds since Unix epoch, UTC
    category TEXT NOT NULL,     -- session, entity, connection, counters, media_clock, gptp, link, latency, redundancy
    severity TEXT NOT NULL,     -- info, warning, error, recovered
    entity_id TEXT,             -- hex EID, empty for session-wide events
    entity_name TEXT,           -- entity name at the time of the event (denormalized, so the file is self-contained)
    subject TEXT,               -- sub-object designation (stream, interface, clock domain, ...)
    summary TEXT NOT NULL,      -- human readable description
    details TEXT                -- additional details, JSON object
);
```

Metadata keys: `schema_version`, `hive_version`, `computer_name`, `started_utc`, `stopped_utc`, `interface_id`, `interface_name`.

### Recorded events

| Category | Source signals | Severity |
|---|---|---|
| Session | `controllerOnline` / `controllerOffline` / `transportError` | Info / Error |
| Entity | `entityOnline` / `entityOffline` | Info / Warning |
| Connection | `streamInputConnectionChanged` (listener side) | Info |
| Counters | `streamInputErrorCounterChanged`, `statisticsErrorCounterChanged` (the counters Hive flags as errors) | Error |
| Media Clock | `clockDomainCountersChanged` (Locked/Unlocked transitions, same rule as the Discovered Entities list) | Error / Recovered |
| gPTP | `gptpChanged` (grandmaster or domain change) | Warning |
| Link | `avbInterfaceLinkStatusChanged` | Error (down) / Recovered (up) |
| Latency | `streamInputLatencyErrorChanged` | Error / Recovered |
| Redundancy | `redundancyWarningChanged` | Error / Recovered |

The `Recovered` severity marks the end of a previously reported error condition, so the duration of an incident (clock unlock, link down, ...) is directly visible in the journal.

To only record actual changes (and not the initial state of each entity being enumerated), the module seeds change-detection baselines from the entity model when an entity comes online (`EventJournal::Private::seedBaselines`), and error counters are compared against their previously recorded values.

## Viewer UI

`EventJournalView` (in `src/eventJournal/`) displays a session with filtering, in two modes (same pattern as the Entity Model Inspector):

- **Live**: embedded in the `MainWindow` "Event Journal" dock (View menu, hidden by default), following the current recording session, with auto-scroll and an Export button.
- **File**: `File > Open Event Journal...` opens any journal file in a standalone viewer window (multiple windows can be opened, eg. to compare with the live session).

Features: severity/category/entity filter menus, free text search (regular expression), time range filter, per-severity row colors (error/warning/recovered), details pane for the selected event, event counters. `EventJournalModel` + `EventJournalFilterProxyModel` (in the same directory) implement the table and filtering.

A timeline strip (`EventJournalTimeline`) is displayed above the table, plotting the filtered events on a time axis with one lane per category, so recurring problems (eg. repeated gPTP grandmaster changes) and their spacing in time are directly visible. Markers are colored by severity; mouse wheel zooms around the cursor, left drag pans, double-click fits the whole session, hovering a marker shows its details (including the time elapsed since the previous event of the same category), and clicking a marker selects the event in the table (selection is synchronized both ways).

`File > Export > Event Journal...` exports a snapshot of the current session at any time while recording.

## Possible future improvements

- Additional event types (entity renames, unsolicited notification losses, Milan compatibility changes, stream format changes).
- CSV export from the viewer.
- Configurable retention policy (currently a fixed 50 session files).
