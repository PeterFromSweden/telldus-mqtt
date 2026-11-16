#!/bin/sh
# Test the uci-defaults script and start_service of the init script of
# packaging/openwrt/package without OpenWrt. The paths in the scripts are changed to
# a temp directory. procd, logger, jsonfilter and telldusd are stubs.
# Needs ucode with the uci and fs modules, and the uci program.
# Exit 77 (skipped) if one is missing.

dir=$(cd "$(dirname "$0")/.." && pwd)
ucode=${UCODE:-$(command -v ucode)}
uci=${UCI:-$(command -v uci)}

if [ -z "$ucode" ] || [ -z "$uci" ] ||
   ! "$ucode" -e 'import { cursor } from "uci"; import * as fs from "fs";' >/dev/null 2>&1; then
	echo "ucode with the uci and fs modules, or uci, not found => skip"
	exit 77
fi

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT
root=$tmp/root
failed=0

fail() {
	echo "FAIL $*"
	failed=$((failed + 1))
}

# Stubs
mkdir "$tmp/bin"
ln -s "$ucode" "$tmp/bin/ucode"
cat > "$tmp/bin/logger" <<EOF
#!/bin/sh
cat >> "$tmp/log"
EOF
cat > "$tmp/bin/jsonfilter" <<'EOF'
#!/bin/sh
# Only: jsonfilter -i <file> -e @.gateway.serial
[ -f "$2" ] || exit 1
F="$2" exec ucode -e 'import { readfile } from "fs"; let s = json(readfile(getenv("F"))).gateway?.serial; if (s) print(s, "\n"); exit(s ? 0 : 1);'
EOF
chmod +x "$tmp/bin/logger" "$tmp/bin/jsonfilter"
PATH="$tmp/bin:$PATH"
export PATH

# The package scripts with the paths in the temp directory
for f in defaults init; do
	sed -e "s,/usr/share/telldus-mqtt,$dir,g" -e "s,\([^a-z]\)/\(etc\|var\)/,\1$root/\2/,g" \
		"$dir/package/files/telldus-mqtt.$f" > "$tmp/$f"
done

uciget() {
	"$uci" -c "$root/etc/config" -q get "telldus-mqtt.$1"
}

# A new root: package files, tellstick.conf with switch ids 1 and 2
new_root() {
	rm -rf "$root" "$tmp/log" "$tmp/procd" "$tmp/telldusd"
	mkdir -p "$root/etc/config" "$root/etc/telldus-mqtt" "$root/etc/init.d" "$root/var/run"
	cp "$dir/package/files/telldus-mqtt.config" "$root/etc/config/telldus-mqtt"
	cp "$dir/../../telldus-mqtt.json" "$root/etc/telldus-mqtt/"
	cat > "$root/etc/init.d/telldusd" <<-EOF
	#!/bin/sh
	echo "\$1" >> "$tmp/telldusd"
	EOF
	chmod +x "$root/etc/init.d/telldusd"
	cat > "$root/etc/tellstick.conf" <<-'EOF'
	user = "root"
	group = "root"
	device {
	  id = 1
	  name = "Lamp"
	  protocol = "arctech"
	  model = "selflearning-switch:nexa"
	  parameters {
	    house = "30341302"
	    unit = "1"
	  }
	}
	device {
	  id = 2
	  name = "Desk"
	  protocol = "arctech"
	  model = "selflearning-switch:nexa"
	  parameters {
	    house = "30341302"
	    unit = "2"
	  }
	}
	EOF
}

run_defaults() {
	(cd "$root/etc" && . "$tmp/defaults") > "$tmp/defaults.out" 2>&1 || fail "uci-defaults exit code not 0"
}

# Run start_service of the init script with procd stubs
run_start() {
	rm -f "$tmp/procd"
	(
		config_load() { :; }
		config_get_bool() {
			local v
			v=$(uciget "$2.$3") || v=$4
			case "$v" in 1|on|true|yes|enabled) v=1;; *) v=0;; esac
			eval "$1=$v"
		}
		procd_open_instance() { echo open >> "$tmp/procd"; }
		procd_set_param() { echo "$*" >> "$tmp/procd"; }
		procd_append_param() { echo "$*" >> "$tmp/procd"; }
		procd_close_instance() { echo close >> "$tmp/procd"; }
		. "$tmp/init"
		start_service
	)
}

# 1. New install: the import works without the serial
new_root
run_defaults
[ "$(uciget main.manage_switches)" = 1 ] || fail "new install: manage_switches not 1"
[ -f "$root/etc/telldus-mqtt/tellstick-import-pending" ] && fail "new install: import pending"
[ "$(uciget @device[1].house)" = 30341302 ] || fail "new install: switch 2 not imported"
cp "$root/etc/config/telldus-mqtt" "$tmp/uci.1"
cp "$root/etc/tellstick.conf" "$tmp/tellstick.1"
run_defaults
cmp -s "$root/etc/config/telldus-mqtt" "$tmp/uci.1" || fail "new install: second run changed UCI"

run_start
grep -q "^command /usr/bin/telldus-mqtt$" "$tmp/procd" || fail "start: no command"
grep -q "^command --config $root/var/etc/telldus-mqtt.json --status $root/var/run/telldus-mqtt.json$" "$tmp/procd" ||
	fail "start: no --config, --status"
grep -q "^close$" "$tmp/procd" || fail "start: no instance"
[ "$(stat -c %a "$root/var/etc/telldus-mqtt.json")" = 600 ] || fail "start: config not mode 600"
cmp -s "$root/etc/tellstick.conf" "$tmp/tellstick.1" || fail "start: tellstick.conf changed"
[ -f "$tmp/telldusd" ] && fail "start: telldusd restarted, tellstick.conf did not change"

# A new switch in UCI: tellstick.conf changes, telldusd restarts
"$uci" -c "$root/etc/config" batch >/dev/null <<-EOF
	add telldus-mqtt device
	set telldus-mqtt.@device[-1].type=switch
	set telldus-mqtt.@device[-1].id=3
	set telldus-mqtt.@device[-1].name=Hall
	set telldus-mqtt.@device[-1].protocol=arctech
	set telldus-mqtt.@device[-1].model=selflearning-switch:nexa
	set telldus-mqtt.@device[-1].house=30341302
	set telldus-mqtt.@device[-1].unit=3
	commit telldus-mqtt
EOF
run_start
grep -q 'name = "Hall"' "$root/etc/tellstick.conf" || grep -q 'name="Hall"' "$root/etc/tellstick.conf" ||
	fail "new switch: not in tellstick.conf"
[ "$(cat "$tmp/telldusd" 2>/dev/null)" = restart ] || fail "new switch: telldusd not restarted"

# enabled 0: no instance
"$uci" -c "$root/etc/config" set telldus-mqtt.main.enabled=0
"$uci" -c "$root/etc/config" commit telldus-mqtt
run_start
[ -f "$tmp/procd" ] && fail "enabled 0: instance started"

# 2. Upgrade from 0.4: an old JSON with two TellSticks and the same switch id
new_root
cat > "$root/etc/telldus-mqtt/telldus-mqtt.json" <<'EOF'
{
  "host": "192.168.1.2",
  "port": 1883,
  "topic-translation": [
    { "telldus": "telldus/AAAA0001/switch/2", "mqtt": "Home/Desk", "name": "Desk" },
    { "telldus": "telldus/BBBB0002/switch/2", "mqtt": "Cabin/Desk", "name": "Cabin desk" },
    { "telldus": "telldus/AAAA0001/sensor/fineoffset_temperaturehumidity_135", "mqtt": "Home/Out", "name": "Out" }
  ]
}
EOF
run_defaults
[ "$(uciget main.host)" = 192.168.1.2 ] || fail "upgrade: JSON not migrated"
[ -z "$(uciget main.manage_switches)" ] || fail "upgrade: manage_switches set without serial"
[ -f "$root/etc/telldus-mqtt/tellstick-import-pending" ] || fail "upgrade: import not pending"
cp "$root/etc/config/telldus-mqtt" "$tmp/uci.2"
run_defaults
cmp -s "$root/etc/config/telldus-mqtt" "$tmp/uci.2" || fail "upgrade: second run changed UCI"

# Start without a status file: the import waits, the daemon starts
run_start
grep -q "^close$" "$tmp/procd" || fail "upgrade start: no instance"
[ -f "$root/etc/telldus-mqtt/tellstick-import-pending" ] || fail "upgrade start: import not pending"
grep -q '"Home/Desk"' "$root/var/etc/telldus-mqtt.json" || fail "upgrade start: topic not in config"

# Start (reload) after the daemon wrote the serial: the import runs
echo '{ "gateway": { "found": true, "type": "TellStick Duo", "serial": "AAAA0001" } }' > "$root/var/run/telldus-mqtt.json"
run_start
[ -f "$root/etc/telldus-mqtt/tellstick-import-pending" ] && fail "reload: import still pending"
[ "$(uciget main.manage_switches)" = 1 ] || fail "reload: manage_switches not 1"
cmp -s "$root/etc/tellstick.conf" "$tmp/tellstick.1" || fail "reload: tellstick.conf changed"
[ -f "$tmp/telldusd" ] && fail "reload: telldusd restarted"
grep -q "devices imported" "$tmp/log" || fail "reload: import not logged"
[ -f "$root/etc/telldus-mqtt/telldus-mqtt.json" ] || fail "reload: old JSON deleted"

# Generator error: not started
rm -f "$root/var/etc/telldus-mqtt.json"
mkdir "$root/var/etc/telldus-mqtt.json"
run_start && fail "generator error: exit code 0"
[ -f "$tmp/procd" ] && fail "generator error: instance started"
grep -q "not started" "$tmp/log" || fail "generator error: not logged"

if [ $failed -ne 0 ]; then
	echo "$failed failed. Log:"
	cat "$tmp/log" "$tmp/defaults.out" 2>/dev/null
	exit 1
fi
echo "package scripts: all passed"
