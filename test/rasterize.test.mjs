import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { createRequire } from 'node:module';

const { rasterizePoints } = createRequire(import.meta.url)('..');
const OPTIONS = { fov: 45, distScale: 1.6, zUp: true };

const cube = (side) => {
    const positions = new Float32Array(side ** 3 * 3);
    let offset = 0;
    for(let x = 0; x < side; x++){
        for(let y = 0; y < side; y++){
            for(let z = 0; z < side; z++){
                positions.set([x * 2.5, y * 2.5, z * 2.5], offset);
                offset += 3;
            }
        }
    }
    return positions;
};

const solid = (count, rgba) => {
    const colors = new Uint8Array(count * 4);
    for(let index = 0; index < count; index++){
        colors.set(rgba, index * 4);
    }
    return colors;
};

const render = (directory, name, positions, colors) => {
    const pngPath = path.join(directory, `${name}.png`);
    assert.equal(rasterizePoints(positions, colors, pngPath, 320, 200, 45, 25, OPTIONS), true);
    return readFileSync(pngPath);
};

test('a point cloud renders to a PNG of the requested size in its own colours', () => {
    const directory = mkdtempSync(path.join(tmpdir(), 'headless-rasterizer-'));
    try{
        const positions = cube(12);
        const count = positions.length / 3;
        const red = render(directory, 'red', positions, solid(count, [255, 0, 0, 255]));
        const blue = render(directory, 'blue', positions, solid(count, [0, 0, 255, 255]));

        assert.deepEqual([...red.subarray(0, 8)], [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
        assert.equal(red.readUInt32BE(16), 320);
        assert.equal(red.readUInt32BE(20), 200);
        assert.notDeepEqual(red, blue);
    }finally{
        rmSync(directory, { recursive: true, force: true });
    }
});

test('arguments that do not describe a point cloud are rejected', () => {
    assert.throws(() => rasterizePoints(new Float32Array(4), null, 'x.png', 8, 8, 0, 0, OPTIONS), TypeError);
    assert.throws(() => rasterizePoints(new Float32Array(3), new Uint8Array(3), 'x.png', 8, 8, 0, 0, OPTIONS), TypeError);
});
