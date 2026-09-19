# Journal Analyser

Standalone application for the offline analysis of Hive Event Journal files (`.hej`), without running Hive and without any network access.

## Usage

```
JournalAnalyser [files...]
```

`files` is a journal file (`.hej`) and optionally network state (`.ans`) or virtual entity (`.ave`) files. The entities of the journal are rebuilt from its snapshots and change tracking events as they were at the selected event (the twin of the network, driven by the Replay toolbar: Follow Selection, Latest, or a double-click on an event), inspected with the Entity Inspector (double-click an event) and displayed in the Network Graph (View menu). A network state or virtual entity file replaces the twin (the only way for journals without snapshots). The same files can be opened from the File menu or dropped on the window.

The application analyzes a single journal per instance: opening another journal (File menu, drag and drop, or a double-click in the OS) starts a new instance.

## Design

See the "Journal Analyser" section of `docs/EventJournal.md`. The application links the `Hive_static` library to reuse the Hive widgets (`EventJournalView`, `EntityInspector`, `NetworkGraphView`) and the application plumbing (`HiveApplication`, settings manager, stylesheet), with its own settings file.
