import extract from 'sff-extractor';
import fs from 'fs';
import path from 'path';
import { PNG } from 'pngjs';

async function run() {
    const sffPath = process.argv[2];
    const outDir = process.argv[3];
    if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });

    console.log(`Reading ${sffPath}...`);
    const buffer = fs.readFileSync(sffPath);
    const data = extract(buffer, { decodeSpriteBuffer: true });

    console.log(`Extracting ${data.sprites.length} sprites...`);
    let offsetText = '';
    for (const s of data.sprites) {
        if (!s.decodedBuffer) continue;
        
        const fileName = `g${s.group.toString().padStart(4, '0')}_i${s.number.toString().padStart(4, '0')}.png`;
        const filePath = path.join(outDir, fileName);
        
        const png = new PNG({ width: s.width, height: s.height });
        png.data = s.decodedBuffer;
        const outBuffer = PNG.sync.write(png, { colorType: 6 });
        fs.writeFileSync(filePath, outBuffer);
        
        offsetText += `${s.group} ${s.number} ${s.x} ${s.y}\n`;
    }

    fs.writeFileSync(path.join(outDir, 'offsets.txt'), offsetText);
    console.log(`Successfully extracted sprites to ${outDir}`);
}

run().catch(console.error);
