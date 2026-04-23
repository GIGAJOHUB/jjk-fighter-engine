export default function decodeRLE8(data, width, height, palette) {
  if (data.length < 4) {
    throw new Error(`Invalid RLE8, length: ${data.length}`);
  }
  //first 4 octet represents uncompressed length of data
  //and it must be the same as imgw * imgh
  //and RLE8 is always for 256 indexed colour
  const uncompressedDataLength = data.readUInt32LE(0);
  if (uncompressedDataLength !== width * height) {
    // Not true, sometimes x8
    /*
    throw new Error(
      `Invalid RLE8, uncompressed data length should be ${
        width * height
      } (${width} x ${height}), actual: ${uncompressedDataLength}`
    );
    //*/
  }
  if (data.length < 4) {
    throw new Error(`Invalid RLE8, too short`);
  }

  const colorComponentCount = 4;
  const out = Buffer.alloc(width * height * colorComponentCount, 0);

  const rawDataOffset = 4; // first 4 octet represents uncompressed length of data
  let x = 0;
  let y = 0;
  let runlength = -1;
  for (let index = 0; index < data.length - rawDataOffset; index++) {
    const paletteValueIndex = data.readUInt8(index + rawDataOffset);

    //0b01xxxxxx is runlength, otherwise raw data
    if ((paletteValueIndex & 0xc0) != 0x40 || runlength != -1) {
      //if not runlength or octet after runlength
      if (runlength == -1) {
        runlength = 1; //always assume runlength = 1
      }
      //Add pixel for runlength times
      for (let ii = 0; ii < runlength; ii++) {
        const red = palette[paletteValueIndex * colorComponentCount + 0];
        const green = palette[paletteValueIndex * colorComponentCount + 1];
        const blue = palette[paletteValueIndex * colorComponentCount + 2];
        const alpha = palette[paletteValueIndex * colorComponentCount + 3];
        //console.log(`R ${red}, G ${green}, B ${blue}, A ${alpha}`);

        out[(y * width + x) * colorComponentCount + 0] = red;
        out[(y * width + x) * colorComponentCount + 1] = green;
        out[(y * width + x) * colorComponentCount + 2] = blue;
        out[(y * width + x) * colorComponentCount + 3] = alpha;

        x++;
        if (x >= width) {
          y++;
          x = 0;
          if (y >= height) {
            break;
          }
        }
      }
      runlength = -1;
      if (y >= height) {
        break;
      }
    } else {
      runlength = paletteValueIndex - 0x40; //if e was 0b01xxxxxx, e - 0x40 is runlen.
    }
  }

  return out;
}
