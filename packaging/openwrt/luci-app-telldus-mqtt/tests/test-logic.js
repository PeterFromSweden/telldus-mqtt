// Tests of htdocs/luci-static/resources/telldus-mqtt/logic.js, without LuCI.
// Usage: node tests/test-logic.js
'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');

// Shim: LuCI runs a class file as a function body with the required
// classes as arguments. baseclass.extend() is enough as the identity here.
function load() {
	const file = path.join(__dirname, '..', 'htdocs/luci-static/resources/telldus-mqtt/logic.js');
	const src = fs.readFileSync(file, 'utf8');
	const factory = new Function('baseclass', '_', src);
	return factory({ extend: function(o) { return o; } }, function(s) { return s; });
}

const logic = load();
let failed = 0;
let passed = 0;

function test(name, fn) {
	try {
		fn();
		passed++;
	}
	catch (e) {
		failed++;
		console.log('FAIL ' + name + '\n  ' + e.message.split('\n').join('\n  '));
	}
}

// Sections as uci.sections() gives them
function sec(name, opts) {
	return Object.assign({ '.name': name, '.type': 'device', '.anonymous': true }, opts);
}

const SERIAL = 'AE01DHPQ';

const status = {
	devices: [
		{ type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Uteförråd', topic: 'Hemma/Uteförråd', last_seen: 1000, online: true, values: { temperature: '15.8', humidity: '44' } },
		{ type: 'sensor', id: 'oregon_1A2D_80', last_seen: 900, online: false, values: { temperature: '3.1' } },
		{ type: 'switch', id: '2', name: 'Arbetsrum', topic: 'Hemma/Arbetsrum', values: { state: 'ON' } },
		{ type: 'switch', id: '11', values: {} }
	],
	events: [
		{ time: 1000, 'class': 'command', protocol: 'arctech', model: 'selflearning', house: '15858734', unit: '1', group: '0', method: 'turnon' }
	]
};

// The owner's case: the same sensor id under several TellSticks
const sections = [
	sec('cfg01', { type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Annan', gateway: 'OTHER1' }),
	sec('cfg02', { type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Uteförråd', topic: 'Hemma/Uteförråd', gateway: SERIAL }),
	sec('cfg03', { type: 'sensor', id: 'oregon_9_9', name: 'Källare' }),
	sec('cfg04', { type: 'switch', id: '2', name: 'Arbetsrum', protocol: 'arctech', model: 'selflearning-switch:nexa', house: '30341302', unit: '2', gateway: SERIAL }),
	sec('cfg05', { type: 'switch', id: '7', protocol: 'sartano', model: 'codeswitch', code: '0101010101' }),
	sec('cfg06', { type: 'sensor', id: 'oregon_9_9', name: 'Dubblett' }),
	sec('cfg07', { type: 'switch', id: '30', name: 'Other', gateway: 'OTHER2' })
];

test('deriveTopic', function() {
	assert.strictEqual(logic.deriveTopic('home', 'Uteförråd'), 'home/Uteförråd');
	assert.strictEqual(logic.deriveTopic('home', 'a/b+c#d'), 'home/a_b_c_d');
	assert.strictEqual(logic.deriveTopic('', 'Garage'), 'Garage');
	assert.strictEqual(logic.deriveTopic('Hemma/Ute', 'x'), 'Hemma/Ute/x');
});

test('topicPrefix as the generator', function() {
	assert.strictEqual(logic.topicPrefix({}), 'home');
	assert.strictEqual(logic.topicPrefix(null), 'home');
	assert.strictEqual(logic.topicPrefix({ topic_prefix: '' }), '');
	assert.strictEqual(logic.topicPrefix({ topic_prefix: 'Hemma' }), 'Hemma');
});

test('appliesTo and newSectionGateway', function() {
	assert.ok(logic.appliesTo({}, SERIAL));
	assert.ok(logic.appliesTo({ gateway: '' }, SERIAL));
	assert.ok(logic.appliesTo({ gateway: SERIAL }, SERIAL));
	assert.ok(!logic.appliesTo({ gateway: 'X' }, SERIAL));
	assert.ok(!logic.appliesTo({ gateway: 'X' }, ''));
	assert.strictEqual(logic.newSectionGateway(sections, SERIAL), SERIAL);
	assert.strictEqual(logic.newSectionGateway(sections, ''), null);
	assert.strictEqual(logic.newSectionGateway([ sec('a', { type: 'sensor', id: '1', name: 'x' }) ], SERIAL), null);
	assert.strictEqual(logic.newSectionGateway([], SERIAL), null);
});

test('mergeDevices sensors with gateways', function() {
	const m = logic.mergeDevices('sensor', status.devices, sections, SERIAL, 'home');
	assert.strictEqual(m.otherGateways, 1);
	assert.strictEqual(m.rows.length, 4);

	const [ ute, oregon, kallare, dup ] = m.rows;
	assert.strictEqual(ute.sid, 'cfg02');
	assert.strictEqual(ute.name, 'Uteförråd');
	assert.strictEqual(ute.effectiveTopic, 'Hemma/Uteförråd');
	assert.strictEqual(ute.derivedTopic, 'home/Uteförråd');
	assert.ok(ute.heard && ute.online);
	assert.strictEqual(ute.lastSeen, 1000);
	assert.deepStrictEqual(ute.values, { temperature: '15.8', humidity: '44' });

	assert.strictEqual(oregon.sid, null);
	assert.strictEqual(oregon.name, '');
	assert.strictEqual(oregon.effectiveTopic, '');
	assert.ok(oregon.heard && !oregon.online);

	assert.strictEqual(kallare.sid, 'cfg03');
	assert.ok(!kallare.heard);
	assert.ok(!kallare.duplicate);
	assert.strictEqual(kallare.effectiveTopic, 'home/Källare');

	assert.strictEqual(dup.sid, 'cfg06');
	assert.ok(dup.duplicate);
	assert.notStrictEqual(dup.key, kallare.key);
});

test('mergeDevices: other TellStick serial', function() {
	const m = logic.mergeDevices('sensor', status.devices, sections, 'OTHER1', 'home');
	assert.strictEqual(m.rows[0].sid, 'cfg01');
	assert.strictEqual(m.rows[0].name, 'Annan');
	assert.strictEqual(m.otherGateways, 1);
});

test('mergeDevices: serial not known shows only sections without gateway', function() {
	const m = logic.mergeDevices('sensor', [], sections, '', 'home');
	assert.deepStrictEqual(m.rows.map(r => r.sid), [ 'cfg03', 'cfg06' ]);
	assert.strictEqual(m.otherGateways, 2);
});

test('mergeDevices: a device matched by a duplicate section', function() {
	// A section with the id of a heard device, but not used: duplicate
	const s = [ sec('a', { type: 'sensor', id: 'oregon_1A2D_80', name: 'A' }), sec('b', { type: 'sensor', id: 'oregon_1A2D_80', name: 'B' }) ];
	const m = logic.mergeDevices('sensor', status.devices, s, SERIAL, 'home');
	const rows = m.rows.filter(r => r.id == 'oregon_1A2D_80');
	assert.strictEqual(rows.length, 2);
	assert.strictEqual(rows[0].sid, 'a');
	assert.ok(!rows[0].duplicate);
	assert.ok(rows[1].duplicate);
});

test('mergeDevices switches', function() {
	const m = logic.mergeDevices('switch', status.devices, sections, SERIAL, 'home');
	assert.strictEqual(m.otherGateways, 1);
	assert.deepStrictEqual(m.rows.map(r => r.id), [ '2', '11', '7' ]);
	assert.deepStrictEqual(m.rows[0].rf, { protocol: 'arctech', model: 'selflearning-switch:nexa', house: '30341302', unit: '2' });
	assert.deepStrictEqual(m.rows[0].values, { state: 'ON' });
	assert.strictEqual(m.rows[1].sid, null);
	assert.ok(!m.rows[2].heard);
	assert.strictEqual(m.rows[2].name, '');
	assert.strictEqual(logic.formatRf(m.rows[2].rf), 'sartano codeswitch, code 0101010101');
});

test('mergeDevices: bad input', function() {
	const m = logic.mergeDevices('sensor', null, null, null, 'home');
	assert.deepStrictEqual(m, { rows: [], otherGateways: 0 });
	const m2 = logic.mergeDevices('sensor', [ null, { type: 'sensor' } ], [], SERIAL, 'home');
	assert.strictEqual(m2.rows.length, 0);
});

test('duplicateTopics', function() {
	const rows = [
		{ effectiveTopic: 'home/a', duplicate: false },
		{ effectiveTopic: 'home/a', duplicate: false },
		{ effectiveTopic: 'home/b', duplicate: false },
		{ effectiveTopic: 'home/b', duplicate: true },
		{ effectiveTopic: '', duplicate: false },
		{ effectiveTopic: '', duplicate: false }
	];
	assert.deepStrictEqual(logic.duplicateTopics(rows), { 'home/a': true });
});

test('formatAge and age', function() {
	assert.strictEqual(logic.formatAge(0), '0 s');
	assert.strictEqual(logic.formatAge(59.9), '59 s');
	assert.strictEqual(logic.formatAge(60), '1 min');
	assert.strictEqual(logic.formatAge(3599), '59 min');
	assert.strictEqual(logic.formatAge(3600), '1 h');
	assert.strictEqual(logic.formatAge(3600 + 5 * 60), '1 h 5 min');
	assert.strictEqual(logic.formatAge(86400 * 2 + 3600 * 4), '2 d 4 h');
	assert.strictEqual(logic.formatAge(86400), '1 d');
	assert.strictEqual(logic.formatAge(-5), '0 s');
	assert.strictEqual(logic.formatAge(null), '-');
	assert.strictEqual(logic.age(1010, 1000), 10);
	assert.strictEqual(logic.age(1000, 1010), 0);	// clock step back
	assert.strictEqual(logic.age(1000, 0), null);	// 0 = never
	assert.strictEqual(logic.age(null, 5), null);
});

test('formatValues', function() {
	assert.strictEqual(logic.formatValues({ temperature: '15.8', humidity: '44' }), '15.8 °C, 44 %');
	assert.strictEqual(logic.formatValues({ windgust: '3.0', foo: 'x' }), 'windgust 3.0 m/s, foo x');
	assert.strictEqual(logic.formatValues({}), '');
	assert.strictEqual(logic.formatValues(null), '');
});

test('planName: new name makes a section', function() {
	const row = { type: 'sensor', id: 'oregon_1A2D_80', sid: null };
	assert.deepStrictEqual(logic.planName(row, '  Garage ', null, sections, SERIAL),
		[ { op: 'add', values: { type: 'sensor', id: 'oregon_1A2D_80', name: 'Garage', gateway: SERIAL } } ]);
	// No section uses gateways: no gateway
	assert.deepStrictEqual(logic.planName(row, 'Garage', null, [], SERIAL),
		[ { op: 'add', values: { type: 'sensor', id: 'oregon_1A2D_80', name: 'Garage' } } ]);
	assert.deepStrictEqual(logic.planName(row, '  ', null, sections, SERIAL), []);
});

test('planName: change and clear', function() {
	const s = sec('cfg03', { type: 'sensor', id: 'oregon_9_9', name: 'Källare', topic: 'x/y' });
	const row = { type: 'sensor', id: 'oregon_9_9', sid: 'cfg03' };
	assert.deepStrictEqual(logic.planName(row, 'Vind', s, sections, SERIAL),
		[ { op: 'set', sid: 'cfg03', option: 'name', value: 'Vind' } ]);
	assert.deepStrictEqual(logic.planName(row, 'Källare', s, sections, SERIAL), []);
	assert.deepStrictEqual(logic.planName(row, '', s, sections, SERIAL), [ { op: 'remove', sid: 'cfg03' } ]);
});

test('planName: a switch with RF options keeps its section', function() {
	const s = sections[3];
	const row = { type: 'switch', id: '2', sid: 'cfg04' };
	assert.deepStrictEqual(logic.planName(row, '', s, sections, SERIAL),
		[ { op: 'unset', sid: 'cfg04', option: 'name' } ]);
	const s2 = Object.assign({}, s, { topic: 'a/b' });
	assert.deepStrictEqual(logic.planName(row, '', s2, sections, SERIAL),
		[ { op: 'unset', sid: 'cfg04', option: 'name' }, { op: 'unset', sid: 'cfg04', option: 'topic' } ]);
	// A switch without RF options: removed
	const s3 = sec('x', { type: 'switch', id: '11', name: 'Lampa', gateway: SERIAL });
	assert.deepStrictEqual(logic.planName({ type: 'switch', id: '11', sid: 'x' }, '', s3, sections, SERIAL),
		[ { op: 'remove', sid: 'x' } ]);
});

test('planTopic and validTopic', function() {
	const row = { sid: 'cfg03', topic: '' };
	assert.deepStrictEqual(logic.planTopic(row, ' a/b '), [ { op: 'set', sid: 'cfg03', option: 'topic', value: 'a/b' } ]);
	assert.deepStrictEqual(logic.planTopic(row, ''), []);
	assert.deepStrictEqual(logic.planTopic({ sid: 'cfg03', topic: 'a/b' }, ''), [ { op: 'unset', sid: 'cfg03', option: 'topic' } ]);
	assert.deepStrictEqual(logic.planTopic(row, 'a/#'), []);
	assert.deepStrictEqual(logic.planTopic({ sid: null, topic: '' }, 'a'), []);
	assert.ok(logic.validTopic('home/a b'));
	assert.ok(!logic.validTopic('home/+'));
	assert.ok(logic.validTopic(''));
});

test('Replace', function() {
	const rows = logic.mergeDevices('sensor', status.devices.concat([
		{ type: 'sensor', id: 'fineoffset_temperature_12', last_seen: 990, online: true, values: { temperature: '2.0' } }
	]), sections, SERIAL, 'home').rows;
	const old = rows.find(r => r.sid == 'cfg03');
	const cand = logic.replaceCandidates(old, rows);
	assert.deepStrictEqual(cand.map(r => r.id), [ 'fineoffset_temperature_12', 'oregon_1A2D_80' ]);
	assert.deepStrictEqual(logic.planReplace(old, cand[0]),
		[ { op: 'set', sid: 'cfg03', option: 'id', value: 'fineoffset_temperature_12' } ]);
	// The new sensor has a section without name: it is removed
	assert.deepStrictEqual(logic.planReplace(old, { id: 'n', sid: 'cfg99' }),
		[ { op: 'remove', sid: 'cfg99' }, { op: 'set', sid: 'cfg03', option: 'id', value: 'n' } ]);
	assert.deepStrictEqual(logic.planReplace({ id: 'a', sid: null }, { id: 'b', sid: null }), []);
});

test('nextSwitchId', function() {
	assert.strictEqual(logic.nextSwitchId(sections, status.devices), '31');
	assert.strictEqual(logic.nextSwitchId([], status.devices), '12');
	assert.strictEqual(logic.nextSwitchId([], []), '1');
	assert.strictEqual(logic.nextSwitchId([ sec('a', { type: 'sensor', id: '500' }), sec('b', { type: 'switch', id: 'abc' }) ], null), '1');
});

test('capture', function() {
	const e1 = { time: 1000, 'class': 'command', protocol: 'arctech', model: 'selflearning', house: '1', unit: '1', group: '0', method: 'turnon' };
	const start = logic.captureStart([ e1 ]);
	assert.deepStrictEqual(logic.capturedEvents([ e1 ], start), []);
	// The same button again: a new time, so it is new
	const e1b = Object.assign({}, e1, { time: 1005 });
	const e2 = Object.assign({}, e1, { time: 1003, method: 'turnoff' });
	assert.deepStrictEqual(logic.capturedEvents([ e1b, e2 ], start), [ e1b, e2 ]);
	assert.deepStrictEqual(logic.capturedEvents(null, start), []);
	assert.deepStrictEqual(logic.capturedEvents([ e1 ], logic.captureStart(null)), [ e1 ]);
	// Key order does not matter
	assert.strictEqual(logic.eventKey({ a: 1, b: 2 }), logic.eventKey({ b: 2, a: 1 }));
});

test('eventToSwitch: known protocols', function() {
	const cases = [
		[ { 'class': 'command', protocol: 'arctech', model: 'selflearning', house: '15858734', unit: '1', group: '0', method: 'turnon' },
			{ protocol: 'arctech', model: 'selflearning-switch', house: '15858734', unit: '1' } ],
		[ { 'class': 'command', protocol: 'arctech', model: 'codeswitch', house: 'A', unit: '3', method: 'turnoff' },
			{ protocol: 'arctech', model: 'codeswitch', house: 'A', unit: '3' } ],
		[ { 'class': 'command', protocol: 'arctech', model: 'codeswitch', house: 'B', method: 'bell' },
			{ protocol: 'arctech', model: 'bell', house: 'B' } ],
		[ { 'class': 'command', protocol: 'waveman', model: 'codeswitch', house: 'C', unit: '16', method: 'turnon' },
			{ protocol: 'waveman', model: 'codeswitch', house: 'C', unit: '16' } ],
		[ { 'class': 'command', protocol: 'sartano', model: 'codeswitch', code: '0110011001', method: 'turnon' },
			{ protocol: 'sartano', model: 'codeswitch', code: '0110011001' } ],
		[ { 'class': 'command', protocol: 'everflourish', model: 'selflearning', house: '1234', unit: '4', method: 'learn' },
			{ protocol: 'everflourish', model: 'selflearning', house: '1234', unit: '4' } ],
		[ { 'class': 'command', protocol: 'x10', model: 'codeswitch', house: 'P', unit: '1', method: 'turnoff' },
			{ protocol: 'x10', model: 'codeswitch', house: 'P', unit: '1' } ]
	];
	for (const [ ev, expected ] of cases) {
		const r = logic.eventToSwitch(Object.assign({ time: 5 }, ev));
		assert.deepStrictEqual(r, { options: expected, known: true }, JSON.stringify(ev));
	}
});

test('eventToSwitch: unknown protocol', function() {
	const ev = { time: 5, 'class': 'command', protocol: 'hasta', model: 'selflearningv2', house: '4242', unit: '2', method: 'up', 'Bad-Key': 'x' };
	assert.deepStrictEqual(logic.eventToSwitch(ev),
		{ options: { protocol: 'hasta', model: 'selflearningv2', house: '4242', unit: '2' }, known: false });
	// Known protocol, but keys missing
	assert.deepStrictEqual(logic.eventToSwitch({ protocol: 'arctech', model: 'selflearning', house: '1' }),
		{ options: { protocol: 'arctech', model: 'selflearning', house: '1' }, known: false });
});

test('findSwitch', function() {
	assert.strictEqual(logic.findSwitch(sections, { protocol: 'arctech', model: 'selflearning-switch', house: '30341302', unit: '2' }), sections[3]);
	assert.strictEqual(logic.findSwitch(sections, { protocol: 'arctech', model: 'selflearning-switch', house: '30341302', unit: '3' }), null);
	assert.strictEqual(logic.findSwitch(sections, { protocol: 'sartano', model: 'codeswitch', code: '0101010101' }), sections[4]);
});

test('planCreateSwitch', function() {
	const ev = status.events[0];
	assert.deepStrictEqual(logic.planCreateSwitch(ev, ' Lampa ', sections, status.devices, SERIAL), [ { op: 'add', values: {
		type: 'switch', id: '31', name: 'Lampa', protocol: 'arctech', model: 'selflearning-switch',
		house: '15858734', unit: '1', gateway: SERIAL } } ]);
	assert.deepStrictEqual(logic.planCreateSwitch(ev, 'Lampa', [], [], SERIAL), [ { op: 'add', values: {
		type: 'switch', id: '1', name: 'Lampa', protocol: 'arctech', model: 'selflearning-switch',
		house: '15858734', unit: '1' } } ]);
});

test('formatEvent', function() {
	assert.strictEqual(logic.formatEvent(status.events[0]), 'arctech selflearning, house 15858734, unit 1, group 0, turnon');
	assert.strictEqual(logic.formatEvent({ time: 1, protocol: 'sartano', model: 'codeswitch', code: '01', method: 'turnoff' }), 'sartano codeswitch, code 01, turnoff');
});

console.log(passed + ' passed, ' + failed + ' failed');
process.exit(failed ? 1 : 0);
