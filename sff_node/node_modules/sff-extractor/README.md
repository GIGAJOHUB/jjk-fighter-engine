# SFF Extractor

Extract information and images contained in a SFF file (Elecbyte format).

## Options

| Option             | Description                                   | Type    | Default value |
| ------------------ | --------------------------------------------- | ------- | ------------- |
| palettes           | Includes palettes                             | Boolean | `true`        |
| paletteBuffer      | Includes buffer of each palette               | Boolean | `true`        |
| paletteTable       | Includes colors table of each palette         | Boolean | `true`        |
| sprites            | Includes sprites                              | Boolean | `true`        |
| spriteBuffer       | Includes buffer of each sprite                | Boolean | `true`        |
| decodeSpriteBuffer | Decode buffer of each sprite                  | Boolean | `false`       |
| spriteGroups       | Filter sprites by groups                      | Array   | `[]`          |
| applyPalette       | Apply the palette when decoding sprite buffer | Buffer  | `null`        |

## Examples

Extract everything:

```js
import { readFileSync } from "node:fs";
import extract from "sff-extractor";

const buffer = readFileSync("kfm.sff");
const data = extract(buffer);

console.log(data);
```

Extract only global metadata:

```js
const options = {
  sprites: false,
  palettes: false,
};
const data = extract(buffer, options);
```

Extract a specific sprite group and decode the image:

```js
const options = {
  palettes: false,
  spriteBuffer: false,
  decodeSpriteBuffer: true,
  spriteGroups: [6053],
};
const data = extract(buffer, options);
const sprite = data.sprites[0];
const image = convertToImage(
  sprite.decodedBuffer, // 1 pixel = 4 bytes (RGBA)
  sprite.width,
  sprite.height,
);
```

Apply an external palette:

```js
const buffer = readFileSync(`kfm.sff`);
const palette = readFileSync(`palette.act`);
const options = {
  palettes: false,
  spriteBuffer: false,
  decodeSpriteBuffer: true,
  spriteGroups: [0],
  applyePalette: palette,
};
const data = extract(buffer, options);
const sprite = data.sprites[0];
const image = convertToImage(sprite.decodedBuffer, sprite.width, sprite.height);
```
