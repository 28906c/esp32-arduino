// gen_font_ascii.js - Generate a small ASCII-only font for fallback
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const asciiChars = ' !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~';

const args = [
    '--font', 'C:\\Windows\\Fonts\\simhei.ttf',
    '--size', '24', '--bpp', '4', '--format', 'lvgl',
    '-o', path.join(__dirname, '..', 'main', 'src', 'ui', 'lv_font_ascii_24.c'),
    '--lv-font-name', 'lv_font_ascii_24',
    '--lv-include', 'lvgl.h',
    '--no-compress',
    '--symbols', asciiChars
];

console.log('Generating ASCII fallback font...');
const r = spawnSync(process.execPath,
    ['C:\\nvm4w\\nodejs\\node_modules\\lv_font_conv\\lv_font_conv.js'].concat(args),
    { stdio: 'inherit' });
if (r.status !== 0) { process.exit(r.status); }

// Write header
const hContent = `#ifndef LV_FONT_ASCII_24_H
#define LV_FONT_ASCII_24_H
#ifdef __cplusplus
extern "C" {
#endif
#include "lvgl.h"
extern const lv_font_t lv_font_ascii_24;
#ifdef __cplusplus
}
#endif
#endif
`;
fs.writeFileSync(path.join(__dirname, '..', 'main', 'include', 'ui', 'lv_font_ascii_24.h'), hContent, 'utf-8');
console.log('Done.');
