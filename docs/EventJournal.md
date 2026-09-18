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

Metadata keys: `schema_version`, `hive_version`, `computer_name`, `started_utc`, `stopped_utc`, `interface_id`, `interface_name`, and in dual-PI (redundant controller) mode `secondary_interface_id`, `secondary_interface_name`.

### Recorded events

| Category    | Source signals                                                                                                                                                                                                                                                              | Severity                      |
| ----------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------- |
| Session     | `controllerOnline` / `controllerOffline` / `transportError` (fatal transport error on every controller interface)                                                                                                                                                           | Info / Error                  |
| Entity      | `entityOnline` / `entityOffline`                                                                                                                                                                                                                                            | Info / Warning                |
| Entity      | `unsolicitedRegistrationChanged` (aggregated state: the entity is only out-of-sync when no controller interface is subscribed anymore)                                                                                                                                      | Error / Recovered             |
| Connection  | `streamInputConnectionChanged` (listener side)                                                                                                                                                                                                                              | Info                          |
| Counters    | `streamInputErrorCounterChanged`, `statisticsErrorCounterChanged` (the counters Hive flags as errors); in dual-PI mode the statistics error counter increases are journaled from the per-counter signals instead, attributing each increase to the interface it occurred on | Error                         |
| Media Clock | `clockDomainCountersChanged` (Locked/Unlocked transitions, same rule as the Discovered Entities list, including the transitions inferred from the counter increments, see below)                                                                                         | Error / Recovered             |
| gPTP        | `gptpChanged` (grandmaster or domain change)                                                                                                                                                                                                                                | Warning                       |
| Link        | `avbInterfaceLinkStatusChanged`                                                                                                                                                                                                                                             | Error (down) / Recovered (up) |
| Latency     | `streamInputLatencyErrorChanged`                                                                                                                                                                                                                                            | Error / Recovered             |
| Redundancy  | `redundancyWarningChanged`                                                                                                                                                                                                                                                  | Error / Recovered             |
| Redundancy  | `interfaceTransportError` (transport error on one controller interface in dual-PI mode; the event tells whether the other interface is still operational)                                                                                                                   | Error                         |
| Redundancy  | `interfaceUnsolicitedRegistrationChanged` (dual-PI mode only: one controller interface lost/recovered its unsolicited notifications subscription for an entity, key sign of per-network instability)                                                                        | Warning / Recovered           |
| Redundancy  | `entityRedundantInterfaceOffline` / `entityRedundantInterfaceOnline` (entity lost/recovered on one of its redundant interfaces while remaining online on the other; the online notification is only journaled as a recovery, not during normal discovery)                   | Warning / Recovered           |

The `Recovered` severity marks the end of a previously reported error condition, so the duration of an incident (clock unlock, link down, ...) is directly visible in the journal.

To only record actual changes (and not the initial state of each entity being enumerated), the module seeds change-detection baselines from the entity model when an entity comes online (`EventJournal::Private::seedBaselines`), and error counters are compared against their previously recorded values.

### Media clock transitions inferred from the counters

The media clock lock state of a clock domain is derived from its `LOCKED` and `UNLOCKED` counters (locked when `LOCKED > UNLOCKED`), but the counters are only notified once in a while: a clock that unlocks and relocks between two notifications increments both counters without changing the derived state. Rather than losing those events, the journal replays the transitions implied by the counter increments since the previous notification, in the only order consistent with an alternating unlock/lock sequence starting from the previous state, and always ends on the state derived from the counters. Every media clock event carries the raw values in `details` (`locked_counter`, `unlocked_counter`, `locked_increment`, `unlocked_increment`), and the transitions that could not have been observed by comparing the lock states before and after the notification carry `"inferred": true` (all of them when the state did not change, all but the last one otherwise). Inferred events share the timestamp of the notification, so the duration of an inferred unlock is not known (only that it was shorter than the notification interval). A counter going backwards (device reboot, counters reset) carries no usable increment: only the resulting lock state is compared.

## Viewer UI

`EventJournalView` (in `src/eventJournal/`) displays a session with filtering, in two modes (same pattern as the Entity Model Inspector):

- **Live**: embedded in the `MainWindow` "Event Journal" dock (View menu, hidden by default), following the current recording session, with auto-scroll and an Export button.
- **File**: `File > Open Event Journal...` opens any journal file in a standalone viewer window (multiple windows can be opened, eg. to compare with the live session).

Features: severity/category/entity filter menus, free text search (regular expression), time range filter, per-severity row colors (error/warning/recovered), details pane for the selected event, event counters. `EventJournalModel` + `EventJournalFilterProxyModel` (in the same directory) implement the table and filtering.

A timeline strip (`EventJournalTimeline`) is displayed above the table, plotting the filtered events on a time axis with one lane per category, so recurring problems (eg. repeated gPTP grandmaster changes) and their spacing in time are directly visible. Markers are colored by severity; mouse wheel zooms around the cursor, left drag pans, double-click fits the whole session, hovering a marker shows its details (including the time elapsed since the previous event of the same category), and clicking a marker selects the event in the table (selection is synchronized both ways).

`File > Export > Event Journal...` exports a snapshot of the current session at any time while recording.

The Export button of the viewer offers two formats: a journal file snapshot (live session only) and a CSV file (all events or only the currently filtered ones), for analysis in a spreadsheet.

## Journal Analyser

`JournalAnalyser` (in `tools/JournalAnalyser/`) is a standalone application dedicated to the offline analysis of journal files, so a show can be analyzed without running Hive (and without a network interface). It links the `Hive_static` library to reuse the Hive widgets as they are:

- the journal viewer (`EventJournalView` in file mode, with its filters, search and timeline) as central widget;
- standalone `EntityInspector` windows, opened by double-clicking an event (one window per entity, raised when already open);
- the `NetworkGraphView` in a dock widget.

The entities are hosted by a **virtual controller** (`ProtocolInterface::Type::Virtual`, no network access), populated from a network state file (`File > Load Network State`, or `.ans`/`.ave` files given on the command line or dropped on the window). The inspector and the graph therefore display the state captured in that file, until journals embed their own entity snapshots (see the evolution plan). Double-clicking an event whose entity is not loaded shows a hint in the status bar.

The application runs **one instance per journal**: journal files are given as positional command line arguments, and a journal opened while one is already loaded (File > Open, drag and drop, or a macOS FileOpen event, which Launch Services always routes to the running instance) is started in a new process (`QProcess::startDetached` on the application executable). The analyser keeps its own settings file (`KikiSoft/JournalAnalyser.ini`, same location scheme as Hive) for the theme color, appearance and graph layout, so Hive's preferences are never touched.

## File association

The `.hej` extension is associated with the Journal Analyser on macOS (`JournalAnalyser.plist.in` document type, `Owner` rank, Hive no longer declares it) and on Windows (installer file association pointing to `bin/JournalAnalyser.exe`, whose first icon is `Icon_HEJ.ico`), so double-clicking a journal file opens it in the analyser, in a new instance. On Linux the AppImage only bundles Hive, so the `application/x-hive-hej` mime type keeps opening journals in Hive until the analyser gets its own package.

Hive itself still opens journal files through `File > Open Event Journal` (standalone viewer window in the running Hive): `MainWindowImpl::loadFile()` handles the extension like `.ave`/`.ans` files (command line arguments, macOS FileOpen events, and forwarding to the already running instance on Windows). On macOS, FileOpen events received before the MainWindow is created (eg. while the splashscreen is displayed) are buffered by `HiveApplication` and loaded once the window exists.

## Possible future improvements

- Additional event types (entity renames, Milan compatibility changes, stream format changes).
- Configurable retention policy (currently a fixed 50 session files).
