import { writeFileSync } from "node:fs";
import { PNG } from "pngjs";
import decodeRLE8 from "./decodeRLE8.mjs";
import decodeLZ5 from "./decodeLZ5.mjs";
import decodePNG8 from "./decodePNG8.mjs";
import decodePCX from "./decodePCX.mjs";

function saveAsPNG(image, width, height, outputPath) {
  const png = new PNG({ width: width, height: height });

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      let index = (y * width + x) * 4;
      let red = image[index];
      let green = image[index + 1];
      let blue = image[index + 2];
      let alpha = image[index + 3];

      let pngIndex = (y * width + x) << 2;
      png.data[pngIndex] = red;
      png.data[pngIndex + 1] = green;
      png.data[pngIndex + 2] = blue;
      png.data[pngIndex + 3] = alpha;
    }
  }

  const options = { colorType: 6 };
  const buffer = PNG.sync.write(png, options);
  writeFileSync(outputPath, buffer);
}

export default function saveSpriteAsPng(spriteGroup, spriteNumber, spriteBuffer, imageWidth, imageHeight, palette, compressionMethod) {
  switch (compressionMethod) {
    default:
      console.error(`Unknown compression method: ${compressionMethod}`);
      throw new Error(`Unknown compression method: ${compressionMethod}`);
      break;
    case 'PCX':
      {
        const sprite = decodePCX(
          spriteBuffer,
          imageWidth,
          imageHeight,
          palette
        );
        saveAsPNG(
          sprite,
          imageWidth,
          imageHeight,
          `sprites/${spriteGroup}-${spriteNumber}.png`
        );
      }
      break;
    case "RLE5":
      {
        console.error("TODO RLE5");
        throw new Error(`TODO RLE5`);
      }
      break;
    case "RLE8":
      {
        const sprite = decodeRLE8(
          spriteBuffer,
          imageWidth,
          imageHeight,
          palette
        );
        saveAsPNG(
          sprite,
          imageWidth,
          imageHeight,
          `sprites/${spriteGroup}-${spriteNumber}.png`
        );
      }
      break;
    case "LZ5":
      {
        const sprite = decodeLZ5(
          spriteBuffer,
          imageWidth,
          imageHeight,
          palette
        );
        saveAsPNG(
          sprite,
          imageWidth,
          imageHeight,
          `sprites/${spriteGroup}-${spriteNumber}.png`
        );
      }
      break;
    case "PNG8":
      {
        const sprite = decodePNG8(
          spriteBuffer,
          imageWidth,
          imageHeight,
          palette
        );
        saveAsPNG(
          sprite,
          imageWidth,
          imageHeight,
          `sprites/${spriteGroup}-${spriteNumber}.png`
        );
      }
      break;
    case "PNG24":
    case "PNG32": {
      var png = PNG.sync.read(spriteBuffer.subarray(4));
      var options = { colorType: 6 };
      var buffer = PNG.sync.write(png, options);
      writeFileSync(`sprites/${spriteGroup}-${spriteNumber}.png`, buffer);
      //console.log(`sprites/${spriteGroup}-${spriteNumber}.png`);
    }
  }
}