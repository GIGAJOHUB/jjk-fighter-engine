export default function convertPaletteRGBABufferToTable(buffer) {
  const table = [];
  for (let colorIndex = 0; colorIndex < buffer.length; colorIndex += 4) {
    const red = buffer[colorIndex];
    const green = buffer[colorIndex + 1];
    const blue = buffer[colorIndex + 2];
    const alpha = buffer[colorIndex + 3];
    table.push([red, green, blue, alpha]);
  }
  return table;
}