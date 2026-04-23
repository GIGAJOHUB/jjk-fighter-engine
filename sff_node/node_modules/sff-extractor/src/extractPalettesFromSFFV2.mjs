export default function extractPalettesFromSFFV2(buffer, metadata) {
  // Palette Map, linear 16bytes per map
  const paletteMapSize = 16;
  const palettes = [];
  for (
    let paletteIndex = 0;
    paletteIndex < metadata.paletteCount;
    paletteIndex++
  ) {
    const group = buffer.readUInt16LE(
      metadata.paletteMapOffset + paletteIndex * paletteMapSize,
    );

    const number = buffer.readUInt16LE(
      metadata.paletteMapOffset + paletteIndex * paletteMapSize + 0x02,
    );

    const colorCount = buffer.readUInt16LE(
      metadata.paletteMapOffset + paletteIndex * paletteMapSize + 0x04,
    );
    const copyPaletteIndex = buffer.readUInt16LE(
      metadata.paletteMapOffset + paletteIndex * paletteMapSize + 0x06,
    );

    const dataOffset = buffer.readUInt32LE(
      metadata.paletteMapOffset + paletteIndex * paletteMapSize + 0x08,
    );
    const dataLength = buffer.readUInt32LE(
      metadata.paletteMapOffset + paletteIndex * paletteMapSize + 0x0c,
    );

    let paletteBuffer = buffer.subarray(
      metadata.paletteBankOffset + dataOffset,
      metadata.paletteBankOffset + dataOffset + dataLength,
    );

    // When the dataLength is zero
    // It means this is a copy of another palette
    if (dataLength === 0 && palettes.length > 0) {
      if (copyPaletteIndex > 0) {
        // Copy the previous palette
        paletteBuffer = palettes[copyPaletteIndex].buffer;
      } else {
        // Otherwise, use the first palette
        paletteBuffer = palettes[0].buffer;
      }
    }

    palettes.push({
      index: paletteIndex,
      group,
      number,
      colorCount,
      copyPaletteIndex,
      dataOffset,
      dataLength,
      buffer: paletteBuffer,
    });
  }

  return palettes;
}
