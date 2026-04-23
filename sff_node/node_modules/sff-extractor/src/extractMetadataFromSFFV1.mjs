export default function extractMetadataFromSFFV1(buffer) {
  // Version
  const build = buffer.readUInt8(12);
  const patch = buffer.readUInt8(13);
  const minor = buffer.readUInt8(14);
  const major = buffer.readUInt8(15);
  const version = `${major}.${minor}.${patch}.${build}`;

  const groupCount = buffer.readUInt32LE(16);
  const spriteCount = buffer.readUInt32LE(20);
  const firstSpriteOffset = buffer.readUInt32LE(24);
  const subHeaderSize = buffer.readUInt32LE(28);

  // 0 = individual
  // 1 = shared
  const paletteType = buffer.readUInt8(32);
  
  // 33 -> 36 = blank
  /*
  const unknown1 = buffer.readUInt8(33);
  const unknown2 = buffer.readUInt8(34);
  const unknown3 = buffer.readUInt8(35);
  */

  const comment = buffer.toString('ascii', 36, 512);

  return {
    version,
    versionMajor: major,
    versionMinor: minor,
    versionPath: patch,
    versionBuild: build,
    groupCount,
    spriteCount,
    firstSpriteOffset,
    subHeaderSize,
    paletteType,
    comment,
  };
}