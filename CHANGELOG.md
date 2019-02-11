# Hive Changelog
All notable changes to Hive will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- Numerical values for _StreamFlags_, _StreamFlagsEx_, _ProbingStatus_ and _AcmpStatus_

### Fixed
- Milan Certification version properly displayed (as x.y.z.w value)

## [1.0.10] - 2019-02-04
### Added
- Windows binary code-signing
- Version clearly saying _beta_, when it's a beta build

### Fixed
- [New version popup hidden behind splash-screen](https://github.com/christophe-calmejane/Hive/issues/49)
- Possible crash when powering-up a device
- Possible crash when using _macOS native interface_ with a Milan compatible device

## [1.0.9] - 2019-02-02
### Added
- Support for _Locking/Unlocking_ an entity
- Detection and display of _Milan compatible_ devices
- Display of _AS Path_ in AVB Interface descriptor
- Display of the AVB Interface _link status_ (when available)
- Button to disconnect (unbind) a _ghost talker_ from Stream Input descriptor
- Possibility to connect a non-redundant stream to a redundant one (one of the pair)
- Milan GetStreamInfo extended information
- Milan StreamOutput counters
- Detection of devices not supporting _Acquire_ and/or _Lock_ commands
- Display of the current dynamic mappings without having to edit them, in StreamPort descriptor
- Button to clear all dynamic mappings in StreamPort descriptor
- Tooltip when the mouse is over a _flags field_ of a descriptor
- Basic entity filtering in connection matrix
- Confirmation dialog when clearing the debug log
- Possibility to apply log filters to the saved output
- [Automatic check for new version can now check for BETA releases](https://github.com/christophe-calmejane/Hive/issues/46)

### Changed
- Icon when an entity is acquired by Hive (changed color from orange to green)
- Changed the colors in the Connection Matrix (see Legend)
- Logger configuration menus does not close automatically

### Fixed
- Upload firmware progression always set to 100% upon successfull completion
- Connection matrix refresh issues
- Possible crash if a toxic entity is on the network
- Exclusive Access not refreshed in Entity Descriptor information
- [Entity Model Inspector focus lost when a new entity is detected](https://github.com/christophe-calmejane/Hive/issues/19)
- [[macOS] Forcing light mode until full dark mode is supported by Qt](https://github.com/christophe-calmejane/Hive/issues/39)
- [Restoring previous ComboBox value if the command failed (Configuration/SamplingRate/ClockSource)](https://github.com/christophe-calmejane/Hive/issues/18)
- Incorrect connection established between 2 redundant streams when clicking on a non-connectable box
- Automatically selecting the Entity descriptor when inspecting a new entity ([for now](https://github.com/christophe-calmejane/Hive/issues/22))
- Partial deadlock (in background tasks) when editing channel mappings, sometimes leading to the impossibility to apply the mappings
- Possible crash upon loading after having changed ProtocolInterface and/or NetworkInterface multiple times
- _Current Stream Format_ field not properly refreshed
- EntityID column always displayed (instead of the Logo column)
- [Entity logo scaled to fit](https://github.com/christophe-calmejane/Hive/issues/48)

## [1.0.8] - 2018-10-30
### Fixed
- Possible crash if an entity goes online and offline almost at the same time
- [Redundant streams out of sync (cannot always connect/disconnect)](https://github.com/christophe-calmejane/Hive/issues/35)
- [Restoring previous ComboBox value if the command failed (StreamFormat/SamplingRate)](https://github.com/christophe-calmejane/Hive/issues/18)
- Showing not fully compliant entities

## [1.0.7] - 2018-10-03
### Added
- [SetName for AudioUnit, AvbInterface, ClockSource, MemoryObject, AudioCluster and ClockDomain](https://github.com/christophe-calmejane/Hive/issues/7)
- AvbInterface, ClockDomain and StreamInput Counters
- Support for firmware update
- [Option to invert the talkers and listeners in connection matrix](https://github.com/christophe-calmejane/Hive/issues/13)
- [Full ChangeLog accessible from the Help menu](https://github.com/christophe-calmejane/Hive/issues/24)

### Changed
- Updated la_avdecc to v2.7.1
- [Using something more lightweight (and faster) than QtWebEngine for ChangeLog](https://github.com/christophe-calmejane/Hive/issues/21)

### Fixed
- [Memory Object length changes not properly notified](https://github.com/christophe-calmejane/Hive/issues/30)
- [Image/Logo possible invalid size](https://github.com/christophe-calmejane/Hive/issues/29)
- [Image/Logo background garbage](https://github.com/christophe-calmejane/Hive/issues/26)
- [Acquired state properly initialized](https://github.com/christophe-calmejane/Hive/issues/8)
- Connection matrix highlighting issues
- Optimize the code for ConnectionMatrixModel::ConnectionMatrixModelPrivate::refreshHeader

## [1.0.6] - 2018-08-08
### Added
- [Display missing basic entity information](https://github.com/christophe-calmejane/Hive/issues/11)

### Removed
- ["Access" information from all descriptors but Entity](https://github.com/christophe-calmejane/Hive/issues/3)

### Changed
- CRF StreamFormat reports 0 instead of 1 for the count of channels it has

### Fixed
- Incorrect StreamOutput channel dynamic mappings
- Possible crash in avdecc library

## [1.0.5] - 2018-08-06
### Added
- [Support for dynamic mappings on StreamOutput](https://github.com/christophe-calmejane/Hive/issues/9)

### Fixed
- [Crash when changing active configuration](https://github.com/christophe-calmejane/Hive/issues/15)
- [Incorrect dynamic mappings sent when adding/removing them](https://github.com/christophe-calmejane/Hive/issues/12)

## [1.0.4] - 2018-07-30
### Added
- [Display entity's logo image in the entity list](https://github.com/christophe-calmejane/Hive/issues/10)
- Support for manufacturer and entity images download in Memory Object descriptors
- Automatic check for new version

## [1.0.3] - 2018-07-17
### Added
- Support for Memory Object descriptors (Contributed by Florian Harmuth)
- Settings menu

### Fixed
- [Matrix connections contextual menu not always correct](https://github.com/christophe-calmejane/Hive/issues/4)
- [Descriptor name randomly change in entity inspector](https://github.com/christophe-calmejane/Hive/issues/5)

## [1.0.2] - 2018-06-18
### Fixed
- [Possible crash using "macOS native interface"](https://github.com/christophe-calmejane/Hive/issues/1)

## [1.0.1] - 2018-06-09
### Fixed
- Issue when a device returns 0 as the total number of dynamic maps

## [1.0.0] - 2018-06-03
- First public version
