# How to setup a webserver for Hive to auto-update itself

## One time setup
- Setup a webserver with support for php7 or later and https. *BaseURL* refers to the full https path to where Hive files will be accessed.
- Copy [changelog.php](changelog.php) and [Parsedown.php](Parsedown.php) files to *BaseURL*
- Copy [appcast-release.xml](appcast-release.xml) and [appcast-beta.xml](appcast-beta.xml) files to *BaseURL*
- Create the following subfolders: *BaseURL*/release and *BaseURL*/beta

## Steps for each release
- Copy the newly generated Hive installer to either *BaseURL*/release/ or *BaseURL*/beta/ (depending on the release type)
- Update either appcast-release.xml or appcast-beta.xml from the webserver (depending on the release type), with the content of the generated appcastItem-*.xml during gen_install.sh (always put the most recent item first, and always put both release and beta items in the beta appcast)
- Upload (override) the CHANGELOG.md file to *BaseURL*/CHANGELOG.md
