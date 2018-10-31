# Hive

Copyright (C) 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

## What is Hive

Hive is a pro audio Avdecc (IEEE Std 1722.1) controller. Hive allows you to inspect, configure and connect AVB Entities on your network, specifically targeting AVnu Milan compatible devices (but not only).

## Precompiled binaries

Precompiled binaries for macOS and Windows [can be found here](http://www.kikisoft.com/Hive).

## Minimum requirements for compilation

- CMake 3.12
- Qt 5.11.2
- Visual Studio 2017 15.8, Xcode 10, or any c++17 compliant compiler

## Compilation

- Clone this repository
- Run the setup_fresh_env.sh script that should properly setup your working copy
- Run the gen_cmake.sh script with whatever optional parameters required (run *gen_cmake.sh -h* to display the help)
- Go into the generated output folder
- Open the generated solution
- Compile everything
- Compile the *INSTALL* target

## Versioning

We use [SemVer](http://semver.org/) for versioning.

## License

See the [COPYING](COPYING) and [COPYING.LESSER](COPYING.LESSER) files for details.

## Contributing code

[Please read this file](CONTRIBUTING.md)

## Third party

Hive uses the following 3rd party resources:
- [L-Acoustics (open source) Avdecc libraries](https://github.com/L-Acoustics/avdecc).
- [Qt](https://www.qt.io)
- [Material Icons](https://material.io/icons/)
- [Discount Markdown](http://www.pell.portland.or.us/~orc/Code/markdown/)
- [BugTrap](https://github.com/bchavez/BugTrap)
