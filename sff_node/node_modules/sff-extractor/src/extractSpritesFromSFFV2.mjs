function stringifyCompressionMethod(value) {
  switch (value) {
    default:
      return "unknown";
    case 0:
      return "RAW";
    case 1:
      return "LINKED";
    case 2:
      return "RLE8";
    case 3:
      return "RLE5";
    case 4:
      return "LZ5";
    case 0x0a:
      return "PNG8";
    case 0x0b:
      return "PNG24";
    case 0x0c:
      return "PNG32";
  }
}

export default function extractSpritesFromSFFV2(data, metadata, palettes) {
  // Sprite List, linear 28bytes per node
  const spriteNodeSize = 28;
  const sprites = [];
  for (let spriteIndex = 0; spriteIndex < metadata.spriteCount; spriteIndex++) {
    const group = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize,
    );

    const number = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x02,
    );

    const width = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x04,
    );
    const height = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x06,
    );

    const x = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x08,
    );
    const y = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x0a,
    );

    const linkedIndex = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x0c,
    );

    if (linkedIndex > 0) {
      sprites.push({
        index: spriteIndex,
        group,
        number,
        width,
        height,
        x,
        y,
        linkedIndex,
      });
      continue;
    }

    const compressionMethodValue = data.readUInt8(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x0e,
    );
    const compressionMethod = stringifyCompressionMethod(
      compressionMethodValue,
    );

    const colorDepth = data.readUInt8(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x0f,
    );

    const dataOffset = data.readUInt32LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x10,
    );
    const dataLength = data.readUInt32LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x14,
    );

    const paletteIndex = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x18,
    );

    // 0 = OnDemand (default)
    // 1 = OnLoad
    const loadMode = data.readUInt16LE(
      metadata.spriteListOffset + spriteIndex * spriteNodeSize + 0x1a,
    );

    // If the linked index and the length are 0
    // Then copy the first sprite
    if (dataLength === 0) {
      sprites.push({
        ...sprites[0],
        index: spriteIndex,
        group,
        number,
        x,
        y,
      });
      continue;
    }
    let spriteBuffer = data.subarray(
      metadata.paletteBankOffset + dataOffset,
      metadata.paletteBankOffset + dataOffset + dataLength,
    );
    if (loadMode === 1) {
      spriteBuffer = data.subarray(
        metadata.onDemandDataSizeTotal + dataOffset,
        metadata.onDemandDataSizeTotal + dataOffset + dataLength,
      );
    }

    sprites.push({
      index: spriteIndex,
      group,
      number,
      width,
      height,
      x,
      y,
      linkedIndex,
      compressionMethod,
      colorDepth,
      dataOffset,
      dataLength,
      paletteIndex,
      loadMode,
      buffer: spriteBuffer,
    });
  }

  return sprites;
}
