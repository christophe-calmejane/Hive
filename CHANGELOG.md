# Hive Changelog
All notable changes to Hive will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- [Automatically _fixing_ invalid mapping when changing stream input format](https://github.com/christophe-calmejane/Hive/issues/44)
- [New matrix color code (grey) for stream format incompatibility (ie. no format exist in listener's list that would fit for the talker's current format)](https://github.com/christophe-calmejane/Hive/issues/29)
- [Button to clear errors for all entities](https://github.com/christophe-calmejane/Hive/issues/77)
- [Button to remove all active connections](https://github.com/christophe-calmejane/Hive/issues/107)
- Search filter for discovered entities (currently only by name)

### Fixed
- First entity might be automatically inspected (but not selected) when Hive goes to foreground
- Currently highlighted entity in connection matrix incorrect when an entity comes online or offline
- Adapting listener's format to talker's format from the matrix (contextual menu)

## [1.2.7] - 2021-12-24
### Added
- [New 'Matrix' layout with only the connection matrix displayed by default](https://github.com/christophe-calmejane/Hive/issues/109)
- Command line option to specify the application settings file to use
- Currently selected entity is highlighted in connection matrix
- _Device Details Dialog_ tab 'Stream Format' for simplified user access to stream format settings

### Changed
- Discovered Entities list is now dockable, with show/hide option from the View menu
- Application settings are now saved in the .ini format (whatever the platform)
- If the currently selected entity goes offline, active selection is removed instead of randomly choosing another entity

### Fixed
- [Main window's size not properly restored on macOS](https://github.com/christophe-calmejane/Hive/issues/108)
- Default sort entities list column is EntityID instead of Logo (which is not sortable)

## [1.2.6] - 2021-07-22
### Changed
- [Minimum required macOS version set to 10.13 (required by Qt 5.15)](https://github.com/christophe-calmejane/Hive/issues/103)

## [1.2.5] - 2021-07-21
### Added
- More color explanation in the Connection Matrix legend
- Visualization of a Connected and Media Locked stream in Connection Matrix intersection (same information than the header arrow), Milan Only
  - Can be enabled/disabled in Settings
- Confirmation dialog when trying to disconnect a Media Locked stream whose Talker is not visible on the network
- [Possibility to sort entities by column](https://github.com/christophe-calmejane/Hive/issues/81)
- Possibility to change AssociationID for devices supporting it
- Support for ANS files loading (only to create virtual entities)

### Fixed
- Matrix being refreshed more than required
- [Unhandled exception causing crash](https://github.com/christophe-calmejane/Hive/issues/101)
- Entity properly show identification if discovered while actively identifying
- [Frequent app hang when multiple Milan entities are on the network and streaming](https://github.com/christophe-calmejane/Hive/issues/104)
- _Device Details Dialog_ 'Receive'/'Transmit' tables to correctly show connections resulting from connections of a single talker stream to multiple listener streams on a receiving device
- _Device Details Dialog_ 'Receive'/'Transmit' tables to show correct connection status WrongDomain when domain numbers mismatch
- [AssociationID field not always accurately displayed](https://github.com/christophe-calmejane/Hive/issues/45)

## [1.2.4] - 2021-04-02
### Added
- New CLI tool to dump AEM from entities: AEMDumper
- Support for Control Descriptors (only Linear Values for now)
- Support for Controller to Entity Identification (right-click an entity in the list)
- [Added a daemon on macOS to setup pcap access rights for non-root execution](https://github.com/christophe-calmejane/Hive/issues/50)
- Automatic view scrolling when dragging a channel mapping near the edges of the window (Dynamic Mappings Editor View)

### Changed
- macOS minimum version is now 10.12 (due to Qt update)
- Using macOS PKG installer instead of simple DMG
  - **WARNING**: When upgrading from a DMG version of Hive, you will have to manually close and erase the previous version

### Fixed
- Slightly improved Firmware Update Dialog
- Application not properly closing during self-update on windows
- Application not properly restarting after self-update on windows
- [AVB domain incompatibility checks for gPTP domain number](https://github.com/christophe-calmejane/Hive/issues/98)
- [Using a timestamped filename when saving the log file](https://github.com/christophe-calmejane/Hive/issues/99)
- Added a popup error message if the pcap driver is not properly installed
- [Changing more than 63 mappings at the same time doesn't cause an error](https://github.com/christophe-calmejane/Hive/issues/95)

## [1.2.3] - 2020-09-14
### Added
- [Displaying a message with shell command to run, if network interface cannot be opened](https://github.com/christophe-calmejane/Hive/issues/88)
- Error message when a critical error occurs on the active network interface
- [Configurable automatic entities discovery delay](https://github.com/christophe-calmejane/Hive/issues/87)
- [Scroll logger view to selected item whenever the filter changes](https://github.com/christophe-calmejane/Hive/issues/92)

### Changed
- No longer clearing talker mapping when the removing the last (CBR matrix) channel connection
- Using AVDECC Library v3.0.2

### Fixed
- [CBR matrix not refreshing](https://github.com/christophe-calmejane/Hive/issues/89)
- [Deadlock on linux when shutting down pcap interface](https://github.com/christophe-calmejane/Hive/issues/90)

## [1.2.2] - 2020-05-25
### Added
- Legal notices for each third party resource
- [Possibility to disconnect an Input Stream connected to an offline talker](https://github.com/christophe-calmejane/Hive/issues/6)
- [Highlighted currently selected item in comboBox](https://github.com/christophe-calmejane/Hive/issues/82)
- [Possibility to edit dynamic mappings from the connection matrix headers context menu](https://github.com/christophe-calmejane/Hive/issues/85)

### Changed
- Small rework of About Dialog
- Using AVDECC Library v3.0.1
- macOS Native is now restricted to macOS Catalina and later

### Fixed
- Full Entity State loading issue (connection state was not properly loaded)
- [Crashed when using macOS Native ProtocolInterface](https://github.com/christophe-calmejane/Hive/issues/76)
- USB-C and Thunderbolt Network interfaces enumeration issues on macOS Catalina

## [1.2.1] - 2019-11-21
### Fixed
- Windows updater not ignoring winPcap reinstallation

## [1.2.0] - 2019-11-21
### Added
- Detection of arriving and departing network interfaces (and link status)
- [Collapse/Expand all buttons for connection matrix](https://github.com/christophe-calmejane/Hive/issues/51)
- [Sort connection matrix by EntityID](https://github.com/christophe-calmejane/Hive/issues/59)
- Major performance improvements
- Smart connections in Connection Matrix
- Support for drag&drop of json virtual entity files
- Color theme selection in Settings
- Controller Statistics (displayed in Entity Node)
- Utilities toolbar for quick access to Media Clock Management and Settings
- [WinPcap included in Windows Installer](https://github.com/christophe-calmejane/Hive/issues/14)
- Device Firmware Update multi-selection window
- Channel Based Routing (Alternate Connection Matrix), with CTRL-M shortcut to switch routing modes
- Main Menu shortcuts
- [_Error Counters_ now display the count since last acknowledge](https://github.com/christophe-calmejane/Hive/issues/62)
- [_Statistics Error Counters_ are displayed as errors](https://github.com/christophe-calmejane/Hive/issues/62)
- A few options to visually configure the connection matrix arrows
- [Display of StreamOutput _Streaming State_](https://github.com/christophe-calmejane/Hive/issues/66)
- [Display of SteramInput _Media Locked State_](https://github.com/christophe-calmejane/Hive/issues/66)
- Displaying the currently active ClockSource in the AEM Tree
- Considering MediaUnlocked StreamInput counter changes as errors (only when the stream is connected)
- [Detection for WinPcap driver to be installed and started](https://github.com/christophe-calmejane/Hive/issues/69)
- [Grey out stream input counters if there is no connection](https://github.com/christophe-calmejane/Hive/issues/57)
- Option to export the json EntityModel of a device (.aem file)
- Option to enumerate and display the full Static Model

### Changed
- Always reselecting the last selected Descriptor when reinspecting an Entity
- [Moved ProtocolInterface selection to the Settings](https://github.com/christophe-calmejane/Hive/issues/58)
- Automatically Locking the Entity when opening the Dynamic Mappings Editor
- All exported files are now using MessagePack (JSON binary) file format
- Using SHIFT modifier while choosing one of the Export feature will dump the file in readable JSON format
- Improved software update with automatic download and install
- Entities in a Full Network State dump are always sorted by descending EntityID

### Fixed
- [Splashscreen displayed on the same screen than Hive will be shown](https://github.com/christophe-calmejane/Hive/issues/20)
- Refresh issue when gPTP changes for some non-milan devices
- [Red text no longer applied when item is selected (fixed by using a colored box around the item)](https://github.com/christophe-calmejane/Hive/issues/68)
- [Possible crash when failed to properly enumerate a device](https://github.com/christophe-calmejane/Hive/issues/71)
- [_Arrow Settings_ immediately refresh the Connection Matrix when changed from the Settings Window](https://github.com/christophe-calmejane/Hive/issues/72)
- [WrongFormat error now has priority over InterfaceDown in Redundant Stream Pair Summary](https://github.com/christophe-calmejane/Hive/issues/73)
- _Device Details Dialog_ Receive/Transmit tab refresh issues

## [1.1.0] - 2019-05-21
### Added
- _Device Details Dialog_ for basic device configuration and information
- _Media Clock Master ID_ and _Media Clock Master Name_ columns in entity list
- _Media Clock Management Dialog_ for simple media clock distribution setup
- Keyboard shortcut to refresh the controller (CTRL-R)
- Button to refresh the controller (next to the Interface selection dropdown)
- Entity Identify notifications (from entity to controller)
- Entity and Full Network export as readable json
- Strings descriptor displayed
- Entity descriptor counters

### Changed
- Only displaying _Ethernet_ kind interfaces
- Displaying the type of ethernet adapter on macOS

### Fixed
- Possible deadlock when trying to match stream formats
- [Random crash during application exit](https://github.com/christophe-calmejane/Hive/issues/56)
- StreamInput counters not properly displayed (as sub-nodes of the Counter node)
- Possible crash in EntityModelInspector when an entity goes offline/online again

## [1.0.11] - 2019-02-14
### Added
- Numerical values for _StreamFlags_, _StreamFlagsEx_, _ProbingStatus_ and _AcmpStatus_
- [Highlighting entities that have increments in error counters](https://github.com/christophe-calmejane/Hive/issues/31)

### Fixed
- Milan Certification version properly displayed (as x.y.z.w value)
- Only flagging as Milan, devices with protocol_version 1
- Correctly restoring collapsed streams in connection matrix, when expanding an entity

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
- Support for Memory Object descriptors
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
