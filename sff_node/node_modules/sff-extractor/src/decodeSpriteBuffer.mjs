import { PNG } from 'pngjs';
import decodeRLE8 from './decodeRLE8.mjs';
import decodeLZ5 from './decodeLZ5.mjs';
import decodePNG8 from './decodePNG8.mjs';
import decodePCX from './decodePCX.mjs';

// The returned buffer is in RGBA, each color component encoded in 1 byte
export default function decodeSpriteBuffer(
  compressionMethod,
  buffer,
  width,
  height,
  palette
) {
  switch (compressionMethod) {
    default:
      throw new Error(`Unknown compression method: ${compressionMethod}`);
    case 'PCX':
      return decodePCX(buffer, width, height, palette);
    case 'RLE5':
      throw new Error(`TODO RLE5`);
      break;
    case 'RLE8':
      return decodeRLE8(buffer, width, height, palette);
    case 'LZ5':
      return decodeLZ5(buffer, width, height, palette);
    case 'PNG8':
      return decodePNG8(buffer, width, height, palette);
    case 'PNG24':
    case 'PNG32': {
      const png = PNG.sync.read(buffer.subarray(4));
      return png.data;
    }
  }
}
