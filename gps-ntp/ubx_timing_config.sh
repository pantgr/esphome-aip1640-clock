#!/bin/sh
# u-blox NEO-M8N timing configuration (one-shot; the module keeps it in BBR/EEPROM after SAVE).
# Source: u-blox M8 Receiver Description UBX-13003221:
#   CFG-NAV5 dynModel 2 = stationary «used in timing applications» (p21, p217)
#   CFG-PRT UART1 rates up to 460800, default 9600 (p36, p243, p453)
#   Galileo on: CFG-GNSS, then CFG-CFG save, then CFG-RST HARDWARE reset (p207-209)
#   Time pulse §19.2 p72: disable SBAS for timing
#   CFG-TP5 defaults (p456): no pulse until locked, 100 ms when locked, rising edge, UTC grid -> left as is.
# ubxtool (gpsd 3.22): -P 18 (M8 protocol; the default is 10), -f tty direct, -s local speed,
#   -S receiver speed, -e/-d GNSS, -p MODEL,2, -p SAVE, -c raw cls,id,payload.
# Result here: satellites used 7 -> 12, PPS std dev ~1.4 us -> 95 ns.
# After running it, re-calibrate the NMEA offset in gps_ntp_install.sh (the UART lag changes with speed).
set -x
U="ubxtool -P 18 -f /dev/ttyAMA0"

# gpsd must not hold the port meanwhile.
systemctl stop gpsd.socket gpsd.service

$U -s 9600 -d SBAS
$U -s 9600 -e GALILEO
$U -s 9600 -p MODEL,2
$U -s 9600 -S 115200
$U -s 115200 -p SAVE
# UBX-CFG-RST: navBbrMask 0x0000 (hot start, keep ephemeris), resetMode 0x00 = hardware reset.
$U -s 115200 -c 0x06,0x04,0x00,0x00,0x00,0x00 || true

# gpsd fixed at the new speed (no autobaud at boot).
sed -i 's/^GPSD_OPTIONS=.*/GPSD_OPTIONS="-n -s 115200"/' /etc/default/gpsd
grep GPSD_OPTIONS /etc/default/gpsd

# Read back what the module now holds.
$U -s 115200 -p CFG-PRT,1 2>&1 | grep -iE 'baud|UBX-CFG-PRT' | head -4
$U -s 115200 -p CFG-NAV5 2>&1 | grep -iE 'dynModel' | head -2
$U -s 115200 -p CFG-GNSS 2>&1 | grep -iE 'gnssId|enable|flags' | head -12

systemctl start gpsd.socket gpsd.service
systemctl is-active gpsd.socket gpsd.service
