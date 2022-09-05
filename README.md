# Hive

Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

## What is Hive

Hive is a pro audio Avdecc (IEEE Std 1722.1) controller. Hive allows you to inspect, configure and connect AVB Entities on your network, specifically targeting AVnu Milan compatible devices (but not only).

## Precompiled binaries

Precompiled binaries for macOS and Windows [can be found here](https://github.com/christophe-calmejane/Hive/releases).

## Minimum requirements for compilation

- CMake 3.22
- Qt 5.15.2
- Visual Studio 2019 16.3 (using platform toolset v142), Xcode 12, g++ 11.2.0
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
- Generate the build solution: _docker-compose run --rm gen_cmake -debug -c Ninja -- -DBUILD_HIVE_TESTS=FALSE_
 - You may change parameters to your convenience
- Build the solution: _docker-compose run --rm build --target install_
- If you want to run the application, you must authorize X connections to your display: _xhost local:root_
- You can then run the application from the docker container: _APP=Hive-d docker-compose run -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix --rm run_

## Installer generation

- Run the `gen_install.sh` script on either Windows or macOS (not supported on Linux yet)

### MacOS notarization

If you want to generate a proper installer that can be distributed (outside the AppStore), you need to notarize the installer. The `gen_install.sh` script can do this for you if you define `notarization_username` and `notarization_password`.
Note that `notarization_password` can be omitted if you save the password in your keychain.

You can only use an application-password, **not your Apple ID account password**. To generate an application-password, do the following:
- Sign in to your [Apple ID account page](https://appleid.apple.com/account/home) (https://appleid.apple.com/account/home)
- In the Security section, click Generate Password below App-Specific Passwords
- Follow the steps on your screen

To save the password in your keychain, do the following. It is strongly suggested to specify the Login Keychain with `--keychain` (use `security list-keychains` to get your Login Keychain filepath):
- `xcrun altool --store-password-in-keychain-item "AC_PASSWORD" -u AccountEmailAdrs -p AppSpecificPwd --keychain LoginKeychainPath`

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
