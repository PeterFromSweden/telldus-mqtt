// Parse all LuCI JS files of the package, as LuCI does: the file is the body
// of a function (it has a top-level 'return'). Also parse the JSON files.
// Usage: node tests/check-syntax.js
'use strict';

const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');
let failed = 0;
let count = 0;

function walk(dir, out) {
	for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
		const p = path.join(dir, e.name);
		if (e.isDirectory())
			walk(p, out);
		else
			out.push(p);
	}
	return out;
}

for (const file of walk(path.join(root, 'htdocs'), []).concat(walk(path.join(root, 'root'), []))) {
	const src = fs.readFileSync(file, 'utf8');
	try {
		if (file.endsWith('.js'))
			new Function('window', 'document', 'L', src);
		else if (file.endsWith('.json'))
			JSON.parse(src);
		else
			continue;
		count++;
	}
	catch (e) {
		failed++;
		console.log('FAIL ' + path.relative(root, file) + ': ' + e.message);
	}
}

console.log(count + ' files OK, ' + failed + ' failed');
process.exit(failed ? 1 : 0);
