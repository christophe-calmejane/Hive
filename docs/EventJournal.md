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

### Recording levels

Every event has a level, telling who it is for:

- `user` (default): what the sound engineer needs, the only level recorded unless diagnostic recording is enabled;
- `diagnostic`: everything needed for deep analysis and, later, for rebuilding the state of the network (much larger files).

Diagnostic recording is toggled from the Event Journal dock ("Record Diagnostic" button, not from the preferences), the choice is persisted (`settings::EventJournal_DiagnosticLevel`) so the next session starts in the same mode, and the current mode is shown in the dock status label. `EventJournal::setRecordingLevel()` applies it: user events are always written, diagnostic events only while the level is `Diagnostic`, and a switch during a session writes a user level `session` event (`Diagnostic recording enabled` / `Diagnostic recording disabled`) so any reader knows which periods carry diagnostic data (`metadata.recording_level_at_start` gives the initial mode). Enabling it also makes every monitored stream announce its `Stream data monitoring started` again at its next notification, so the diagnostic period starts with the coverage information.

Viewers hide the diagnostic events by default ("Show Diagnostic" button), so a diagnostic journal stays readable by a sound engineer; the Journal Analyser shows them by default.

### Schema (version 2)

```sql
CREATE TABLE metadata (key TEXT PRIMARY KEY NOT NULL, value TEXT);
CREATE TABLE entities (entity_id TEXT PRIMARY KEY NOT NULL, name TEXT);
CREATE TABLE events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,          -- milliseconds since Unix epoch, UTC
    category TEXT NOT NULL,              -- session, entity, connection, counters, media_clock, gptp, link, latency, redundancy, streaming
    severity TEXT NOT NULL,              -- info, warning, error, recovered
    entity_id TEXT,                      -- hex EID, empty for session-wide events
    entity_name TEXT,                    -- entity name at the time of the event (denormalized, so the file is self-contained)
    subject TEXT,                        -- sub-object designation (stream, interface, clock domain, ...)
    summary TEXT NOT NULL,               -- human readable description
    details TEXT,                        -- additional details, JSON object
    level TEXT NOT NULL DEFAULT 'user'   -- user, diagnostic (version 2)
);
CREATE TABLE snapshots (                 -- version 2, serialized entity models (empty until the snapshots are recorded, see the evolution plan)
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    entity_id TEXT NOT NULL,
    reason TEXT NOT NULL,                -- online, session_start, manual
    format TEXT NOT NULL,                -- serialization format identifier
    data BLOB NOT NULL,
    uncompressed_size INTEGER NOT NULL
);
CREATE INDEX idx_events_timestamp ON events (timestamp);
CREATE INDEX idx_snapshots_entity ON snapshots (entity_id, timestamp);
```

Metadata keys: `schema_version`, `hive_version`, `hive_build`, `la_avdecc_version`, `os`, `computer_name`, `started_utc`, `stopped_utc` (written on clean stop only: a file without it was interrupted), `recording_level_at_start`, `interface_id`, `interface_name`, and in dual-PI (redundant controller) mode `secondary_interface_id`, `secondary_interface_name`.

#### Compatibility rules

`metadata.schema_version` is the contract between writers and readers (`EventJournal::CurrentSchemaVersion`):

- schema changes are additive only: new tables, new columns with a default, new `details` keys, new `category` / `severity` / `level` values; a column is never renamed or dropped, the meaning of an existing value never changes, and an existing `summary` string is never changed (new information goes in `details`);
- a reader accepts any file whose version is lower than or equal to its own, and opens a higher version file in degraded mode rather than refusing it: the events are loaded, unknown `category` values fall back to `session`, unknown `severity` to `info`, unknown `level` to `user`, and the viewer shows a non blocking banner (`Session::isNewerThanReader()`);
- old files are never rewritten (no migration);
- every schema bump gets a row below and a fixture file of the previous version in `tests/data/eventJournal/` (`schema_v1.hej`, `schema_v2.hej`, ...), loaded by the `EventJournal_F.Session_*` unit tests.

| Version | Introduced by       | Changes                                                                                                                                      | Reader behavior on older files                                     |
| ------- | ------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| 1       | Hive 1.5.0-beta1    | Initial schema (`metadata`, `entities`, `events`)                                                                                            | n/a                                                                |
| 2       | Hive 1.5.0 (2026-09) | `events.level` column, `snapshots` table, metadata `recording_level_at_start`, `hive_build`, `la_avdecc_version`, `os`; `streaming` category | `level` read when the column exists (`PRAGMA table_info`), else `user` |

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
| Streaming   | `streamInputCountersChanged` (`FRAMES_RX` increments of the connected stream inputs: `Stream data loss` when frames were missing during the interval, `Stream data monitoring started` (diagnostic level) when the reference of a stream is learned, see below)             | Error / Info                  |

All the events of this table are `user` level, except `Stream data monitoring started` (`diagnostic`).

The `Recovered` severity marks the end of a previously reported error condition, so the duration of an incident (clock unlock, link down, ...) is directly visible in the journal.

To only record actual changes (and not the initial state of each entity being enumerated), the module seeds change-detection baselines from the entity model when an entity comes online (`EventJournal::Private::seedBaselines`), and error counters are compared against their previously recorded values.

### Media clock transitions inferred from the counters

The media clock lock state of a clock domain is derived from its `LOCKED` and `UNLOCKED` counters (locked when `LOCKED > UNLOCKED`), but the counters are only notified once in a while: a clock that unlocks and relocks between two notifications increments both counters without changing the derived state. Rather than losing those events, the journal replays the transitions implied by the counter increments since the previous notification, in the only order consistent with an alternating unlock/lock sequence starting from the previous state, and always ends on the state derived from the counters. Every media clock event carries the raw values in `details` (`locked_counter`, `unlocked_counter`, `locked_increment`, `unlocked_increment`), and the transitions that could not have been observed by comparing the lock states before and after the notification carry `"inferred": true` (all of them when the state did not change, all but the last one otherwise). Inferred events share the timestamp of the notification, so the duration of an inferred unlock is not known (only that it was shorter than the notification interval). A counter going backwards (device reboot, counters reset) carries no usable increment: only the resulting lock state is compared.

### Stream data loss detected from the `FRAMES_RX` increments

An input stream that stops receiving data is only reported by the device itself through the `STREAM_INTERRUPTED` counter, whose trigger is implementation defined (the field journals show thousands of `Media Unlocked` events for a handful of `Stream Interrupted`). The journal therefore also watches the data reception from the controller side, using what Milan 1.2 guarantees:

- `FRAMES_RX` is mandatory on every Stream Input (table 5.16) and is "incremented at the end of every observation interval during which at least one Stream Data AVTPDU has been received", the observation interval being implementation specific and at most one second (table 5.6);
- a device sends at most one `GET_COUNTERS` unsolicited notification per descriptor per second (table 5.22), so two consecutive notifications of a stream are at least one second apart and, while frames flow, the increment between them is at least one second worth of frames;
- the Stream Input counters are reset each time the stream goes from not bound to bound.

**The rule** (`EventJournal::Private::handleStreamInputCountersChanged`, no timer): for every stream input that is connected (state read live from the entity model at evaluation time, the same source as the `Media Unlocked` rule) and carries `FRAMES_RX`, the increment `d` between two consecutive notifications is computed modulo 2^32 (the counter wraps every few days at 8000 frames per second) together with the time elapsed between their arrivals on the main thread. The expected one second worth of frames `e` is learned per stream as the 10th percentile of the last 30 increments (the low end is the reference: a device whose cadence varies between one and two seconds produces larger increments, and a single undetected partial loss must not lower the reference); nothing is evaluated before 10 increments. Then:

- `d = 0` is not a new observation: a redundant (dual interface) controller receives every notification once per interface, with the same payload a few ms apart (or seconds apart when the main thread stalled between the two), and a notification triggered by another counter while no frame arrived tells nothing the next one will not tell with a longer elapsed time. It is ignored entirely (the reference sample is not moved);
- a notification arriving less than 900 ms after the previous one (queued burst after a main thread stall, the other interface of a redundant controller having re-read the counter a few frames later, non compliant device) only moves the reference sample: it is neither evaluated nor learned, so it can neither raise an event nor lower the reference;
- `d > 10 x e` is a counter reset (rebind) or a value jump: the reference sample is resynchronized, nothing is journaled, the learned increments are kept;
- `d < 0.5 x e`: one `Stream data loss: about N ms of frames missing` event (category `streaming`, severity `error`, subject = the input stream), with `details.frames_rx`, `increment`, `expected_increment`, `elapsed_ms`, `missing_ms` (`(e - d) / e x 1000`, the estimate assumes a one second device cadence), `talker_id` and `talker_stream_index` (so listener and talker events can be joined without a network state dump). The alerted increment is not learned, so repeated partial losses keep being reported against the same reference.

The learning restarts when the connection of the stream changes (the counters are reset by a rebind and the first increment after a connection is partial) and when its stream format changes (the frame rate may change). The baseline of a connected stream is seeded from the entity model when the entity comes online. The end of the learning phase is journaled once per stream as `Stream data monitoring started` (severity `info`, level `diagnostic`, so only with diagnostic recording enabled; `details.expected_increment`, `samples`, `talker_id`, `talker_stream_index`): a stream without that event is not monitored (no `FRAMES_RX`, not connected, no frames received so far, or a device counting once per observation interval of one second, for which `e = 1` and the threshold is never crossed).

**Why it produces no false positive** (the guarantee the journal must keep): every controller side problem makes the increment larger or delays it, never smaller. A lost unsolicited notification doubles the next increment; an expired subscription or a controller interface down stops the notifications, then delivers a large increment; a stalled main thread delivers the queued notifications in a burst with normal increments and no elapsed time (not evaluated); a counter reset or a wrap goes through the modular arithmetic and the `10 x e` rule; a stream the user just disconnected is already seen as not connected when its last partial second is processed (la_avdecc updates its model before the queued notification reaches the main thread). Only a stream that really received less than half a second worth of frames in an interval of at least 900 ms is reported, which includes a talker deliberately stopped (`STOP_STREAMING`) or gone offline: a true positive, explained by the other events. In exchange the rule only sees an interruption at its edges: a long silence gives an event at its start (when the cut falls in the first half of the device interval), nothing during the silence (the device has nothing to report) and an event at the resume, whose `elapsed_ms` is the duration of the silence. Events of the two members of a redundant stream within the same second are the "both planes" signal; they are correlated at analysis time, not at recording time.

Known residual exposure, to be checked during the field validation: a redundant controller whose devices re-read the counter for the second interface (a near duplicate, a few frames above the first one) combined with a main thread stall starting between the processing of the two copies would evaluate the near duplicate as a tiny increment over a long interval. Such a false event is recognizable (`increment` of a few frames, `elapsed_ms` equal to the stall, one stream at a time); if it shows up, the fix is to ignore increments below a few frames, not to add a watchdog.

Deliberately not done: a timer based watchdog on the absence of notifications (it cannot distinguish a device receiving no frames from a controller receiving no notifications and would reintroduce controller side false positives), `recovered` events, redundant pair state at recording time. The only tuning knob is the loss threshold (`StreamDataLossThreshold`, 0.5): the noise floor of the increments is a few percent, so it can be lowered after a field validation if the edge events turn out to be too rare.

## Viewer UI

`EventJournalView` (in `src/eventJournal/`) displays a session with filtering, in two modes (same pattern as the Entity Model Inspector):

- **Live**: embedded in the `MainWindow` "Event Journal" dock (View menu, hidden by default), following the current recording session, with auto-scroll and an Export button.
- **File**: `File > Open Event Journal...` opens any journal file in a standalone viewer window (multiple windows can be opened, eg. to compare with the live session).

Features: severity/category/entity filter menus, diagnostic level events toggle, free text search (regular expression), time range filter, per-severity row colors (error/warning/recovered), details pane for the selected event, event counters, degraded mode banner for files written by a newer schema. `EventJournalModel` + `EventJournalFilterProxyModel` (in the same directory) implement the table and filtering.

A timeline strip (`EventJournalTimeline`) is displayed above the table, plotting the filtered events on a time axis with one lane per category, so recurring problems (eg. repeated gPTP grandmaster changes) and their spacing in time are directly visible. Markers are colored by severity; mouse wheel zooms around the cursor, left drag pans, double-click fits the whole session, hovering a marker shows its details (including the time elapsed since the previous event of the same category), and clicking a marker selects the event in the table (selection is synchronized both ways).

`File > Export > Event Journal...` exports a snapshot of the current session at any time while recording.

The Export button of the viewer offers two formats: a journal file snapshot (live session only) and a CSV file (all events or only the currently filtered ones, with a Level column), for analysis in a spreadsheet.

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
