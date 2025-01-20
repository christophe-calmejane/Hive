# Hive

Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

## What is Hive

Hive is a pro audio Avdecc (IEEE Std 1722.1) controller. Hive allows you to inspect, configure and connect AVB Entities on your network, specifically targeting AVnu Milan compatible devices (but not only).

## Precompiled binaries

Precompiled binaries for macOS and Windows [can be found here](https://github.com/christophe-calmejane/Hive/releases).

**Note**: Starting with Hive 1.3, the precompiled Mac binaries require macOS 11 or later to run (both Intel and Apple Silicon).

## Minimum requirements for compilation

- CMake 3.29
- Qt 6.5.2 (although Qt 5.15.2 was supported in the past, it's no longer guaranteed to compile correctly)
- Visual Studio 2022 17.4 (using platform toolset v143), Xcode 14, g++ 11.0
- [Optional, for cross-compilation] Docker / Docker Compose

## Compilation

- Check and install [la_avdecc compilation requirements](https://github.com/L-Acoustics/avdecc/blob/master/README.md) for your system
- Clone this repository
- Copy `.hive_config.sample` to `.hive_config`, then edit it for installer customization
- Run the `setup_fresh_env.sh` script that should properly setup your working copy
- Run the `gen_cmake.sh` script with whatever optional parameters required (run *gen_cmake.sh -h* to display the help)
  - [Linux only] For Ubuntu users, install the `qtbase5-dev` package and make sure the major and minor version matches what Hive requires. You can alternatively use the `-qtvers` and `-qtdir` options when invoking `gen_cmake.sh` if you want to use a different Qt version.
- Go into the generated output folder
- Compile everything
  - [macOS/Windows] Open the generated solution and compile from the IDE
  - [Linux] Run `cmake --build . --config Release`

## Cross-compilation using Docker

- Requires `docker` and `docker-compose` to be installed
- Go to the `Docker` folder
- Build the docker builder image: _docker-compose build_
- Generate the build solution: _docker-compose run --rm gen_cmake -debug -c Ninja -qtvers 6.5.2 -qtdir /usr/local/Qt-6.5.2/lib/cmake -- -DBUILD_HIVE_TESTS=FALSE_
 - You may change parameters to your convenience
- Build the solution: _docker-compose run --rm build --target install_
- If you want to run the application, you must authorize X connections to your display: _xhost local:root_
- You can then run the application from the docker container: _APP=Hive-d docker-compose run -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix --rm run_
- Windows users need to have a running X Server:
  - Install [VcXsrv](http://vcxsrv.sourceforge.net)
  - Start it with _access control disabled_
  - Find the IP address of your WSL network interface using _ipconfig_
  - Set a DISPLAY environment variable with value _WSL\_Interface\_IP_:0
- macOS users need to have a running XQuartz:
  - Install [XQuartz](https://www.xquartz.org)
  - Start it and make sure it's not running with the _Allow connections from network clients_ option (XQuartz -> Preferences -> Security)
  - Change the above command line to _APP=Hive-d docker-compose run -e DISPLAY=docker.for.mac.host.internal:0 --rm run_

## Installer generation

- Run the `gen_install.sh` script on either Windows or macOS (not supported on Linux yet)

### MacOS notarization

If you want to generate a proper installer that can be distributed (outside the AppStore), you need to notarize the installer. The `gen_install.sh` script can do this for you if you define `notarization_profile` in the config file.

You need to create a notarization profile that will be saved in your keychain (just once). For that you'll need your Apple team identifier, Apple account ID, and to generate an application-password (you cannot use your Apple ID account password for security reasons). To generate an application-password, do the following:
- Sign in to your [Apple ID account page](https://appleid.apple.com/account/home) (https://appleid.apple.com/account/home)
- In the Security section, click Generate Password below App-Specific Passwords
- Follow the steps on your screen

To save a profile in your keychain, do the following:
- `xcrun notarytool store-credentials YourProfileName --apple-id YourAccountEmailAdrs --password YourAppSpecificPwd --team-id YourTeamID`

## MacOS runtime specificities

Before running Hive on a macOS system, you must install `Install ChmodBPF.pkg` which can be found in `/Applications/Hive <Version>/`. If you have previously installed [Wireshark](https://www.wireshark.org) or [LANetworkManager](https://www.l-acoustics.com), then you don't need this step.

## Linux runtime specificities

Before running Hive on a linux system, you must give the program access to RAW SOCKETS creation. The easiest way to do it is to run the following command (replace `/path/to/Hive` with the actual path to the binary):
```bash
sudo setcap cap_net_raw+ep /path/to/Hive
```

## Versioning

We use [SemVer](http://semver.org/) for versioning.

## License

See the [COPYING](COPYING.txt) and [COPYING.LESSER](COPYING.LESSER.txt) files for details.

## Contributing code

[Please read this file](CONTRIBUTING.md)

## Third party

Hive uses the following 3rd party resources:
- [L-Acoustics (open source) Avdecc libraries](https://github.com/L-Acoustics/avdecc)
- [Qt](https://www.qt.io)
- [Material Icons](https://material.io/icons/)
- [Discount Markdown](http://www.pell.portland.or.us/~orc/Code/markdown/)
- [BugTrap](https://github.com/bchavez/BugTrap)
- [Sparkle](https://sparkle-project.org) and [WinSparkle](https://github.com/vslavik/winsparkle)
- ChmodBPF macOS script from [Wireshark](https://www.wireshark.org)
