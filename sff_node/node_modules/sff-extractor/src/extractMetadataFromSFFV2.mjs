export default function extractMetadataFromSFFV2(buffer) {
  // Version
  const build = buffer.readUInt8(12);
  const patch = buffer.readUInt8(13);
  const minor = buffer.readUInt8(14);
  const major = buffer.readUInt8(15);
  const version = `${major}.${minor}.${patch}.${build}`;

  const paletteMapOffset = buffer.readUInt32LE(0x1a);
  const spriteListOffset = buffer.readUInt32LE(0x24);
  const spriteCount = buffer.readUInt32LE(0x28);
  const paletteCount = buffer.readUInt32LE(0x30);
  const paletteBankOffset = buffer.readUInt32LE(0x34);
  const onDemandDataSize = buffer.readUInt32LE(0x38);
  const onDemandDataSizeTotal = buffer.readUInt32LE(0x3c);
  const onLoadDataSize = buffer.readUInt32LE(0x40);

  // ??? between 0x44 and paletteMapOffset

  return {
    version,
    versionMajor: major,
    versionMinor: minor,
    versionPath: patch,
    versionBuild: build,
    paletteMapOffset,
    spriteListOffset,
    spriteCount,
    paletteCount,
    paletteBankOffset,
    onDemandDataSize,
    onDemandDataSizeTotal,
    onLoadDataSize
  };
}