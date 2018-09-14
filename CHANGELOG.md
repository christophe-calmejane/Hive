# Hive Changelog
All notable changes to Hive will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Changed
- Updated la_avdecc to v2.7.0

### Fixed
- [Image/Logo possible invalid size](https://github.com/christophe-calmejane/Hive/issues/29)
- [Image/Logo background garbage](https://github.com/christophe-calmejane/Hive/issues/26)
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
