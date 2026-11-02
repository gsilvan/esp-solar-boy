const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const attribute = (tag, name) => new RegExp(`\\b${name}="([^"]*)"`).exec(tag)?.[1]
    ?.replaceAll('&quot;', '"').replaceAll('&#39;', "'").replaceAll('&amp;', '&');
const keys = ['soc', 'battery', 'plant', 'grid'];
for (const name of ['sun2000', 'sun2000-emma', 'generic']) {
    const html = fs.readFileSync(`${process.argv[2]}/${name}.html`, 'utf8');
    assert(!/%[A-Z_]+%/.test(html), 'Every firmware placeholder must render');
    const table = /<table class="telemetry-table">([\s\S]*?)<\/table>/.exec(html);
    assert(table, 'Advanced telemetry uses a semantic table');
    const rows = [.../<tbody>([\s\S]*?)<\/tbody>/.exec(table[1])[1].matchAll(/<tr>([\s\S]*?)<\/tr>/g)];
    assert.equal(rows.length, 4);
    for (const row of rows) {
        assert(/<th scope="row">/.test(row[1]));
        assert.equal([...row[1].matchAll(/<td(?:\s|>)/g)].length, 7);
        assert(!row[1].includes('<fieldset'), 'Table controls must be valid table-cell content');
    }
    const elements = {};
    for (const match of html.matchAll(/<(input|select|fieldset)\b[^>]*>/g)) {
        const tag = match[0];
        const id = attribute(tag, 'id');
        if (!id) continue;
        elements[id] = {
            value: attribute(tag, 'value') ?? '',
            checked: /\schecked(?:\s|>)/.test(tag),
            disabled: /\sdisabled(?:\s|>)/.test(tag),
            listeners: {},
            addEventListener(event, callback) { this.listeners[event] = callback; },
            change() { this.listeners.change(); },
        };
        if (match[1] === 'select') {
            const body = html.slice(match.index + tag.length).split('</select>')[0];
            const options = [...body.matchAll(/<option\b[^>]*>/g)].map(([option]) => ({
                value: attribute(option, 'value'),
                selected: /\sselected(?:\s|>)/.test(option),
                dataset: {defaults: attribute(option, 'data-defaults')},
            }));
            elements[id].value = (options.find(option => option.selected) ?? options[0]).value;
            Object.defineProperty(elements[id], 'selectedOptions', {
                get() { return options.filter(option => option.value === this.value); },
            });
        }
    }
    elements['generic-warning'] = {hidden: name !== 'generic'};
    const get = id => elements[id];
    const field = (key, suffix) => get(`settings-${key}-${suffix}`);
    const assertRowDisabled = (key, disabled) => {
        for (const suffix of ['register', 'unit', 'type', 'order', 'scale', 'invert', 'unavailable'])
            assert.equal(field(key, suffix).disabled, disabled);
        assert.equal(field(key, 'override').disabled, false);
    };
    const document = {getElementById: get};
    vm.runInNewContext(/<script>([\s\S]*?)<\/script>/.exec(html)[1], {document});
    const preset = get('settings-device-preset');
    assert.equal(preset.value, name);
    assert(!html.includes('settings-inverter-modbus-unit'), 'Global unit setting is removed');
    assert(!html.includes('-kind'), 'Holding registers have no type selector');
    assert(!html.includes('Default Modbus unit:') && !html.includes('preset-unit'));
    assert(!html.includes('unit-mode') && !html.includes('Preset default'), 'No unit-mode dropdowns');
    assert.equal(field('soc', 'scale').value, name === 'generic' ? '1' : '0.1');
    assert.equal(field('grid', 'invert').checked, name === 'sun2000-emma');
    for (const key of keys) {
        const expected = name === 'sun2000-emma' ? (key === 'grid' ? '0' : '2') : '1';
        assert.equal(field(key, 'unit').value, expected, 'Table shows the effective preset unit');
    }
    for (const key of keys) {
        assert.equal(field(key, 'unavailable').checked, false);
        assertRowDisabled(key, true);
        field(key, 'override').checked = true;
        field(key, 'override').change();
        assertRowDisabled(key, false);
        assert(!field(key, 'unit').readOnly, 'Unit can be edited immediately after enabling the override');
        field(key, 'register').value = '500';
        field(key, 'unit').value = '5';
        field(key, 'scale').value = '0.01';
        field(key, 'invert').checked = true;
        assert.equal(field(key, 'scale').value, '0.01', 'Inversion is separate from scale');
        field(key, 'type').value = 'u16';
        field(key, 'type').change();
        assert.equal(field(key, 'register').max, '65535');
        field(key, 'type').value = 'i32';
        field(key, 'type').change();
        assert.equal(field(key, 'register').max, '65534');
        field(key, 'override').checked = false;
        field(key, 'override').change();
        const defaults = JSON.parse(preset.selectedOptions[0].dataset.defaults).telemetry[keys.indexOf(key)];
        assert.equal(field(key, 'unit').value, defaults.unit);
        assert.equal(field(key, 'register').value, defaults.register);
        assert.equal(field(key, 'scale').value, defaults.scale);
        assert.equal(field(key, 'invert').checked, defaults.invert);
        assertRowDisabled(key, true);
    }
    for (const next of ['sun2000-emma', 'generic', 'sun2000']) {
        preset.value = next;
        preset.change();
        const defaults = JSON.parse(preset.selectedOptions[0].dataset.defaults);
        assert.equal(get('generic-warning').hidden, next !== 'generic');
        keys.forEach((key, index) => {
            const expectedUnit = next === 'sun2000-emma' ? (key === 'grid' ? '0' : '2') : '1';
            assert.equal(field(key, 'unit').value, expectedUnit, 'Switching presets updates the table units');
            assert.equal(field(key, 'override').checked, false);
            assert.equal(field(key, 'unavailable').checked, false);
            assertRowDisabled(key, true);
            for (const [suffix, value] of Object.entries(defaults.telemetry[index])) {
                if (typeof value === 'boolean') assert.equal(field(key, suffix).checked, value);
                else assert.equal(field(key, suffix).value, value);
            }
        });
        assert.equal(field('grid', 'invert').checked, next === 'sun2000-emma');
    }
    assert.equal(get('settings-inverter-ip').value, '192.0.2.10');
    assert.equal(get('settings-inverter-port').value, '502');
    // An unavailable preset option has no defaults; selecting it must not throw.
    const selected = preset.selectedOptions[0];
    const savedDefaults = selected.dataset.defaults;
    delete selected.dataset.defaults;
    assert.doesNotThrow(() => preset.change());
    selected.dataset.defaults = savedDefaults;
    const form = /<form\b[^>]*>/.exec(html)[0];
    elements.message = {textContent: ''};
    const handleResponse = new Function('event', 'document', attribute(form, 'hx-on::after:request'));
    handleResponse({detail: {ctx: {response: {raw: {ok: false}}, text: 'Invalid mapping'}}}, document);
    assert.equal(elements.message.textContent, 'Invalid mapping');
}
console.log('All telemetry controls, inversion switches, EMMA defaults and generic preset passed.');
