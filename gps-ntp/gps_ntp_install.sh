#!/bin/sh
# Raspberry Pi + u-blox NEO-M8N -> GPS-disciplined NTP server for the LAN (stratum 1).
# Why: the clocks keep the right time even with the internet down.
# Before this script (part 1): serial console off ttyAMA0 (cmdline.txt + serial-getty),
#   /boot config: dtoverlay=pps-gpio,gpiopin=18, then: apt install gpsd gpsd-clients chrony pps-tools.
# Wiring used: NEO-M8N carrier VCC = Pi 5 V (pin 2), GND pin 6, GPS TX -> Pi RX (pin 10),
#   GPS RX -> Pi TX (pin 8), TIMEPULSE (PPS) -> GPIO18 (pin 12).
# How: gpsd reads the receiver and feeds chrony NMEA via SHM 0 (coarse, only numbers the seconds);
#   chrony reads the PPS edge itself from /dev/pps0 and locks it to NMEA (fine).
# Docs: https://gpsd.io/gpsd-time-service-howto.html , https://chrony-project.org/doc/4.3/chrony.conf.html
# NMEA offset: calibrate it on YOUR setup once PPS is locked (chronyc sourcestats). Measured here:
#   0.56 s at 9600 baud, 0.05 s after ubx_timing_config.sh moved the UART to 115200. A wrong offset
#   pushes NMEA outside the +/-0.5 s PPS lock window and PPS stops locking.
# Idempotent. Reboot afterwards for /dev/pps0 (the overlay).
set -e

LAN_SUBNET="192.168.1.0/24"   # who may query this NTP server
NMEA_OFFSET="0.05"            # see above

[ -f /etc/default/gpsd.bak ] || cp /etc/default/gpsd /etc/default/gpsd.bak
cat > /etc/default/gpsd <<'EOF'
# GPS time server: -n polls the receiver even with no clients (chrony needs SHM fed all the time).
DEVICES="/dev/ttyAMA0"
GPSD_OPTIONS="-n -s 115200"
USBAUTO="false"
EOF

mkdir -p /etc/chrony/conf.d
cat > /etc/chrony/conf.d/gps.conf <<EOF
# NMEA from gpsd (SHM 0): only numbers the seconds for PPS, never selected on its own.
refclock SHM 0 refid NMEA offset ${NMEA_OFFSET} delay 0.2 noselect
# PPS from the NEO-M8N TIMEPULSE on GPIO18 (rising edge = top of the UTC second).
refclock PPS /dev/pps0 lock NMEA refid PPS prefer
# Serve the LAN; keep serving (stratum 10) if GPS and WAN are both gone.
allow ${LAN_SUBNET}
local stratum 10
EOF

# With IPv6 off, the shipped gpsd.socket also listens on [::1]:2947, fails with
# 'Cannot assign requested address' and takes gpsd.service down with it.
# An empty ListenStream= resets the list (man systemd.socket).
mkdir -p /etc/systemd/system/gpsd.socket.d
cat > /etc/systemd/system/gpsd.socket.d/no-ipv6.conf <<'EOF'
[Socket]
ListenStream=
ListenStream=/run/gpsd.sock
ListenStream=127.0.0.1:2947
EOF
systemctl daemon-reload

systemctl enable gpsd.service
systemctl disable --now systemd-timesyncd.service 2>/dev/null || true

echo "--- installed ---"
cat /etc/default/gpsd
chronyd -p -f /etc/chrony/chrony.conf | grep -E 'refclock|allow|local' || true
systemctl is-enabled gpsd.service chrony.service
echo "Reboot, then check: chronyc -n sources  (want '#* PPS'), chronyc tracking (Stratum 1)"
