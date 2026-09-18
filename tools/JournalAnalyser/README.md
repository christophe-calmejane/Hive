# Journal Analyser

Standalone application for the offline analysis of Hive Event Journal files (`.hej`), without running Hive and without any network access.

## Usage

```
JournalAnalyser [files...]
```

`files` is a journal file (`.hej`) and optionally network state (`.ans`) or virtual entity (`.ave`) files to load into the virtual controller, so the entities of the journal can be inspected (double-click an event to open the Entity Inspector of its entity) and displayed in the Network Graph (View menu). The same files can be opened from the File menu or dropped on the window.

The application analyzes a single journal per instance: opening another journal (File menu, drag and drop, or a double-click in the OS) starts a new instance.

## Design

See the "Journal Analyser" section of `docs/EventJournal.md`. The application links the `Hive_static` library to reuse the Hive widgets (`EventJournalView`, `EntityInspector`, `NetworkGraphView`) and the application plumbing (`HiveApplication`, settings manager, stylesheet), with its own settings file.
