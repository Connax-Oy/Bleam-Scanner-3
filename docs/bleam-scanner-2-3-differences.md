Bleam Scanner 3 is an upgraded and refined version of [Bleam Scanner 2](https://github.com/Connax-Oy/Bleam-Scanner-2).

To get the gist of what does a Bleam Scanner do and how, please refer to [Bleam Scanner 2's wiki pages](https://github.com/Connax-Oy/Bleam-Scanner-2/wiki).

Here are important differences between the two projects:

## Asymmetric cryptography

While Bleam Scanner 2 used a common key for HMAC signature,
Bleam Scanner 3 uses elliptic curve crypthography.
Instead of receiving a project-wide key at configuration stage,
Bleam Scanner 3 exchanges public keys with the configurating device.

This assures stronger security of the system.

## nRF51 board support

Bleam Scanner 3 has been developed with intention to be compatible with
some nRF51 boards, which are still very popular despite being outdated.
The nRF51822 DK compatible implementation can be easily tweaked to work
on nRF51422.

The IBKS Plus implementation is a lightweight Bleam Scanner 3
for lower memory usage and longer battery life.

## Isolated modules

The source code has been restructured for modularity and convinience,
allowing for easier multi-platform approach.

## Device type identification, scan filtering and Bleam Tools interaction

Bleam Scanner 3 is able to differentiate between Bleam devices types:
Bleam Service full UUID carries info on the device platform and
the Bleam source, which allows refining scanned data to only the
most important (for example, by limiting the RSSI levels)
while still being able to be accessed by Bleam Tools
application at any point.

Bleam Tools action commands have been expanded to edit RSSI level limit
and useful system setup debug tools such as Idle command, which sends
the Bleam Scanner node to sleep for a set amount of time.

## Refined iOS problem solution

Bleam Scanner 3 no longer identifies iOS devices with background services
by a dynamic MAC, insteam it goes by advertised backgroupnd services thumbprint
that isn't prone to change as often.

Updated Bleam Service client allows for Bleam Scanner 3 to identify itself
to an iOS device.

Custom service discovery module has been refined and debugged.

