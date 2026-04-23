import UPNG from "./UPNG.mjs";

export default function decodePNG8(buffer, width, height, palette) {
  const rawDataOffset = 4; // first 4 octet represents uncompressed length of data
  const data = buffer.subarray(rawDataOffset);

  const decodedPng = UPNG.decode(data);
  const colorComponentCount = 4;
  const out = Buffer.alloc(width * height * colorComponentCount, 0);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const colorIndex = decodedPng.data[y * width + x];
      const red = palette[colorIndex * colorComponentCount + 0];
      const green = palette[colorIndex * colorComponentCount + 1];
      const blue = palette[colorIndex * colorComponentCount + 2];
      //const alpha = palette[colorIndex + 3];

      const index = (y * width + x) * 4;
      out[index + 0] = red;
      out[index + 1] = green;
      out[index + 2] = blue;
      out[index + 3] = colorIndex === 0 ? 0 : 255; // Only the first color is transparent
    }
  }

  return out;
}
