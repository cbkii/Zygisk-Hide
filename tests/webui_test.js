const fs = require('node:fs');
const assert = require('node:assert/strict');
const html = fs.readFileSync('module/template/webroot/index.html', 'utf8');
const source = html.match(/function parseTargetConfig\(text\) \{[\s\S]*?\n\}\n/)[0];
const parse = new Function(source + '; return parseTargetConfig;')();
assert.equal(parse('{"com.example":true}')['com.example'], true);
assert.equal(parse(' { "com.example": false, "com.example:worker":true } ')['com.example'], false);
for (const invalid of ['', '[]', '{}garbage', '{"a":true,"a":false}', '{"a":1}',
  '{"a":true,}', '{"a":trueX}', '{"a\\u002eb":true}', ' '.repeat(65537)]) {
  assert.throws(() => parse(invalid));
}
assert.throws(() => parse(JSON.stringify(Object.fromEntries(Array.from({length:1025}, (_, i) => ['com.app'+i, true])))));
