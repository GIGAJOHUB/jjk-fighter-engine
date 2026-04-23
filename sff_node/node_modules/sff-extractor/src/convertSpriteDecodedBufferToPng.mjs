import { PNG } from "pngjs";

export default function convertSpriteDecodedBufferToPng(buffer, width, height) {
  const png = new PNG({ width, height });

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      let index = (y * width + x) * 4;
      let red = buffer[index];
      let green = buffer[index + 1];
      let blue = buffer[index + 2];
      let alpha = buffer[index + 3];

      let pngIndex = (y * width + x) << 2;
      png.data[pngIndex] = red;
      png.data[pngIndex + 1] = green;
      png.data[pngIndex + 2] = blue;
      png.data[pngIndex + 3] = alpha;
    }
  }

  const options = { colorType: 6 };
  return PNG.sync.write(png, options);
}
