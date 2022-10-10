# BlazeNet Coordinator Software
This repo contains a bunch of software for the BlazeNet coordinator.

## coordinatord
A daemon to run on the host that has radio hardware attached, which in turn should serve as the central coordinator node of the network. It's responsible for handling higher level protocol stuff, and keeping track of connected clients. Additionally, it implements various security features.

It's intended to interface to a radio via a local interface such as UART, SPI, or over USB. Various pluggable communication backends can be selected via configuration.

Any user data packets are provided on a tap interface, to be handled by the host's IP stack.

## webui
A PHP-based web interface to manage the coordinator. It interfaces with the configuration storage and coordinatord to allow changing of various network settings, and shows the current state of the network (as well as historical records, if kept) as well.

### Honorable Mentions
If you're looking for the coordinator host radio firmware, it's with the other BlazeNet device firmwares: [see this repo](https://github.com/tristanseifert/blazenet-device-fw/tree/main/host-radio) for the source and more information.
