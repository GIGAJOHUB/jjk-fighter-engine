import fs from 'fs';
import path from 'path';

async function extract() {
    const sndPath = process.argv[2];
    const outDir = process.argv[3];
    if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });

    const buffer = fs.readFileSync(sndPath);
    const numSounds = buffer.readUInt32LE(16);
    let nextOffset = buffer.readUInt32LE(20);

    console.log(`Extracting ${numSounds} sounds from ${sndPath}...`);

    for (let i = 0; i < numSounds; i++) {
        if (nextOffset === 0) break;

        const currentOffset = nextOffset;
        nextOffset = buffer.readUInt32LE(currentOffset);
        const dataLen = buffer.readUInt32LE(currentOffset + 4);
        const group = buffer.readInt32LE(currentOffset + 8);
        const item = buffer.readInt32LE(currentOffset + 12);

        const data = buffer.subarray(currentOffset + 16, currentOffset + 16 + dataLen);
        
        const fileName = `s${group}_${item}.wav`;
        fs.writeFileSync(path.join(outDir, fileName), data);
    }

    console.log(`Successfully extracted sounds to ${outDir}`);
}

extract().catch(console.error);
