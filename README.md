# Hive

Copyright (C) 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

## What is Hive

Hive is a pro audio Avdecc (IEEE Std 1722.1) controller. Hive allows you to inspect, configure and connect AVB Entities on your network, specifically targeting AVnu certified devices.

## Precompiled binaries

Precompiled binaries for macOS and Windows [can be found here](http://www.kikisoft.com/Hive).

## Minimum requirements for compilation

- CMake 3.10
- Qt 5.10.1 (including QtWebEngine)
- Visual Studio 2017 15.7.0, Xcode 9.2, or any c++17 compliant compiler

## Compilation

- Clone this repository
- Update submodules: *git submodule update --init*
- Follow LA_avdecc [installation instructions](https://github.com/L-Acoustics/avdecc/blob/master/README.md)
- Run the gen_cmake.sh script with whatever optional parameters required (run *gen_cmake.sh -h* to display the help)
- Go into the generated output folder
- Open the generated solution
- Compile everything
- Compile the *INSTALL* target

## Versioning

We use [SemVer](http://semver.org/) for versioning.

## License

See the [COPYING](COPYING) and [COPYING.LESSER](COPYING.LESSER) files for details.

## Third party

Hive uses the following 3rd party resources:
- [L-Acoustics (open source) Avdecc libraries](https://github.com/L-Acoustics/avdecc).
- [Qt](https://www.qt.io)
- [Material Icons](https://material.io/icons/)
- [Marked JS](https://github.com/markedjs/marked)
- [BugTrap](https://github.com/bchavez/BugTrap)
