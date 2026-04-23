export default function convertExternalPaletteToRGBA(buffer) {
  const rgba = Buffer.alloc(256 * 4);
  for (
    let rgbIndex = 0, rgbaIndex = 256 * 4 - 1;
    rgbIndex < buffer.length;
    rgbIndex += 3, rgbaIndex -= 4
  ) {
    const red = buffer[rgbIndex];
    const green = buffer[rgbIndex + 1];
    const blue = buffer[rgbIndex + 2];

    // The first color is transparent
    // (last from the buffer)
    const alpha = rgbIndex === buffer.length - 3 ? 0 : 255;

    rgba[rgbaIndex] = alpha;
    rgba[rgbaIndex - 1] = blue;
    rgba[rgbaIndex - 2] = green;
    rgba[rgbaIndex - 3] = red;
  }
  return rgba;
}
