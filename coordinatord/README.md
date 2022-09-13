# BlazeNet Coordinator
Daemon to run on the host (blazed) that implements the upper layers of the BlazeNet protocol, as [partially documented here.](https://wiki.trist.network/books/blazenet)

## Dependencies
At a minimum, the following libraries _must_ be present on the system, and findable by CMake:

- SQLite3
- libevent2
- OpenSSL (at least 3.0.5)

Additionally, the following libraries are also used by the coordinator software, but may be automagically acquired by the build process:

- glog
- libfmt
- toml++

See the `EXPECT_DEPENDENCIES_LOCAL` build option to control the automatic library fetching behavior. (At this time, this is all-or-nothing: it's not possible to collect some libraries from the system, and fetch the remainder.)

### External Applications
The only external application dependency of the coordinator daemon is confd (from the [programmable load](https://github.com/tristanseifert/meta-programmable-load) project) to store some run-time configuration about the network, such as RF channel/power levels, network name and security settings.

## Configuration
Most of the daemon is configured via a simple TOML-formatted file.

**TODO: add config file example**

Some of the configuration is instead dynamically retrieved from `confd` at start-up to allow for easy updating by administrative tools. (Dynamically reloading configuration is not yet supported.) These configuration options are referred to as "external" options.
