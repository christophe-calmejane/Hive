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
    category TEXT NOT NULL,              -- session, entity, connection, counters, media_clock, gptp, link, latency, redundancy, streaming, configuration, control
    severity TEXT NOT NULL,              -- info, warning, error, recovered
    entity_id TEXT,                      -- hex EID, empty for session-wide events
    entity_name TEXT,                    -- entity name at the time of the event (denormalized, so the file is self-contained)
    subject TEXT,                        -- sub-object designation (stream, interface, clock domain, ...)
    summary TEXT NOT NULL,               -- human readable description
    details TEXT,                        -- additional details, JSON object
    level TEXT NOT NULL DEFAULT 'user'   -- user, diagnostic (version 2)
);
CREATE TABLE snapshots (                 -- version 2, serialized entity models (see "Entity snapshots" below)
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    entity_id TEXT NOT NULL,
    reason TEXT NOT NULL,                -- online, session_start, manual
    format TEXT NOT NULL,                -- ave-msgpack-zlib
    data BLOB NOT NULL,                  -- the serialized entity, compressed
    uncompressed_size INTEGER NOT NULL
);
CREATE INDEX idx_events_timestamp ON events (timestamp);
CREATE INDEX idx_snapshots_entity ON snapshots (entity_id, timestamp);
```

Metadata keys: `schema_version`, `hive_version`, `hive_build`, `la_avdecc_version`, `os`, `computer_name`, `started_utc`, `stopped_utc` (written on clean stop only: a file without it was interrupted), `recording_level_at_start`, `snapshot_format`, `snapshots_count`, `snapshots_bytes`, `snapshots_serialization_ms` (size and cost figures of the snapshots, updated as they are written), `interface_id`, `interface_name`, and in dual-PI (redundant controller) mode `secondary_interface_id`, `secondary_interface_name`.

#### Compatibility rules

`metadata.schema_version` is the contract between writers and readers (`EventJournal::CurrentSchemaVersion`):

- schema changes are additive only: new tables, new columns with a default, new `details` keys, new `category` / `severity` / `level` values; a column is never renamed or dropped, the meaning of an existing value never changes, and an existing `summary` string is never changed (new information goes in `details`);
- a reader accepts any file whose version is lower than or equal to its own, and opens a higher version file in degraded mode rather than refusing it: the events are loaded, unknown `category` values fall back to `session`, unknown `severity` to `info`, unknown `level` to `user`, and the viewer shows a non blocking banner (`Session::isNewerThanReader()`);
- old files are never rewritten (no migration);
- every schema bump gets a row below and a fixture file of the previous version in `tests/data/eventJournal/` (`schema_v1.hej`, `schema_v2.hej`, ...), loaded by the `EventJournal_F.Session_*` unit tests.

| Version | Introduced by       | Changes                                                                                                                                      | Reader behavior on older files                                     |
| ------- | ------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| 1       | Hive 1.5.0-beta1    | Initial schema (`metadata`, `entities`, `events`)                                                                                            | n/a                                                                |
| 2       | Hive 1.5.0 (2026-09) | `events.level` column, `snapshots` table, metadata `recording_level_at_start`, `hive_build`, `la_avdecc_version`, `os`, `snapshot_*`; `streaming`, `configuration` and `control` categories; `Entity is online` details describe the entity; `gPTP AsPath changed` / `gPTP propagation delay changed` events; change tracking events with the `patch` details key (see below, added without a version bump: new categories and details keys are additive) | `level` read when the column exists (`PRAGMA table_info`), else `user`; no snapshots |

### Entity snapshots

A journal is self-contained: the full entity model of every entity is stored in the `snapshots` table when the entity comes online (`reason = online`, right after its baselines are seeded, the row id is referenced by the `snapshot_id` details key of the `Entity is online` event), for the entities already online when a recording starts (`session_start`), and on demand ("Snapshot Now" button of the dock, diagnostic recording only, `manual`, journaled as a `Snapshot of N entities taken` session event). Snapshots are written at both recording levels: they are what makes a journal usable without a network state dump.

Format `ave-msgpack-zlib` (`EventJournal::SnapshotFormat`): the la_avdecc JSON serializer output with the flags of a full entity export (ADP, compatibility, dynamic and static models, Milan, state, statistics, diagnostics) in the `.ave` binary encoding (MessagePack), compressed with `qCompress` (zlib). la_avdecc only serializes to files, so the writer goes through a temporary file in the system temporary directory. Measured on the field rig dump (77 entities loaded in a virtual controller, release build): 5.84 MB serialized, 410 KB stored (max 9 KB per entity), 103 ms of total serialization time on the GUI thread (1.3 ms per entity), so no worker thread is needed. `EventJournal::loadSnapshotsIndex()` reads the index without the blobs and `loadSnapshotData()` returns one decompressed entity, ready for `ControllerManager::loadVirtualEntityFromJson()` with the `BinaryFormat` flag (through a temporary file again).

The `Entity is online` event carries, in its `details`, the static description the analysis scripts need without decoding the snapshot: `entity_model_id`, `vendor` (OUI database), `firmware_version`, `milan_version` (Milan entities only), `avb_interfaces` (array of `{index, mac, clock_identity, grandmaster_id, priority1, as_path, propagation_delay}`, so the rack and branch of every plane is in the journal), `clock_domains` (array of `{index, clock_source_index, clock_source_stream_index}`, the last one only when the clock source is an input stream), `stream_inputs` (connected streams only, `{index, name, talker_entity_id, talker_stream_index}`), `snapshot_id` and `enumeration_time_ms`.

The gPTP AsPath of an interface is journaled on change (`gPTP AsPath changed`, category `gptp`, severity `info`, user level, `details.previous_as_path` / `new_as_path`): a topology event. The propagation delay of a link jitters by a few nanoseconds between measurements (5 to 16 ns on the field rig), so its changes are journaled at diagnostic level only (`gPTP propagation delay changed`, `details.previous_propagation_delay` / `new_propagation_delay`).

### Change tracking events and the JSON patches

At diagnostic level, every dynamic model change the controller notifies is journaled with a human readable summary and, in `details.patch` (`EventJournal::PatchDetailsKey`), a JSON Patch (RFC 6902) applying the change to the AVE document of the entity (the format of the snapshots): the state of an entity at any time of the session is its last snapshot plus the patches of the events journaled since (one generic loop, the analyser's time cursor of the evolution plan). The patch rules:

- only `replace` operations, because the la_avdecc serializer always writes the targeted keys (an unset optional is written as `null`), so an operation applies whatever the previous value;
- the paths are JSON Pointers (RFC 6901) rooted at the snapshot document, built by `libs/modelsLibrary/avePatch.hpp` from the descriptor type and index: the serializer writes the descriptors of a configuration as arrays ordered by index, and the nested descriptors (controls of an audio unit, jack, stream port or PTP instance; stream ports of an audio unit; audio clusters and audio maps of a stream port; PTP ports of a PTP instance) under their parent at the position "index minus the base index of the parent", looked up in the entity model (eg. `/entity_model/entity_descriptor/configuration_descriptors/0/audio_unit_descriptors/0/stream_port_output_descriptors/0/audio_cluster_descriptors/1/dynamic/object_name` for audio cluster 9 when the output stream port starts at cluster 8);
- the values are serialized by la_avdecc itself (`la/avdecc/internals/jsonTypes.hpp`), so they match the snapshots byte for byte; the acquire and lock states and the compatibility flags, serialized by the controller's private serializer, use the same names, rebuilt in `eventJournal.cpp`;
- an event without `patch` is a change the replay cannot apply (the talker side connections, not stored in the AVE document, or a descriptor that could not be located); a patch targeting a descriptor of a non active configuration (only its names can change) does not apply either, because the serializer only writes the dynamic model of the active configuration: the replay must count the operations it could not apply and say so;
- every change is deduplicated by value against the last journaled value of the item (baselines seeded from the entity model when the entity comes online): a redundant controller receives every notification once per interface, and la_avdecc does not compare the values of every notification before notifying.

The events (all `info`, `diagnostic` level unless stated otherwise, the user level ones of the table below simply carry a `patch` in addition to their existing details):

| Signal (`ControllerManager`)                                                                                                            | Category      | Summary                                                                              | Patch target                                                                                        |
| --------------------------------------------------------------------------------------------------------------------------------------- | ------------- | ------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------- |
| `entityNameChanged`, `entityGroupNameChanged`                                                                                           | configuration | `Entity name changed to 'X'`, `Entity group name changed to 'X'`                     | `.../entity_descriptor/dynamic/entity_name`, `group_name`                                           |
| `configurationNameChanged`, `audioUnitNameChanged`, `streamNameChanged`, `jackNameChanged`, `avbInterfaceNameChanged`, `clockSourceNameChanged`, `memoryObjectNameChanged`, `audioClusterNameChanged`, `controlNameChanged`, `clockDomainNameChanged`, `timingNameChanged`, `ptpInstanceNameChanged`, `ptpPortNameChanged` | configuration | `<Descriptor type> name changed to 'X'` (subject `<Descriptor type> <index>`) | `<descriptor>/dynamic/object_name` |
| `streamFormatChanged`                                                                                                                   | configuration | `Stream format changed to 0x...` (`details.stream_format`)                           | `<stream>/dynamic/stream_format`                                                                    |
| `streamDynamicInfoChanged`                                                                                                              | configuration | `Stream dynamic info changed` (`details.stream_id`, `msrp_accumulated_latency`, `stream_vlan_id`) | `<stream>/dynamic/stream_dynamic_info`                                                 |
| `streamRunningChanged`                                                                                                                  | configuration | `Stream started` / `Stream stopped`                                                  | `<stream>/dynamic/stream_running`                                                                   |
| `audioUnitSamplingRateChanged`                                                                                                          | configuration | `Sampling rate changed to N Hz` (`details.sampling_rate`)                            | `<audio unit>/dynamic/current_sampling_rate`                                                        |
| `clockSourceChanged`                                                                                                                    | configuration | `Clock source changed to Clock Source N (name)` (`details.clock_source_index`)       | `<clock domain>/dynamic/clock_source_index`                                                         |
| `mediaClockReferenceInfoChanged`                                                                                                        | configuration | `Media clock reference changed` (`details.user_media_clock_priority`, `media_clock_domain_name`) | `<clock domain>/dynamic/media_clock_reference_info`                                     |
| `streamPortAudioMappingsChanged`                                                                                                        | configuration | `Audio mappings changed (N mappings)` (the mappings are read from the entity model)  | `<stream port>/dynamic/dynamic_mappings`                                                            |
| `controlValuesChanged`                                                                                                                  | configuration | `Control value changed`, `Control value changed (N changes coalesced)` (`details.values`, `coalesced`), rate limited, see below | `<control>/dynamic/values`                                                  |
| `acquireStateChanged`                                                                                                                   | control       | `Entity acquired by this controller`, `Entity acquired by another controller (X)`, `Entity released` (`details.acquire_state`, `owning_controller_id`; the in progress states are skipped) | `/state/acquire_state`, `/state/owning_controller_id` |
| `lockStateChanged`                                                                                                                      | control       | `Entity locked by this controller`, `Entity locked by another controller (X)`, `Entity unlocked` (`details.lock_state`, `locking_controller_id`) | `/state/lock_state`, `/state/locking_controller_id` |
| `compatibilityChanged`                                                                                                                  | entity        | `Compatibility changed to IEEE17221, MILAN (Milan 1.2)` (`details.compatibility_flags`, `milan_version`) | `/compatibility_flags`, `/milan_compatibility_version`                                  |
| `entityCapabilitiesChanged`                                                                                                             | entity        | `Entity capabilities changed` (`details.entity_capabilities`)                        | `/adp_information/common/entity_capabilities`                                                       |
| `entityCountersChanged`, `avbInterfaceCountersChanged`, `clockDomainCountersChanged`, `streamInputCountersChanged`, `streamOutputCountersChanged` | counters | `Counters updated` (`details.changed`: array of `{counter, previous_value, value}`), see below | `<descriptor>/dynamic/counters` (the whole set)                                     |
| `streamOutputConnectionsChanged`                                                                                                        | connection    | `Talker connections changed (N listeners)` (`details.listeners`: array of `{entity_id, stream_index}`) | none (the talker side of the connections is not stored in the AVE document, la_avdecc rebuilds it from the listener side) |
| `streamInputConnectionChanged` (user level, existing event)                                                                             | connection    | (existing)                                                                           | `<stream input>/dynamic/connected_talker`, `connection_state`                                       |
| `gptpChanged`, `asPathChanged` (user level, existing events)                                                                            | gptp          | (existing)                                                                           | `<avb interface>/dynamic/gptp_grandmaster_id`, `gptp_domain_number`; `as_path`                      |
| `avbInterfaceInfoChanged`                                                                                                               | gptp          | `gPTP propagation delay changed` when the delay changed, `AVB interface info changed` otherwise (flags, MSRP mappings) | `<avb interface>/dynamic/avb_interface_info` (the whole information)          |
| `unsolicitedRegistrationChanged`, `redundancyWarningChanged`, `streamInputLatencyErrorChanged` (user level, existing events)             | entity, redundancy, latency | (existing)                                                              | `/state/subscribed_unsol`; `/diagnostics/redundancy_warning`; `/diagnostics/stream_input_latency_errors` (the set of streams in error, read from the entity diagnostics) |

Not tracked: `mediaClockChainChanged` (the chain is computed by la_avdecc from the models of every entity, it is not part of the AVE document and is recomputed when the twin is loaded), the link status (derived by la_avdecc from the `LINK_UP` / `LINK_DOWN` counters, which are journaled as raw counters).

**Raw counters.** A counters notification is journaled once per descriptor and notification, with the counters that changed since the last journaled set and a patch of the whole set, except when nothing changed (a duplicate notification, or a notification triggered by a counter Hive does not display) and when only the liveness counters moved (`FRAMES_RX`, `FRAMES_TX`, `TIMESTAMP_VALID`, `LivenessCounterNames`): on the field rig every connected stream produces one such notification per second (about 300 per second in total), which would flood the journal for nothing, the `FRAMES_RX` increments being consumed by the stream data loss rule instead. Four devices of the rig also expose `FRAMES_RX` / `FRAMES_TX` in their AVB interface counters, hence the rule applies to every descriptor type. The liveness counters are still part of the tracked set, so the first informative change after a long streaming period reports their previous values correctly. The user level events derived from the counters (error counters, media clock transitions) are unchanged and written at both levels. No time based coalescing is done (the plan suggested 100 ms per entity): the value dedup and the liveness exception remove the identified sources of floods, the actual volume is to be measured on the first diagnostic session of the rig.

**Control values rate limiting.** Faders and meters can notify several times per second: a control journals at most one value change per second (`ControlValuesRateLimitInterval`). The first change is journaled at once; the following changes within the interval are kept pending (the last value wins, the same value again is not a change) and journaled by a single shot timer once the interval elapsed, with `details.coalesced` counting the changes that were skipped (the summary says `(N changes coalesced)`). The pending values are flushed when the session stops, so no value is lost, only intermediate ones. The values are serialized like the AVE document (`{"type": "CONTROL_LINEAR_UINT8", "values": [10]}`).

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
| gPTP        | `asPathChanged` (topology change, user level), `avbInterfaceInfoChanged` (propagation delay change, diagnostic level)                                                                                                                                                       | Info                          |
| Link        | `avbInterfaceLinkStatusChanged`                                                                                                                                                                                                                                             | Error (down) / Recovered (up) |
| Latency     | `streamInputLatencyErrorChanged`                                                                                                                                                                                                                                            | Error / Recovered             |
| Redundancy  | `redundancyWarningChanged`                                                                                                                                                                                                                                                  | Error / Recovered             |
| Redundancy  | `interfaceTransportError` (transport error on one controller interface in dual-PI mode; the event tells whether the other interface is still operational)                                                                                                                   | Error                         |
| Redundancy  | `interfaceUnsolicitedRegistrationChanged` (dual-PI mode only: one controller interface lost/recovered its unsolicited notifications subscription for an entity, key sign of per-network instability)                                                                        | Warning / Recovered           |
| Redundancy  | `entityRedundantInterfaceOffline` / `entityRedundantInterfaceOnline` (entity lost/recovered on one of its redundant interfaces while remaining online on the other; the online notification is only journaled as a recovery, not during normal discovery)                   | Warning / Recovered           |
| Streaming   | `streamInputCountersChanged` (`FRAMES_RX` increments of the connected stream inputs: `Stream data loss` when frames were missing during the interval, `Stream data monitoring started` (diagnostic level) when the reference of a stream is learned, see below)             | Error / Info                  |
| Configuration | Name, stream format, stream running, stream dynamic info, sampling rate, clock source, media clock reference, audio mappings and control value changes (diagnostic level, see the change tracking table above)                                                          | Info                          |
| Control     | `acquireStateChanged`, `lockStateChanged` (diagnostic level, see the change tracking table above)                                                                                                                                                                          | Info                          |
| Counters    | Raw counters (`Counters updated`, diagnostic level, see above)                                                                                                                                                                                                             | Info                          |
| Entity      | `compatibilityChanged`, `entityCapabilitiesChanged` (diagnostic level)                                                                                                                                                                                                     | Info                          |
| Connection  | `streamOutputConnectionsChanged` (talker side, diagnostic level)                                                                                                                                                                                                           | Info                          |

All the events of this table are `user` level, except `Stream data monitoring started`, `gPTP propagation delay changed` and the change tracking events (`diagnostic`).

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

The entities are hosted by a **virtual controller** (`ProtocolInterface::Type::Virtual`, no network access), populated from the entity snapshots stored in the journal (the last snapshot of every entity, so the inspector and the graph show the network as last seen during the session; the replay at the time of a selected event is the next step of the evolution plan). A network state file (`File > Load Network State`, or `.ans`/`.ave` files given on the command line or dropped on the window) replaces the snapshot entities, as an explicit override, and is the only source for journals predating the snapshots (schema version 1). Double-clicking an event whose entity is not loaded shows a hint in the status bar.

The application runs **one instance per journal**: journal files are given as positional command line arguments, and a journal opened while one is already loaded (File > Open, drag and drop, or a macOS FileOpen event, which Launch Services always routes to the running instance) is started in a new process (`QProcess::startDetached` on the application executable). The analyser keeps its own settings file (`KikiSoft/JournalAnalyser.ini`, same location scheme as Hive) for the theme color, appearance and graph layout, so Hive's preferences are never touched.

## File association

The `.hej` extension is associated with the Journal Analyser on macOS (`JournalAnalyser.plist.in` document type, `Owner` rank, Hive no longer declares it) and on Windows (installer file association pointing to `bin/JournalAnalyser.exe`, whose first icon is `Icon_HEJ.ico`), so double-clicking a journal file opens it in the analyser, in a new instance. On Linux the AppImage only bundles Hive, so the `application/x-hive-hej` mime type keeps opening journals in Hive until the analyser gets its own package.

Hive itself still opens journal files through `File > Open Event Journal` (standalone viewer window in the running Hive): `MainWindowImpl::loadFile()` handles the extension like `.ave`/`.ans` files (command line arguments, macOS FileOpen events, and forwarding to the already running instance on Windows). On macOS, FileOpen events received before the MainWindow is created (eg. while the splashscreen is displayed) are buffered by `HiveApplication` and loaded once the window exists.

## Possible future improvements

- Configurable retention policy (currently a fixed 50 session files).
