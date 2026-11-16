// Smoke test of the views with a small fake LuCI (E, dom, uci, ui, rpc,
// poll, form) on jsdom. It runs the views and clicks through them; it does
// not test the real LuCI. Needs the npm package jsdom; exit 77 (skipped)
// without it. Usage: NODE_PATH=<dir with node_modules> node tests/smoke-views.js
'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');
let JSDOM;
try {
	JSDOM = require('jsdom').JSDOM;
}
catch (e) {
	console.log('jsdom not found => skip');
	process.exit(77);
}

const APP = path.join(__dirname, '..');
const RES = path.join(APP, 'htdocs/luci-static/resources');

const jsdom = new JSDOM('<!doctype html><html><body><div id="maincontent"></div></body></html>');
const window = jsdom.window;
const document = window.document;
global.window = window;
global.document = document;

String.prototype.format = function() {
	const args = Array.from(arguments);
	let i = 0;
	return this.replace(/%[sd]/g, () => String(args[i++]));
};
global._ = s => s;

// ---- dom / E like LuCI ----
function elem(e) { return e != null && typeof e == 'object' && 'nodeType' in e; }
const dom = {
	elem: elem,
	append(node, children) {
		if (Array.isArray(children)) {
			for (const c of children)
				if (elem(c)) node.appendChild(c);
				else if (children != null) node.appendChild(document.createTextNode('' + c));
		}
		else if (typeof children == 'function') this.append(node, children(node));
		else if (elem(children)) node.appendChild(children);
		else if (children != null) node.innerHTML = '' + children;
	},
	content(node, children) {
		while (node.firstChild) node.removeChild(node.firstChild);
		this.append(node, children);
	},
	attr(node, attrs) {
		for (const k in attrs) {
			const v = attrs[k];
			if (v == null) continue;
			if (typeof v == 'function') node.addEventListener(k, v);
			else if (typeof v == 'object') node.setAttribute(k, JSON.stringify(v));
			else node.setAttribute(k, v);
		}
	},
	create(tag, attrs, children) {
		if (!(attrs != null && typeof attrs == 'object' && !Array.isArray(attrs) && !elem(attrs))) {
			children = attrs; attrs = null;
		}
		const n = document.createElement(tag);
		if (attrs) this.attr(n, attrs);
		this.append(n, children);
		return n;
	}
};
global.E = function() { return dom.create.apply(dom, arguments); };

const L = {
	bind(fn, ctx) { const a = Array.prototype.slice.call(arguments, 2); return function() { return fn.apply(ctx, a.concat(Array.from(arguments))); }; },
	url(p) { return '/cgi-bin/luci/' + p; },
	resolveDefault(p, d) { return Promise.resolve(p).catch(() => d); }
};

// ---- fake uci ----
function makeUci(sections) {
	let n = 0;
	const u = {
		data: sections.map(s => Object.assign({}, s)),
		saved: 0,
		load() { return Promise.resolve(); },
		save() { this.saved++; return Promise.resolve(); },
		find(sid) { return this.data.find(s => s['.name'] == sid); },
		get(conf, sid, opt) { const s = this.find(sid); return s ? (opt ? s[opt] : s) : null; },
		set(conf, sid, opt, val) { this.find(sid)[opt] = val; },
		unset(conf, sid, opt) { delete this.find(sid)[opt]; },
		add(conf, type, name) { const sid = name || ('new' + (n++)); this.data.push({ '.name': sid, '.type': type }); return sid; },
		remove(conf, sid) { this.data = this.data.filter(s => s['.name'] != sid); },
		sections(conf, type) { return this.data.filter(s => !type || s['.type'] == type); }
	};
	return u;
}

const ui = {
	modal: null, notes: [],
	showModal(title, children) { this.modal = E('div', { 'class': 'modal' }, children); document.body.classList.add('modal-overlay-active'); document.body.appendChild(this.modal); },
	hideModal() { if (ui.modal) ui.modal.remove(); ui.modal = null; document.body.classList.remove('modal-overlay-active'); },
	addNotification(t, c, cls) { ui.notes.push([ c.textContent, cls ]); },
	createHandlerFn(ctx, fn) {
		const a = Array.prototype.slice.call(arguments, 2);
		if (typeof fn == 'string') fn = ctx[fn];
		return function(ev) { return Promise.resolve(fn.apply(ctx, a.concat([ ev ]))); };
	},
	changes: { applied: 0, apply() { this.applied++; } }
};

const polls = [];
const poll = { add(fn, s) { polls.push(fn); } };

let rpcReply = null;
let initActions = [];
const rpc = {
	declare(d) {
		return function() {
			if (d.object == 'luci.telldus-mqtt') return Promise.resolve(JSON.parse(JSON.stringify(rpcReply)));
			if (d.object == 'luci' && d.method == 'setInitAction') { initActions.push(Array.from(arguments)); return Promise.resolve(true); }
			return Promise.reject(new Error('no rpc'));
		};
	}
};

const view = { extend(o) { const C = function() {}; Object.assign(C.prototype, o); return C; } };
const baseclass = { extend(o) { const C = function() {}; Object.assign(C.prototype, o); return C; } };

// Minimal form for settings.js
const form = {
	Map: function(conf, title, descr) { this.opts = []; this.section = function(cls, name) { const sec = { option: (c, n) => { const o = { name: n }; this.opts.push(o); return o; } }; return sec; }; this.render = () => Promise.resolve(E('div', { 'class': 'cbi-map' }, [ title ])); },
	NamedSection: 'NamedSection', Flag: 'Flag', Value: 'Value'
};

let uci;
const classes = {};
function load(name) {
	if (classes[name]) return classes[name];
	const builtins = { view, dom, poll, uci, ui, rpc, baseclass, form };
	if (builtins[name]) return builtins[name];
	const file = path.join(RES, name.replace(/\./g, '/') + '.js');
	const src = fs.readFileSync(file, 'utf8');
	const names = [], vals = [];
	const re = /^'require ([^ ']+)(?: as ([^']+))?';$/mg;
	let m;
	while ((m = re.exec(src))) {
		names.push(m[2] || m[1].replace(/[^a-zA-Z0-9_]/g, '_'));
		vals.push(load(m[1]));
	}
	const C = new Function('window', 'document', 'L', ...names, src)(window, document, L, ...vals);
	const inst = new C();
	classes[name] = inst;
	return inst;
}
function loadView(name) {
	const file = path.join(RES, 'view', name + '.js');
	const src = fs.readFileSync(file, 'utf8');
	const names = [], vals = [];
	const re = /^'require ([^ ']+)(?: as ([^']+))?';$/mg;
	let m;
	while ((m = re.exec(src))) {
		names.push(m[2] || m[1].replace(/[^a-zA-Z0-9_]/g, '_'));
		vals.push(load(m[1]));
	}
	return new Function('window', 'document', 'L', ...names, src)(window, document, L, ...vals);
}

const SERIAL = 'AE01DHPQ';
function reply() {
	return {
		now: 1763220010,
		status_error: null,
		service: { installed: true, running: true },
		mosquitto: { installed: true, luci: true },
		status: {
			format: 1, version: '0.4.1', started: 1763210000, updated: 1763220000, running: true,
			gateway: { found: true, type: 'tellstick duo', serial: SERIAL },
			service: { connected: true },
			broker: { connected: false, host: '127.0.0.1', port: 1883 },
			last_rf: 1763219990,
			devices: [
				{ type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Uteförråd', topic: 'Hemma/Uteförråd', last_seen: 1763219990, online: true, values: { temperature: '15.8', humidity: '44' } },
				{ type: 'sensor', id: 'oregon_1A2D_80', last_seen: 1763219700, online: true, values: { temperature: '3.1' } },
				{ type: 'sensor', id: 'evil_<b>x</b>_1', last_seen: 1763219700, online: true, values: { temperature: '<img src=x>' } },
				{ type: 'switch', id: '2', name: 'Arbetsrum', topic: 'Hemma/Arbetsrum', values: { state: 'ON' } },
				{ type: 'switch', id: '11', values: {} } ],
			events: [ { time: 1763219990, 'class': 'command', protocol: 'arctech', model: 'selflearning', house: '15858734', unit: '1', group: '0', method: 'turnon' } ]
		}
	};
}

function fire(el, type) { el.dispatchEvent(new window.Event(type, { bubbles: true })); }
const tick = () => new Promise(r => setTimeout(r, 5));

async function main() {
	uci = makeUci([
		{ '.name': 'main', '.type': 'main', enabled: '1', topic_prefix: 'home', manage_switches: '1' },
		{ '.name': 'cfg02', '.type': 'device', type: 'sensor', id: 'fineoffset_temperaturehumidity_135', name: 'Uteförråd', topic: 'Hemma/Uteförråd', gateway: SERIAL },
		{ '.name': 'cfg03', '.type': 'device', type: 'sensor', id: 'oregon_9_9', name: 'Källare', gateway: SERIAL },
		{ '.name': 'cfg05', '.type': 'device', type: 'sensor', id: 'x', name: 'Other', gateway: 'OTHER' },
		{ '.name': 'cfg04', '.type': 'device', type: 'switch', id: '2', name: 'Arbetsrum', protocol: 'arctech', model: 'selflearning-switch', house: '30341302', unit: '2', gateway: SERIAL }
	]);
	rpcReply = reply();
	const main = document.getElementById('maincontent');

	// ---- Status ----
	{
		const V = loadView('telldus-mqtt/status');
		const v = new V();
		const node = await v.render(await v.load());
		main.appendChild(node);
		const text = node.textContent;
		assert.ok(text.includes('Running'), text);
		assert.ok(text.includes('tellstick duo, serial AE01DHPQ'));
		assert.ok(text.includes('Not connected (127.0.0.1:1883)'));
		assert.ok(text.includes('20 s ago'), text);
		assert.ok(node.querySelector('a[href="/cgi-bin/luci/admin/services/mosquitto"]'));
		await polls.pop()();
		rpcReply.status_error = 'missing'; const st = rpcReply.status; rpcReply.status = null;
		const v2 = new V();
		const n2 = await v2.render(await v2.load());
		assert.ok(n2.textContent.includes('There is no status file'), n2.textContent);
		rpcReply.status = st; rpcReply.status_error = null;
		const btn = node.querySelector('button');
		btn.click(); await tick();
		assert.deepStrictEqual(initActions[0], [ 'telldus-mqtt', 'restart' ]);
		main.innerHTML = '';
		polls.length = 0;
		console.log('status OK');
	}

	// ---- Sensors ----
	{
		const V = loadView('telldus-mqtt/sensors');
		const v = new V();
		const node = await v.render(await v.load());
		main.appendChild(node);
		assert.ok(!node.querySelector('b'), 'HTML from the status file must be text');
		assert.ok(!node.querySelector('img'), 'HTML from the status file must be text');
		const rows = Array.from(node.querySelectorAll('tr.tr')).slice(1);
		const names = rows.map(r => r.querySelector('input').value);
		assert.deepStrictEqual(names, [ 'Källare', 'Uteförråd', '', '' ]);
		assert.ok(node.textContent.includes('1 sensors for other TellSticks'), node.textContent);

		// Hint with the prefix; a topic field on each row
		assert.ok(node.textContent.includes('gives the topic home/<name>. Change the prefix on the Settings tab.'), node.textContent);
		assert.ok(!node.querySelector('input[type=checkbox]'));
		assert.strictEqual(node.querySelectorAll('input[data-topic]').length, 4);
		assert.strictEqual(rows[1].querySelector('input[data-topic]').value, 'Hemma/Uteförråd');

		// Name the oregon sensor: the topic field shows the topic at once
		const oregonRow = rows.find(r => r.textContent.includes('oregon_1A2D_80'));
		const inp = oregonRow.querySelector('input');
		const oti = oregonRow.querySelector('input[data-topic]');
		assert.ok(oti.disabled);
		inp.focus(); inp.value = 'Garage'; fire(inp, 'input');
		assert.ok(!oti.disabled);
		assert.strictEqual(oti.getAttribute('placeholder'), 'home/Garage');
		fire(inp, 'change'); inp.blur();
		await tick();
		const added = uci.data.find(s => s.id == 'oregon_1A2D_80');
		assert.deepStrictEqual(Object.assign({}, added, { '.name': undefined }), { '.name': undefined, '.type': 'device', type: 'sensor', id: 'oregon_1A2D_80', name: 'Garage', gateway: SERIAL });
		// Redrawn: topic shown
		assert.ok(node.querySelector('input[data-topic][placeholder="home/Garage"]:not([disabled])'), node.innerHTML);

		// Rename again in the new table: no second section
		let inp2 = Array.from(node.querySelectorAll('input[type=text]')).find(i => i.value == 'Garage');
		inp2.value = 'Garage 2'; fire(inp2, 'change');
		await tick();
		assert.strictEqual(uci.data.filter(s => s.id == 'oregon_1A2D_80').length, 1);
		assert.strictEqual(uci.data.find(s => s.id == 'oregon_1A2D_80').name, 'Garage 2');

		// Topic edit
		const topicInputs = Array.from(node.querySelectorAll('input[type=text]')).filter(i => i.getAttribute('placeholder') && i.getAttribute('placeholder').startsWith('home/'));
		assert.ok(topicInputs.length >= 2);
		const ti = topicInputs.find(i => i.getAttribute('placeholder') == 'home/Garage 2');
		ti.value = 'a/#'; fire(ti, 'change');
		assert.ok(ti.classList.contains('cbi-input-invalid'));
		ti.value = 'x/garage'; fire(ti, 'change'); await tick();
		assert.strictEqual(uci.data.find(s => s.id == 'oregon_1A2D_80').topic, 'x/garage');

		// Clear the name: section removed
		inp2 = Array.from(node.querySelectorAll('input[type=text]')).find(i => i.value == 'Garage 2');
		inp2.value = ''; fire(inp2, 'change'); await tick();
		assert.strictEqual(uci.data.filter(s => s.id == 'oregon_1A2D_80').length, 0);

		// Remove name: Uteförråd (cfg02) is not published, its section is removed
		const uRow = Array.from(node.querySelectorAll('tr.tr')).find(r => r.textContent.includes('fineoffset_temperaturehumidity_135'));
		Array.from(uRow.querySelectorAll('button')).find(b => b.textContent == 'Remove name').click(); await tick();
		assert.ok(!uci.find('cfg02'));

		// Replace: Källare (cfg03) gets the oregon id
		const kRow = Array.from(node.querySelectorAll('tr.tr')).find(r => r.textContent.includes('oregon_9_9'));
		const rb = kRow.querySelector('button');
		rb.click(); await tick();
		assert.ok(ui.modal, 'replace dialog');
		const sel = ui.modal.querySelector('select');
		assert.ok(sel.options.length == 3, ui.modal.textContent);
		assert.ok(ui.modal.textContent.includes('MQTT topic: home/Källare'), ui.modal.textContent);
		sel.value = 'sensor oregon_1A2D_80';
		Array.from(ui.modal.querySelectorAll('button')).find(b => b.textContent == 'Replace').click();
		await tick();
		assert.strictEqual(uci.find('cfg03').id, 'oregon_1A2D_80');
		assert.strictEqual(uci.find('cfg03').name, 'Källare');
		assert.ok(!ui.modal);

		// Poll while typing: no redraw
		const someInput = node.querySelector('input[type=text]');
		someInput.focus();
		const before = node.querySelector('table');
		await polls[polls.length - 1]();
		assert.strictEqual(node.querySelector('table'), before);
		someInput.blur();
		await polls[polls.length - 1]();
		assert.notStrictEqual(node.querySelector('table'), before);

		// Save & Apply
		await v.handleSaveApply(null, '1');
		assert.strictEqual(uci.saved, 1);
		assert.strictEqual(ui.changes.applied, 1);
		main.innerHTML = '';
		polls.length = 0;
		console.log('sensors OK');
	}

	// ---- Switches ----
	{
		const V = loadView('telldus-mqtt/switches');
		const v = new V();
		const node = await v.render(await v.load());
		main.appendChild(node);
		assert.ok(node.textContent.includes('arctech selflearning-switch, house 30341302, unit 2'), node.textContent);
		// Start capture
		const start = Array.from(node.querySelectorAll('button')).find(b => b.textContent == 'Start');
		start.click(); await tick();
		assert.ok(node.textContent.includes('Press a button on the remote control now.'));
		// A new event comes
		rpcReply.status.events.unshift({ time: 1763220005, 'class': 'command', protocol: 'arctech', model: 'selflearning', house: '30341302', unit: '2', group: '0', method: 'turnoff' });
		rpcReply.status.events.unshift({ time: 1763220008, 'class': 'command', protocol: 'sartano', model: 'codeswitch', code: '0101010101', method: 'turnon' });
		// User types in the table: capture still updates
		const ni = node.querySelector('input[type=text]');
		ni.focus();
		await polls[polls.length - 1]();
		ni.blur();
		const capRows = Array.from(node.querySelectorAll('tr.tr')).filter(r => r.textContent.includes('Create switch'));
		assert.strictEqual(capRows.length, 2, node.textContent);
		assert.ok(capRows[1].textContent.includes('Switch 2 "Arbetsrum" has this code'), capRows[1].textContent);
		capRows[0].querySelector('button').click(); await tick();
		assert.ok(ui.modal.textContent.includes('sartano codeswitch, code 0101010101'), ui.modal.textContent);
		assert.ok(ui.modal.textContent.includes('Switch id: 12'), ui.modal.textContent);
		const create = Array.from(ui.modal.querySelectorAll('button')).find(b => b.textContent == 'Create switch');
		create.click(); await tick();
		assert.ok(ui.modal, 'empty name: dialog stays');
		ui.modal.querySelector('input').value = 'Lampa';
		fire(ui.modal.querySelector('input'), 'input');
		assert.ok(ui.modal.textContent.includes('MQTT topic: home/Lampa'), ui.modal.textContent);
		create.click(); await tick();
		const sw = uci.data.find(s => s.name == 'Lampa');
		assert.deepStrictEqual(Object.assign({}, sw, { '.name': 0 }), { '.name': 0, '.type': 'device', type: 'switch', id: '12', name: 'Lampa', protocol: 'sartano', model: 'codeswitch', code: '0101010101', gateway: SERIAL });
		assert.ok(node.textContent.includes('Not in telldusd yet'), node.textContent);

		// Remove name of switch 2: section kept (RF options), name removed
		const arow = Array.from(node.querySelectorAll('tr.tr')).find(r => { const i = r.querySelector('input'); return i && i.value == 'Arbetsrum'; });
		Array.from(arow.querySelectorAll('button')).find(b => b.textContent == 'Remove name').click(); await tick();
		assert.ok(uci.find('cfg04'));
		assert.strictEqual(uci.find('cfg04').name, undefined);

		// Remove the new switch
		const lrow = Array.from(node.querySelectorAll('tr.tr')).find(r => { const i = r.querySelector('input'); return i && i.value == 'Lampa'; });
		Array.from(lrow.querySelectorAll('button')).find(b => b.textContent == 'Remove').click(); await tick();
		Array.from(ui.modal.querySelectorAll('button')).find(b => b.textContent == 'Remove').click(); await tick();
		assert.ok(!uci.data.find(s => s.name == 'Lampa'));

		// Stop capture
		Array.from(node.querySelectorAll('button')).find(b => b.textContent == 'Stop').click(); await tick();
		assert.ok(Array.from(node.querySelectorAll('button')).find(b => b.textContent == 'Start'));
		main.innerHTML = '';
		console.log('switches OK');
	}

	// ---- Settings ----
	{
		const V = loadView('telldus-mqtt/settings');
		const v = new V();
		const node = await v.render(await v.load());
		assert.ok(node.querySelector('a[href="/cgi-bin/luci/admin/services/mosquitto"]'));
		console.log('settings OK');
	}

	// ---- rpc failure ----
	{
		rpcReply = { };
		const V = loadView('telldus-mqtt/sensors');
		const v = new V();
		const node = await v.render(await v.load());
		assert.ok(node.textContent.includes('Cannot get the status'), node.textContent);
		console.log('rpc failure OK');
	}
}

main().then(() => { console.log('ALL OK'); process.exit(0); }, e => { console.log('FAIL', e.stack); process.exit(1); });
