// Tests of the generator and the migration. run-tests.sh runs this file.
// Usage: ucode test.uc <openwrt dir> <empty temp dir>

'use strict';

import { cursor } from 'uci';
import * as fs from 'fs';

const dir = ARGV[0];
const tmp = ARGV[1];
const generator = dir + '/telldus-mqtt-uci2json.uc';
const migration = dir + '/telldus-mqtt-json2uci.uc';
let failed = 0;

function check(ok, what) {
	if (!ok) {
		warn('FAIL ' + what + '\n');
		failed++;
	}
}

function same(a, b, what) {
	let ja = sprintf('%J', a);
	let jb = sprintf('%J', b);
	check(ja == jb, what + '\n  got:      ' + ja + '\n  expected: ' + jb);
}

function quote(s) {
	return "'" + replace(s, "'", "'\\''") + "'";
}

// Run a script as on the router. Returns the exit code, sets out.stderr.
function run(script, args, out) {
	let errfile = tmp + '/stderr';
	let cmd = 'ucode ' + quote(script);
	for (let i = 0; i < length(args); i++)
		cmd += ' ' + quote(args[i]);
	let rc = system(cmd + ' >/dev/null 2>' + quote(errfile));
	out.stderr = fs.readfile(errfile) || '';
	return rc;
}

function readjson(path) {
	let content = fs.readfile(path);
	return content ? json(content) : null;
}

// Load the pure functions without running main
global.TELLDUS_MQTT_UC_TEST = true;
const gen = loadfile(generator)();
const mig = loadfile(migration)();
global.TELLDUS_MQTT_UC_TEST = false;

// ---- Generator, pure function ----

let result = gen.uci2json({
	dev3: { '.type': 'device', '.name': 'dev3', '.index': 3, type: 'switch', id: '2', name: 'Arbetsrum',
		protocol: 'arctech', model: 'selflearning-switch', house: '15858734', unit: '1' },
	main: { '.type': 'main', '.name': 'main', '.index': 0, enabled: '1', host: '10.0.0.2', port: '1884',
		user: 'u', pass: 'p', offline_seconds: '300', topic_prefix: 'Hemma' },
	dev1: { '.type': 'device', '.name': 'dev1', '.index': 1, type: 'sensor',
		id: 'fineoffset_temperaturehumidity_135', name: 'Uteförråd', topic: 'Ute/Förråd', gateway: 'AE01DHPQ' },
	dev2: { '.type': 'device', '.name': 'dev2', '.index': 2, type: 'sensor', id: 'oregon_1A2D_80',
		name: 'Kök/fönster+1#' },
	dev4: { '.type': 'device', '.name': 'dev4', '.index': 4, type: 'sensor', id: 'x_1' },
	dev5: { '.type': 'device', '.name': 'dev5', '.index': 5, id: 'x_2', name: 'No type' },
	dev6: { '.type': 'device', '.name': 'dev6', '.index': 6, type: 'sensor', name: 'No id' },
	dev7: { '.type': 'device', '.name': 'dev7', '.index': 7, type: 'dimmer', id: '7', name: 'Dimmer' }
});
same(result.config, {
	'host': '10.0.0.2',
	'port': 1884,
	'user': 'u',
	'pass': 'p',
	'sensor-offline-seconds': 300,
	'filter-non-transled': 'yes',
	'devices': [
		{ type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Uteförråd', topic: 'Ute/Förråd', gateway: 'AE01DHPQ' },
		{ type: 'sensor', id: 'oregon_1A2D_80', name: 'Kök/fönster+1#', topic: 'Hemma/Kök_fönster_1_' },
		{ type: 'switch', id: '2', name: 'Arbetsrum', topic: 'Hemma/Arbetsrum' }
	]
}, 'uci2json: all options');
check(length(result.warnings) == 4, 'uci2json: 4 sections skipped with a warning');

// Defaults: no main section, bad numbers
result = gen.uci2json({
	main: { '.type': 'main', '.name': 'main', '.index': 0, port: 'abc' },
	d: { '.type': 'device', '.name': 'd', '.index': 1, type: 'switch', id: '5', name: 'Lamp' }
});
same(result.config, {
	'host': '127.0.0.1', 'port': 1883, 'user': '', 'pass': '', 'sensor-offline-seconds': 600,
	'filter-non-transled': 'yes',
	'devices': [ { type: 'switch', id: '5', name: 'Lamp', topic: 'home/Lamp' } ]
}, 'uci2json: defaults');
same(gen.uci2json({}).config.devices, [], 'uci2json: empty config');

// ---- Migration, pure function ----

// As the owner's file: several TellSticks, the same id on more than one,
// a sensor and a switch with the same topic
const multi = {
	'host': '192.168.1.10',
	'port': 1883,
	'user': 'mqtt',
	'pass': 'secret',
	'sensor-offline-seconds': 900,
	'filter-non-transled': 'yes',
	'topic-translation': [
		{ telldus: 'telldus/AE01DHPQ/sensor/fineoffset_temperaturehumidity_135', mqtt: 'Hemma/Uteförråd', name: 'Uteförråd' },
		{ telldus: 'telldus/A703AJQJ/sensor/fineoffset_temperaturehumidity_135', mqtt: 'Landet/Kök', name: 'Landet-kök' },
		{ telldus: 'telldus/A6028FQX/sensor/fineoffset_temperaturehumidity_135', mqtt: 'Båthall/Båt', name: 'Båt' },
		{ telldus: 'telldus/AE01DHPQ/switch/6', mqtt: 'Hemma/Uteförråd', name: 'Uteförråd-lampa' },
		{ telldus: 'telldus/A703AJD8/switch/2', mqtt: 'Stugan/Lampa' },
		{ telldus: 'telldus/A703AJQJ/sensor', mqtt: 'Landet/All', name: 'Prefix' },
		{ telldus: 'homeassistant/x', mqtt: 'y' },
		{ mqtt: 'no/key' },
		{ telldus: 'telldus/AE01DHPQ/sensor/fineoffset_temperaturehumidity_135', mqtt: 'Second/Match', name: 'Duplicate' }
	]
};
result = mig.json2uci(multi);
same(result.main, { host: '192.168.1.10', port: '1883', user: 'mqtt', pass: 'secret', offline_seconds: '900' },
	'json2uci: main options');
same(result.devices, [
	{ type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Uteförråd', topic: 'Hemma/Uteförråd', gateway: 'AE01DHPQ' },
	{ type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Landet-kök', topic: 'Landet/Kök', gateway: 'A703AJQJ' },
	{ type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Båt', topic: 'Båthall/Båt', gateway: 'A6028FQX' },
	{ type: 'switch', id: '6', name: 'Uteförråd-lampa', topic: 'Hemma/Uteförråd', gateway: 'AE01DHPQ' },
	{ type: 'switch', id: '2', name: 'Lampa', topic: 'Stugan/Lampa', gateway: 'A703AJD8' }
], 'json2uci: multi-serial devices');
check(length(result.warnings) == 4, 'json2uci: 3 bad entries and 1 duplicate give warnings');

// A devices list is kept, and wins over topic-translation
result = mig.json2uci({
	devices: [
		{ type: 'switch', id: '2', name: 'Arbetsrum', topic: 'home/Arbetsrum' },
		{ type: 'sensor', id: 'oregon_1A2D_80', name: 'Ute', gateway: 'AE01DHPQ' },
		{ type: 'sensor', id: 'no_name' }
	],
	'topic-translation': [
		{ telldus: 'telldus/AE01DHPQ/sensor/oregon_1A2D_80', mqtt: 'Old/Ute', name: 'Old' }
	]
});
same(result.devices, [
	{ type: 'switch', id: '2', name: 'Arbetsrum', topic: 'home/Arbetsrum' },
	{ type: 'sensor', id: 'oregon_1A2D_80', name: 'Ute', gateway: 'AE01DHPQ' }
], 'json2uci: devices list');
same(result.main, {}, 'json2uci: no main options');
check(length(result.warnings) == 2, 'json2uci: devices list warnings');

// ---- Scripts as on the router: migration, generator, round trip ----

let conf = tmp + '/config';
fs.mkdir(conf, 0755);
let input = tmp + '/telldus-mqtt.json';
fs.writefile(input, sprintf('%.J\n', multi));
let out = {};

check(run(migration, [ input, conf ], out) == 0, 'migration runs');
check(index(out.stderr, 'telldus-mqtt-json2uci: topic-translation entry 6:') >= 0, 'migration warns on stderr');
let uciText = fs.readfile(conf + '/telldus-mqtt');
check(uciText != null, 'migration writes the UCI config');

let c = cursor(conf);
check(c.get('telldus-mqtt', 'main', 'host') == '192.168.1.10', 'migrated host');
check(c.get('telldus-mqtt', 'main', 'offline_seconds') == '900', 'migrated offline_seconds');
let gateways = [];
c.foreach('telldus-mqtt', 'device', function(s) { push(gateways, s.gateway); });
same(gateways, [ 'AE01DHPQ', 'A703AJQJ', 'A6028FQX', 'AE01DHPQ', 'A703AJD8' ], 'migrated gateways');

// Safe to run twice: nothing changed
check(run(migration, [ input, conf ], out) == 0, 'migration runs again');
check(fs.readfile(conf + '/telldus-mqtt') == uciText, 'second migration changes nothing');

// Generator
let output = tmp + '/var/etc/telldus-mqtt.json';
fs.mkdir(tmp + '/var', 0755);
check(run(generator, [ output, conf ], out) == 0, 'generator runs');
let st = fs.stat(output);
check(st && st.mode == 0600, 'generated file has mode 0600');
check(!fs.stat(output + '.tmp'), 'no temporary file left');
let generated = readjson(output);
check(generated && generated.pass == 'secret' && generated.port == 1883 &&
	generated['sensor-offline-seconds'] == 900 && generated['filter-non-transled'] == 'yes',
	'generated main keys');

// Round trip: JSON -> UCI -> JSON gives the same devices
same(generated.devices, mig.json2uci(multi).devices, 'round trip of topic-translation');

// Round trip of a devices list
let conf2 = tmp + '/config2';
fs.mkdir(conf2, 0755);
let devicesJson = {
	host: '127.0.0.1', port: 1883, user: '', pass: '', 'sensor-offline-seconds': 600, 'filter-non-transled': 'yes',
	devices: [
		{ type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Uteförråd', topic: 'Hemma/Uteförråd' },
		{ type: 'switch', id: '2', name: 'Arbetsrum', topic: 'home/Arbetsrum', gateway: 'A703AKOX' }
	]
};
fs.writefile(input, sprintf('%.J\n', devicesJson));
check(run(migration, [ input, conf2 ], out) == 0, 'migration of a devices list runs');
check(run(generator, [ output, conf2 ], out) == 0, 'generator runs again');
same(readjson(output), devicesJson, 'round trip of a devices list');

// Errors
check(run(migration, [ tmp + '/no-such-file.json', conf2 ], out) == 1, 'migration: missing input');
fs.writefile(input, '{ "host": ');
check(run(migration, [ input, conf2 ], out) == 1, 'migration: bad JSON');
check(run(generator, [ output, tmp + '/no-such-dir' ], out) == 1, 'generator: no UCI config');

print(failed ? 'FAILED\n' : 'PASSED\n');
exit(failed ? 1 : 0);
