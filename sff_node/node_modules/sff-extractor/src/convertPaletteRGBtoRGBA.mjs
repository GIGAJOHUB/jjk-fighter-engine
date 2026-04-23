export default function convertPaletteRGBtoRGBA(rgb) {
  const rgba = Buffer.alloc(256 * 4);
  for (let rgbIndex = 0, rgbaIndex=0; rgbIndex < rgb.length; rgbIndex+=3, rgbaIndex+=4) {
    rgba[rgbaIndex + 0] = rgb[rgbIndex + 0];
    rgba[rgbaIndex + 1] = rgb[rgbIndex + 1];
    rgba[rgbaIndex + 2] = rgb[rgbIndex + 2];
    rgba[rgbaIndex + 3] = 255;
  }

  return rgba;
}