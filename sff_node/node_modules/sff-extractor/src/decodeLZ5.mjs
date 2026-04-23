export default function decodeLZ5(rle, width, height, palette) {
  if (rle.length === 0) {
    return rle;
  }

  const rawDataOffset = 4; // first 4 octet represents uncompressed length of data
  const length = rle.readUInt32LE(0);
  //console.log('image length:', length);
  const encoded = rle.subarray(rawDataOffset);
  //console.log('encoded length:', encoded.length);

    let decoded = Buffer.alloc(width * height, 0);
    let i = 0;
    let j = 0;
    let n = 0;
    let ct = encoded[i];
    let cts = 0;
    let rb = 0;
    let rbc = 0;
    if (i < encoded.length - 1) {
      i++;
    }
    while (j < decoded.length) {
      let d = encoded[i];
      if (i < encoded.length - 1) {
        i++;
      }
      if ((ct & (1 << cts)) !== 0) {
        if ((d & 0x3f) === 0) {
          d = (d << 2 | encoded[i]) + 1;
          if (i < encoded.length - 1) {
            i++;
          }
          n = encoded[i] + 2;
          if (i < encoded.length - 1) {
            i++;
          }
        } else {
          rb |= (d & 0xc0) >> rbc;
          rbc += 2;
          n = d & 0x3f;
          if (rbc < 8) {
            d = encoded[i] + 1;
            if (i < encoded.length - 1) {
              i++;
            }
          } else {
            d = rb + 1;
            rb = 0;
            rbc = 0;
          }
        }
        while (true) {
          if (j < decoded.length) {
            decoded[j] = decoded[j - d];
            j++;
          }
          n--;
          if (n < 0) {
            break;
          }
        }
      } else {
        if ((d & 0xe0) === 0) {
          n = encoded[i] + 8;
          if (i < encoded.length - 1) {
            i++;
          }
        } else {
          n = d >> 5;
          d &= 0x1f;
        }
        for (; n > 0; n--) {
          if (j < decoded.length) {
            decoded[j] = d;
            j++;
          }
        }
      }
      cts++;
      if (cts >= 8) {
        ct = encoded[i];
        cts = 0;
        if (i < encoded.length - 1) {
          i++;
        }
      }
    }


  //console.log('Pixel count:', decoded.length);
  /*
  for (let a = 0; a < decoded.length; a++) {
    console.log(decoded[a]);
  }
  //*/

  const colorComponentCount = 4;
  const out = Buffer.alloc(width * height * colorComponentCount, 0);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      let colorIndex = decoded[y * width + x];
      //console.log(colorIndex);
      
      let red   = palette[colorIndex * 4 + 0];
      let green = palette[colorIndex * 4 + 1];
      let blue  = palette[colorIndex * 4 + 2];
      let alpha  = palette[colorIndex * 4 + 3];
   

      /*
      if (colorIndex === 0) {
        red = 255;
        green = 0;
        blue = 0;
        alpha = 255;
      } else {
        red = 0;
        green = 0;
        blue = 255;
        alpha = 255;
      }
      //*/

      let bufferIndex = (y * width + x) * 4;
      out[bufferIndex + 0] = red;
      out[bufferIndex + 1] = green;
      out[bufferIndex + 2] = blue;
      out[bufferIndex + 3] = alpha;
    }
  }
  return out;
}
