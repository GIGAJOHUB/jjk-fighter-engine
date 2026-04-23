import convertPaletteRGBtoRGBA from "./convertPaletteRGBtoRGBA.mjs";

function getWidthFromPcx(buffer) {
  const minX = buffer.readUInt16LE(4);
  const maxX = buffer.readUInt16LE(8);
  return maxX - minX + 1;
}

function getHeightFromPcx(buffer) {
  const minY = buffer.readUInt16LE(6);
  const maxY = buffer.readUInt16LE(10);
  return maxY - minY + 1;
}

export default function extractSpritesFromSFFV1(data, metadata) {
  let firstPalette = null;
  let previousPalette = null;
  const sprites = [];
  for (
    let index = 0, spriteOffset = metadata.firstSpriteOffset;
    index < metadata.spriteCount;
    index++
  ) {
    let nextSpriteOffset = data.readUInt32LE(spriteOffset);
    // Sometimes, the nextSpriteOffset is 0
    // It happens for the last sprite
    // It means the section goes to the end
    if (nextSpriteOffset === 0) {
      nextSpriteOffset = data.length;
    }
    const spriteSection = data.subarray(spriteOffset, nextSpriteOffset);

    // Subfile length (not including header)
    // Length is 0 if it is a linked sprite
    const length = spriteSection.readUInt32LE(4);

    const x = spriteSection.readUInt16LE(8);
    const y = spriteSection.readUInt16LE(10);
    const group = spriteSection.readUInt16LE(12);
    const number = spriteSection.readUInt16LE(14);
    let linkedSpriteIndex = spriteSection.readUInt16LE(16);
    if (linkedSpriteIndex >= index) {
      linkedSpriteIndex = 0;
    }

    // 0 =
    // 1 =
    // 109 = ?
    const samePalette = spriteSection.readUInt8(18);

    const comment = spriteSection.toString("ascii", 19, 19 + 14);

    const c00 = index > 0 || (group === 0 && number === 0);
    const paletteSize = c00 || samePalette ? 0 : 256 * 3;

    let palette = null;
    if (samePalette && firstPalette && group === 0 && number === 0) {
      palette = firstPalette;
    } else if (samePalette && previousPalette) {
      palette = previousPalette;
    } else {
      const paletteRGB = data.subarray(
        nextSpriteOffset - 256 * 3,
        nextSpriteOffset,
      );
      palette = convertPaletteRGBtoRGBA(paletteRGB);
    }
    if (!firstPalette) {
      firstPalette = palette;
    }
    previousPalette = palette;

    if (length === 0) {
      sprites.push({
        ...sprites[index - 1],
        index,
        x,
        y,
        group,
        number,
        linkedSpriteIndex,
        samePalette,
        comment,
        palette,
      });
      spriteOffset = nextSpriteOffset;
      continue;
    }

    /*
      console.log({
        index,
        length,
        x,
        y,
        group,
        number,
        linkedSpriteIndex,
        samePalette,
        paletteSize,
        comment,
        palette,
      });
    */
    let imageBuffer =
      linkedSpriteIndex === 0
        ? data.subarray(spriteOffset + 32, nextSpriteOffset - paletteSize)
        : sprites[linkedSpriteIndex].buffer;
    let width = getWidthFromPcx(imageBuffer);
    let height = getHeightFromPcx(imageBuffer);

    // Too big, invalid size
    // Fallback to 1x1 image
    if (width * height >= 65536 * 65536) {
      width = 1;
      height = 1;
    }

    const sprite = {
      index,
      length,
      x,
      y,
      group,
      number,
      linkedSpriteIndex,
      samePalette,
      paletteSize,
      comment,
      palette,
      buffer: imageBuffer,
      width,
      height,
    };

    sprites.push(sprite);
    spriteOffset = nextSpriteOffset;
  }

  return sprites;
}
