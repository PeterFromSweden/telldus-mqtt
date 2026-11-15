// Tests of the OpenWrt scripts. run-tests.sh runs this file.
// Usage: ucode test.uc <openwrt dir> <empty temp dir> [<confusedump program>]

'use strict';

import { cursor } from 'uci';
import * as fs from 'fs';

const dir = ARGV[0];
const tmp = ARGV[1];
const generator = dir + '/telldus-mqtt-uci2json.uc';
const migration = dir + '/telldus-mqtt-json2uci.uc';
const uci2tellstick = dir + '/telldus-mqtt-uci2tellstick.uc';
const tellstick2uci = dir + '/telldus-mqtt-tellstick2uci.uc';
const confusedump = (ARGV[2] != null && ARGV[2] != '') ? ARGV[2] : null;
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

// Run a script as on the router. Returns the exit code, sets out.stdout
// and out.stderr.
function run(script, args, out) {
	let errfile = tmp + '/stderr';
	let outfile = tmp + '/stdout';
	let cmd = 'ucode ' + quote(script);
	for (let i = 0; i < length(args); i++)
		cmd += ' ' + quote(args[i]);
	let rc = system(cmd + ' >' + quote(outfile) + ' 2>' + quote(errfile));
	out.stdout = fs.readfile(outfile) || '';
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
const imp = loadfile(tellstick2uci)();
global.TELLDUS_MQTT_UC_TEST = false;
const lib = loadfile(dir + '/telldus-mqtt-tellstick.uc')();

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

// A switch without name, with RF options, is only for tellstick.conf: no warning
result = gen.uci2json({
	d: { '.type': 'device', '.name': 'd', '.index': 1, type: 'switch', id: '11', protocol: 'risingsun' }
});
same(result.config.devices, [], 'uci2json: unnamed switch not published');
same(result.warnings, [], 'uci2json: unnamed switch, no warning');

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

// ---- tellstick.conf: parser ----

// libconfuse reads this file as telldusd does: same result as the parser?
function confuse(path) {
	if (!confusedump)
		return null;
	let outfile = tmp + '/confusedump.json';
	let rc = system(quote(confusedump) + ' ' + quote(path) + ' >' + quote(outfile) + ' 2>/dev/null');
	return (rc == 0) ? json(fs.readfile(outfile)) : false;
}

// The parse() result in the confusedump format
function dumped(p) {
	let devices = [], controllers = [];
	for (let d in p.devices) {
		let params = {};
		for (let k in lib.PARAMETERS)
			if (d.parameters[k] != null)
				params[k] = d.parameters[k];
		push(devices, { id: d.id, name: d.name, controller: d.controller, protocol: d.protocol,
			model: d.model, parameters: params });
	}
	for (let c in p.controllers)
		push(controllers, { id: c.id, name: c.name, type: c.type, serial: c.serial });
	return {
		user: p.globals.user, group: p.globals.group, deviceNode: p.globals.devicenode,
		ignoreControllerConfirmation: p.globals.ignorecontrollerconfirmation,
		devices: devices, controllers: controllers
	};
}

// Parse a file with parse() and, if there, with libconfuse: same result
function parse_file(path, what) {
	let p = lib.parse(fs.readfile(path));
	let c = confuse(path);
	if (c != null)
		same(dumped(p), c, what + ': parser and libconfuse agree');
	return p;
}

const fixture = dir + '/tests/tellstick.conf';
const fixtureText = fs.readfile(fixture);
const fixtureParsed = parse_file(fixture, 'fixture');

// Only the parts that come from UCI and back
function rf(devices) {
	let list = [];
	for (let d in devices) {
		let params = {};
		for (let k in lib.PARAMETERS)
			if (d.parameters[k] != null)
				params[k] = d.parameters[k];
		push(list, [ d.id, d.protocol, d.model, params ]);
	}
	return list;
}

same(rf(fixtureParsed.devices), [
	[ 1, 'arctech', 'selflearning-switch:nexa', { house: '30341302', unit: '1' } ],
	[ 2, 'arctech', 'selflearning-switch:nexa', { house: '30341302', unit: '2' } ],
	[ 3, 'arctech', 'selflearning-switch:nexa', { house: '15547', unit: '3' } ],
	[ 4, 'comen', 'selflearning-switch:jula', { house: '1928374', unit: '13' } ],
	[ 5, 'arctech', 'selflearning-switch:nexa', { house: '736456', unit: '8' } ],
	[ 6, 'comen', 'selflearning-switch:jula', { house: '2233557', unit: '5' } ],
	[ 7, 'sartano', 'codeswitch', { code: '1000000100' } ],
	[ 8, 'sartano', 'codeswitch', { code: '0100010000' } ],
	[ 9, 'sartano', 'codeswitch', { code: '0100100000' } ],
	[ 10, 'arctech', 'selflearning-switch:nexa', { house: '2467345', unit: '9' } ],
	[ 11, 'risingsun', 'codeswitch:kjelloco', { unit: '1', system: '1' } ]
], 'fixture devices');
check(fixtureParsed.devices[10].name == 'L4', 'fixture: unquoted name');
check(fixtureParsed.devices[5].name == 'Uteförråd', 'fixture: UTF-8 name');
same(map(fixtureParsed.controllers, c => c.serial), [ 'A501IVK7', 'A703AJQJ', 'A6028FQX', 'AE01DHPQ', 'A703AKOX' ],
	'fixture controllers');
check(fixtureParsed.globals.user == 'root' && fixtureParsed.globals.devicenode == '/dev/tellstick', 'fixture globals');

// What libconfuse accepts: comments, quotes, escapes, case, Windows line ends
const quirks =
	'# comment\r\n' +
	'USER=root // comment\r\n' +
	'/* a comment\r\n   on two lines */ group = "ro\\\not"\r\n' +
	'deviceNode = /dev/ttyUSB0#comment\r\n' +
	'device { id=0x10 name=\'it\\\'s\' Protocol = "a\\"b\\\\c\\td\\x41\\101\\q" model=x/y//z parameters { house = A } }\r\n' +
	'device {\r\n id = "+012"\r\n  name = "Kök \\$ $HOME"\r\n parameters { unit = "1" }\r\n parameters { code = 1 unit = 2 }\r\n}\r\n' +
	'controller { id = 1 serial = "A#B" name = Hemma }';
fs.writefile(tmp + '/quirks.conf', quirks);
let qp = parse_file(tmp + '/quirks.conf', 'quirks');
same(dumped(qp), {
	user: 'root', group: 'root', deviceNode: '/dev/ttyUSB0', ignoreControllerConfirmation: 'false',
	devices: [
		{ id: 16, name: "it's", controller: 0, protocol: 'a"b\\c\tdAAq', model: 'x/y//z', parameters: { house: 'A' } },
		{ id: 10, name: 'Kök $ $HOME', controller: 0, protocol: 'arctech', model: '', parameters: { unit: '2', code: '1' } }
	],
	controllers: [ { id: 1, name: 'Hemma', type: 0, serial: 'A#B' } ]
}, 'parser: libconfuse syntax');

// What the parser refuses (libconfuse refuses it too, or it is not safe)
let bad = {
	'unknown option': 'device { id = 1 room = "x" }',
	'missing }': 'device { id = 1',
	'extra }': 'user = "root" }',
	'missing =': 'user "root"',
	'missing value': 'user =',
	'no number': 'device { id = one }',
	'list': 'user = { "a", "b" }',
	'append': 'user += "a"',
	'comma': 'device { id = 1, name = "x" }',
	'environment variable': 'user = ${USER}',
	'environment variable in a string': 'user = "${USER}"',
	'no end quote': 'user = "root',
	'comment without end': '/* user = "root"',
	'bad escape': 'user = "\\9"',
	'stray character': 'user = "root" *',
	'device title': 'device "x" { id = 1 }'
};
for (let what in bad) {
	let ok = true;
	try {
		lib.parse(bad[what]);
	}
	catch (e) {
		ok = false;
	}
	check(!ok, 'parser refuses: ' + what);
	if (confusedump) {
		fs.writefile(tmp + '/bad.conf', bad[what]);
		// libconfuse reads these, but they are not safe to change
		let lenient = [ 'missing }', 'comment without end', 'stray character',
			'environment variable', 'environment variable in a string' ];
		check(confuse(tmp + '/bad.conf') == false || index(lenient, what) >= 0, 'libconfuse also refuses: ' + what);
	}
}

// Quoting: values with special characters come back the same
let names = [ 'a"b', 'back\\slash', '${HOME}', 'tab\tnew\nline\r', 'Uteförråd # // /* */', "it's", '', 'x\x01y' ];
let qdevices = [];
for (let i = 0; i < length(names); i++)
	push(qdevices, { id: i + 1, name: names[i], controller: 0, protocol: 'p "' + i, model: names[i],
		parameters: { house: names[i], fade: 'true' } });
fs.writefile(tmp + '/quote.conf', lib.replace_devices(lib.DEFAULT_HEADER, lib.parse(lib.DEFAULT_HEADER), qdevices));
let qback = parse_file(tmp + '/quote.conf', 'quoting');
same(map(qback.devices, d => [ d.name, d.model, d.parameters.house, d.protocol ]),
	map(qdevices, d => [ d.name, d.model, d.parameters.house, d.protocol ]), 'quoting: values come back the same');

// ---- tellstick.conf: devices from UCI ----

result = lib.devices_from_uci({
	a: { '.type': 'device', '.name': 'a', '.index': 1, type: 'switch', id: '12', name: 'Lamp', protocol: 'arctech',
		model: 'selflearning-switch', house: '123', unit: '1', topic: 'x/y', gateway: 'AE01DHPQ' },
	b: { '.type': 'device', '.name': 'b', '.index': 2, type: 'switch', id: '3', protocol: 'sartano', code: '0101' },
	c: { '.type': 'device', '.name': 'c', '.index': 3, type: 'switch', id: '4', name: 'No RF options' },
	d: { '.type': 'device', '.name': 'd', '.index': 4, type: 'sensor', id: 'x_1', name: 'Sensor', protocol: 'x' }
});
same(result.errors, [], 'devices_from_uci: no errors');
same(result.devices, [
	{ id: 3, name: 'switch 3', controller: 0, protocol: 'sartano', model: '', parameters: { code: '0101' } },
	{ id: 12, name: 'Lamp', controller: 0, protocol: 'arctech', model: 'selflearning-switch',
		parameters: { house: '123', unit: '1' } }
], 'devices_from_uci: switches with protocol, sorted by id, unnamed gets a name');

result = lib.devices_from_uci({
	a: { '.type': 'device', '.name': 'a', '.index': 1, type: 'switch', id: '2', protocol: 'arctech' },
	b: { '.type': 'device', '.name': 'b', '.index': 2, type: 'switch', id: '2', protocol: 'arctech' },
	c: { '.type': 'device', '.name': 'c', '.index': 3, type: 'switch', id: 'two', protocol: 'arctech' },
	d: { '.type': 'device', '.name': 'd', '.index': 4, type: 'switch', id: '02', protocol: 'arctech' },
	e: { '.type': 'device', '.name': 'e', '.index': 5, type: 'switch', id: '5', protocol: [ 'a', 'b' ] }
});
check(length(result.errors) == 4, 'devices_from_uci: duplicate id, bad ids and a list give errors');

// ---- tellstick.conf: import, pure function ----

result = imp.import_devices(fixtureParsed.devices, {
	a: { '.type': 'device', '.name': 'a', '.index': 1, type: 'switch', id: '1', name: 'One', gateway: 'X' },
	b: { '.type': 'device', '.name': 'b', '.index': 2, type: 'switch', id: '2', name: 'Two' },
	c: { '.type': 'device', '.name': 'c', '.index': 3, type: 'switch', id: '2', name: 'Two', gateway: 'X' }
}, null);
same(result.errors, [], 'import_devices: no errors');
same(map(result.changes, ch => [ ch.id, ch.section ]), [
	[ '1', 'a' ], [ '2', 'b' ], [ '3', null ], [ '4', null ], [ '5', null ], [ '6', null ],
	[ '7', null ], [ '8', null ], [ '9', null ], [ '10', null ], [ '11', null ]
], 'import_devices: one section, section without gateway, new sections');
result = imp.import_devices(fixtureParsed.devices, {
	a: { '.type': 'device', '.name': 'a', '.index': 1, type: 'switch', id: '1', name: 'One', gateway: 'X' }
}, 'Y');
check(result.changes[0].section == null, 'import_devices: the only section is for another gateway: new section');

// ---- tellstick.conf: the scripts as on the router ----

// The owner's config after the json2uci migration: four TellSticks
function owner_uci(path) {
	let names = [ 'motorvärmare', 'arbetsrum', 'sovrum', 'belysning', 'grill', 'uteförråd',
		'L1', 'L2', 'L3', 'C4' ];
	let s = "config main 'main'\n\toption enabled '1'\n\toption topic_prefix 'home'\n\n" +
		"config device\n\toption type 'sensor'\n\toption id 'fineoffset_temperaturehumidity_135'\n" +
		"\toption name 'Uteförråd'\n\toption gateway 'AE01DHPQ'\n\n";
	for (let i = 1; i <= 10; i++)
		s += sprintf("config device\n\toption type 'switch'\n\toption id '%d'\n\toption name 'Hemma-%s'\n" +
			"\toption topic 'Hemma/%s'\n\toption gateway 'AE01DHPQ'\n\n", i, names[i - 1], names[i - 1]);
	for (let i = 1; i <= 3; i++)
		s += sprintf("config device\n\toption type 'switch'\n\toption id '%d'\n\toption name 'Landet-%d'\n" +
			"\toption gateway 'A703AJQJ'\n\n", i, i);
	for (let i = 1; i <= 5; i++)
		s += sprintf("config device\n\toption type 'switch'\n\toption id '%d'\n\toption name 'Båthall-%d'\n" +
			"\toption gateway 'A6028FQX'\n\n", i, i);
	fs.writefile(path, s);
}

let tconf = tmp + '/tconf';
fs.mkdir(tconf, 0755);
owner_uci(tconf + '/telldus-mqtt');
let tellstick = tmp + '/tellstick.conf';
fs.writefile(tellstick, fixtureText);
fs.chmod(tellstick, 0600);

// manage_switches is not set: tellstick.conf is not changed
check(run(uci2tellstick, [ tellstick, tconf ], out) == 0 && out.stdout == 'off\n', 'uci2tellstick: off');
check(fs.readfile(tellstick) == fixtureText, 'uci2tellstick off: file not changed');

// Import without a gateway serial: ids 1-5 are not clear, nothing changed
let uciBefore = fs.readfile(tconf + '/telldus-mqtt');
check(run(tellstick2uci, [ tellstick, tconf ], out) == 1, 'tellstick2uci: not clear without serial');
check(index(out.stderr, 'switch 1: 3 sections have this id') >= 0, 'tellstick2uci: error on stderr');
check(fs.readfile(tconf + '/telldus-mqtt') == uciBefore, 'tellstick2uci error: UCI not changed');

// Import with the serial of this TellStick
check(run(tellstick2uci, [ tellstick, tconf, 'AE01DHPQ' ], out) == 0, 'tellstick2uci with serial runs');
check(fs.readfile(tellstick) == fixtureText, 'tellstick2uci: tellstick.conf not changed');
c = cursor(tconf);
check(c.get('telldus-mqtt', 'main', 'manage_switches') == '1', 'tellstick2uci sets manage_switches');
let imported = [];
c.foreach('telldus-mqtt', 'device', function(s) {
	if (s.type == 'switch')
		push(imported, [ s.id, s.gateway, s.name, s.protocol, s.house, s.unit, s.code, s.system ]);
});
same(imported, [
	[ '1', 'AE01DHPQ', 'Hemma-motorvärmare', 'arctech', '30341302', '1', null, null ],
	[ '2', 'AE01DHPQ', 'Hemma-arbetsrum', 'arctech', '30341302', '2', null, null ],
	[ '3', 'AE01DHPQ', 'Hemma-sovrum', 'arctech', '15547', '3', null, null ],
	[ '4', 'AE01DHPQ', 'Hemma-belysning', 'comen', '1928374', '13', null, null ],
	[ '5', 'AE01DHPQ', 'Hemma-grill', 'arctech', '736456', '8', null, null ],
	[ '6', 'AE01DHPQ', 'Hemma-uteförråd', 'comen', '2233557', '5', null, null ],
	[ '7', 'AE01DHPQ', 'Hemma-L1', 'sartano', null, null, '1000000100', null ],
	[ '8', 'AE01DHPQ', 'Hemma-L2', 'sartano', null, null, '0100010000', null ],
	[ '9', 'AE01DHPQ', 'Hemma-L3', 'sartano', null, null, '0100100000', null ],
	[ '10', 'AE01DHPQ', 'Hemma-C4', 'arctech', '2467345', '9', null, null ],
	[ '1', 'A703AJQJ', 'Landet-1', null, null, null, null, null ],
	[ '2', 'A703AJQJ', 'Landet-2', null, null, null, null, null ],
	[ '3', 'A703AJQJ', 'Landet-3', null, null, null, null, null ],
	[ '1', 'A6028FQX', 'Båthall-1', null, null, null, null, null ],
	[ '2', 'A6028FQX', 'Båthall-2', null, null, null, null, null ],
	[ '3', 'A6028FQX', 'Båthall-3', null, null, null, null, null ],
	[ '4', 'A6028FQX', 'Båthall-4', null, null, null, null, null ],
	[ '5', 'A6028FQX', 'Båthall-5', null, null, null, null, null ],
	[ '11', null, null, 'risingsun', null, '1', null, '1' ]
], 'tellstick2uci: multi-gateway import');

// Safe to run twice
let uciImported = fs.readfile(tconf + '/telldus-mqtt');
check(run(tellstick2uci, [ tellstick, tconf, 'AE01DHPQ' ], out) == 0, 'tellstick2uci runs again');
check(fs.readfile(tconf + '/telldus-mqtt') == uciImported, 'second tellstick2uci changes nothing');

// The generator: switch 11 has no name, it is not published and gives no warning
check(run(generator, [ tmp + '/t.json', tconf ], out) == 0 && out.stderr == '', 'uci2json after import: no warnings');
check(!length(filter(readjson(tmp + '/t.json').devices, d => d.id == '11')), 'uci2json: switch 11 not published');

// Round trip: tellstick.conf -> UCI -> tellstick.conf
check(run(uci2tellstick, [ tellstick, tconf ], out) == 0 && out.stdout == 'changed\n',
	'uci2tellstick: changed (the names are from UCI)');
let tst = fs.stat(tellstick);
check(tst && tst.mode == 0600, 'uci2tellstick keeps the file mode');
check(!fs.stat(tellstick + '.tmp'), 'uci2tellstick: no temporary file left');
let written = fs.readfile(tellstick);
let back = parse_file(tellstick, 'round trip');
same(rf(back.devices), rf(fixtureParsed.devices), 'round trip: same devices');
same(map(back.devices, d => d.name), [ 'Hemma-motorvärmare', 'Hemma-arbetsrum', 'Hemma-sovrum', 'Hemma-belysning',
	'Hemma-grill', 'Hemma-uteförråd', 'Hemma-L1', 'Hemma-L2', 'Hemma-L3', 'Hemma-C4', 'switch 11' ],
	'round trip: names from UCI');
check(lib.replace_devices(written, back, []) == lib.replace_devices(fixtureText, fixtureParsed, []),
	'round trip: the other content (globals, controllers, comments) is the same');
check(index(written, 'device {\n  id = 2\n  name = "Hemma-arbetsrum"\n  controller = 0\n  protocol = "arctech"\n' +
	'  model = "selflearning-switch:nexa"\n  parameters {\n    house = "30341302"\n    unit = "2"\n  }\n}\n') >= 0,
	'uci2tellstick: device format');

// Again: unchanged, the file is not written
fs.chmod(tellstick, 0400);
check(run(uci2tellstick, [ tellstick, tconf ], out) == 0 && out.stdout == 'unchanged\n', 'uci2tellstick: unchanged');
check(fs.readfile(tellstick) == written && fs.stat(tellstick).mode == 0400, 'uci2tellstick unchanged: file not written');
fs.chmod(tellstick, 0600);

// telldusd writes the file in its own format (cfg_print): still unchanged
let cfgprint = replace(written, / = /g, '=');
fs.writefile(tellstick, cfgprint);
check(run(uci2tellstick, [ tellstick, tconf ], out) == 0 && out.stdout == 'unchanged\n',
	'uci2tellstick: other format, same devices: unchanged');

// A UCI change
c = cursor(tconf);
let sec11 = null;
c.foreach('telldus-mqtt', 'device', function(s) { if (s.id == '11') sec11 = s['.name']; });
c.set('telldus-mqtt', sec11, 'name', 'Kjell & "Co" \\ ${x}');
c.commit('telldus-mqtt');
check(run(uci2tellstick, [ tellstick, tconf ], out) == 0 && out.stdout == 'changed\n', 'uci2tellstick: UCI change');
back = parse_file(tellstick, 'after a UCI change');
check(back.devices[10].name == 'Kjell & "Co" \\ ${x}', 'uci2tellstick: name quoted');

// Errors: duplicate id, bad id. The file is not changed.
let before = fs.readfile(tellstick);
c.set('telldus-mqtt', 'dup', 'device');
c.set('telldus-mqtt', 'dup', 'type', 'switch');
c.set('telldus-mqtt', 'dup', 'id', '2');
c.set('telldus-mqtt', 'dup', 'protocol', 'arctech');
c.commit('telldus-mqtt');
check(run(uci2tellstick, [ tellstick, tconf ], out) == 1, 'uci2tellstick: duplicate id is an error');
check(index(out.stderr, 'switch id 2 is also in section') >= 0, 'uci2tellstick: duplicate id on stderr');
check(fs.readfile(tellstick) == before, 'uci2tellstick duplicate: file not changed');
c.set('telldus-mqtt', 'dup', 'id', 'x2');
c.commit('telldus-mqtt');
check(run(uci2tellstick, [ tellstick, tconf ], out) == 1, 'uci2tellstick: bad id is an error');
check(fs.readfile(tellstick) == before, 'uci2tellstick bad id: file not changed');
c.delete('telldus-mqtt', 'dup');
c.commit('telldus-mqtt');

// A file that cannot be parsed is not changed
fs.writefile(tellstick, before + 'room = "x"\n');
check(run(uci2tellstick, [ tellstick, tconf ], out) == 1, 'uci2tellstick: parse error');
check(index(out.stderr, 'unknown option "room"') >= 0, 'uci2tellstick: parse error on stderr');
check(fs.readfile(tellstick) == before + 'room = "x"\n', 'uci2tellstick parse error: file not changed');

// No tellstick.conf: a new file with the default settings
let newconf = tmp + '/new/tellstick.conf';
fs.mkdir(tmp + '/new', 0755);
check(run(uci2tellstick, [ newconf, tconf ], out) == 0 && out.stdout == 'changed\n', 'uci2tellstick: new file');
check(fs.stat(newconf) && fs.stat(newconf).mode == 0644, 'uci2tellstick: new file mode 0644');
let fresh = parse_file(newconf, 'new file');
check(fresh.globals.user == 'root' && fresh.globals.group == 'root' && length(fresh.devices) == 11,
	'uci2tellstick: new file content');

// No devices in UCI: all device sections are removed, controllers stay
let emptyconf = tmp + '/empty';
fs.mkdir(emptyconf, 0755);
fs.writefile(emptyconf + '/telldus-mqtt', "config main 'main'\n\toption manage_switches '1'\n");
fs.writefile(tellstick, fixtureText);
check(run(uci2tellstick, [ tellstick, emptyconf ], out) == 0 && out.stdout == 'changed\n', 'uci2tellstick: no devices');
back = parse_file(tellstick, 'no devices');
check(length(back.devices) == 0 && length(back.controllers) == 5, 'uci2tellstick: no devices, 5 controllers');

// Import into a config without main section and without tellstick.conf
let noconf = tmp + '/noconf';
fs.mkdir(noconf, 0755);
check(run(tellstick2uci, [ tmp + '/no-such.conf', noconf ], out) == 0, 'tellstick2uci: no tellstick.conf');
c = cursor(noconf);
check(c.get('telldus-mqtt', 'main', 'manage_switches') == '1', 'tellstick2uci: no tellstick.conf, manage_switches 1');

// Import errors: duplicate id in tellstick.conf, parse error
let dupconf = tmp + '/dupconf';
fs.mkdir(dupconf, 0755);
fs.writefile(dupconf + '/telldus-mqtt', "config main 'main'\n");
fs.writefile(tmp + '/dup.conf', 'device { id = 1 }\ndevice { id = 1 }\n');
check(run(tellstick2uci, [ tmp + '/dup.conf', dupconf ], out) == 1, 'tellstick2uci: duplicate id');
fs.writefile(tmp + '/dup.conf', 'device { id = 1 \n');
check(run(tellstick2uci, [ tmp + '/dup.conf', dupconf ], out) == 1, 'tellstick2uci: parse error');
check(fs.readfile(dupconf + '/telldus-mqtt') == "config main 'main'\n", 'tellstick2uci errors: UCI not changed');

print(failed ? 'FAILED\n' : 'PASSED\n');
exit(failed ? 1 : 0);
