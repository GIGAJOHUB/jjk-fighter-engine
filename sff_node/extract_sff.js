const { SffExtractor } = require('sff-extractor');
const { PNG } = require('pngjs');
const fs = require('fs');
const path = require('path');

async function extract() {
    const sffPath = process.argv[2];
    const outDir = process.argv[3];

    if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });

    const extractor = new SffExtractor();
    const sprites = await extractor.extract(sffPath);

    let offsetText = '';
    for (const s of sprites) {
        if (!s.data) continue;
        
        const fileName = `g${s.group.toString().padStart(4, '0')}_i${s.item.toString().padStart(4, '0')}.png`;
        const filePath = path.join(outDir, fileName);
        
        fs.writeFileSync(filePath, s.data);
        offsetText += `${s.group} ${s.item} ${s.x} ${s.y}\n`;
    }

    fs.writeFileSync(path.join(outDir, 'offsets.txt'), offsetText);
    console.log(`Extracted ${sprites.length} sprites to ${outDir}`);
}

extract().catch(console.error);
