const assert = require('node:assert/strict');
const fs = require('node:fs');

const attribute = (tag, name) => new RegExp(`\\b${name}="([^"]*)"`).exec(tag)?.[1];
const selectedValue = select => {
    const options = [...select.matchAll(/<option\b[^>]*>/g)].filter(([tag]) => /\sselected(?:\s|>)/.test(tag));
    assert.equal(options.length, 1);
    return attribute(options[0][0], 'value');
};
const keys = ['soc', 'battery', 'plant', 'grid'];
for (const name of ['sun2000', 'sun2000-emma', 'generic']) {
    const html = fs.readFileSync(`${process.argv[2]}/${name}.html`, 'utf8');
    assert(!/%[A-Z_]+%/.test(html), 'Every firmware placeholder must render');
    assert(!/\{\{[A-Z_]+\}\}/.test(html), 'Every partial variable must render');
    assert(!/<script>/.test(html), 'Settings behavior uses HTMX');
    assert(!html.includes('data-defaults'), 'Preset data stays on the server');
    const elements = {};
    for (const match of html.matchAll(/<(input|select)\b[^>]*>/g)) {
        const id = attribute(match[0], 'id');
        if (id) elements[id] = match[0];
    }
    const selector = elements['settings-device-preset'];
    assert.equal(attribute(selector, 'hx-get'), '/settings');
    assert.equal(attribute(selector, 'hx-target'), '#telemetry-settings');
    assert.equal(attribute(selector, 'hx-select'), '#telemetry-settings');
    assert.equal(attribute(selector, 'hx-trigger'), 'change');
    assert.equal(attribute(selector, 'hx-params'), 'settings-device-preset');
    const presetSelect = /<select\b[^>]*id="settings-device-preset"[\s\S]*?<\/select>/.exec(html)[0];
    assert.equal(selectedValue(presetSelect), name);
    const rows = [...html.matchAll(/<tr id="telemetry-([^"]+)">([\s\S]*?)<\/tr>/g)];
    assert.equal(rows.length, 4);
    for (const [row, key] of rows) {
        assert(/<th scope="row">/.test(row));
        assert.equal([...row.matchAll(/<td(?:\s|>)/g)].length, 7);
        const field = suffix => elements[`settings-${key}-${suffix}`];
        const expectedUnit = name === 'sun2000-emma' ? (key === 'grid' ? '0' : '2') : '1';
        assert.equal(attribute(field('unit'), 'value'), expectedUnit);
        assert(!/\schecked(?:\s|>)/.test(field('unavailable')));
        assert.equal(/\schecked(?:\s|>)/.test(field('invert')), name === 'sun2000-emma' && key === 'grid');
        for (const suffix of ['register', 'unit', 'type', 'order', 'scale', 'invert', 'unavailable'])
            assert(/\sdisabled(?:\s|>)/.test(field(suffix)));
        assert(!/\sdisabled(?:\s|>)/.test(field('override')));
        for (const suffix of ['override', 'type']) {
            assert.equal(attribute(field(suffix), 'hx-get'), `/settings?reading=${key}`);
            assert.equal(attribute(field(suffix), 'hx-target'), 'closest tr');
            assert.equal(attribute(field(suffix), 'hx-select'), `#telemetry-${key}`);
            assert.equal(attribute(field(suffix), 'hx-include'), '#settings-device-preset, closest tr');
        }
        const oneWord = key === 'soc';
        assert.equal(attribute(field('register'), 'max'), oneWord ? '65535' : '65534');
        const selects = [...row.matchAll(/<select\b[^>]*>[\s\S]*?<\/select>/g)].map(([select]) => select);
        assert.equal(selectedValue(selects[0]), oneWord ? 'u16' : 'i32');
        assert.equal(selectedValue(selects[1]), 'high');
    }
    assert(keys.every(key => elements[`settings-${key}-override`]));
    for (const mode of ['soc-on', 'soc-i32']) {
        const preview = fs.readFileSync(`${process.argv[2]}/${name}-${mode}.html`, 'utf8');
        assert(!/\{\{[A-Z_]+\}\}/.test(preview));
        const soc = /<tr id="telemetry-soc">([\s\S]*?)<\/tr>/.exec(preview)[1];
        assert(!/\sdisabled(?:\s|>)/.test(soc), 'Overrides enable the row controls');
        const override = /<input\b[^>]*id="settings-soc-override"[^>]*>/.exec(soc)[0];
        assert(/\schecked(?:\s|>)/.test(override));
        const type = /<select\b[^>]*id="settings-soc-type"[\s\S]*?<\/select>/.exec(soc)[0];
        assert.equal(selectedValue(type), mode === 'soc-on' ? 'u16' : 'i32');
        const register = /<input\b[^>]*id="settings-soc-register"[^>]*>/.exec(soc)[0];
        assert.equal(attribute(register, 'max'), mode === 'soc-on' ? '65535' : '65534');
    }
}
console.log('Settings table values and HTMX controls passed.');
