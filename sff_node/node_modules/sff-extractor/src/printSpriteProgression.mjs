import { stdout } from 'node:process';

export default function printSpriteProgression(index, total, spriteGroup, spriteNumber) {
  stdout.clearLine(0);
  stdout.cursorTo(0);
  stdout.write(`Sprite ${index + 1} / ${total}: ${spriteGroup},${spriteNumber}`);
}