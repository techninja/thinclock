/**
 * Server-side GIF encoder for 32×8 LED frames.
 * Takes raw RGB buffers, applies gamma/scale/gaps, outputs GIF binary.
 * @module api/lib/gif
 */

const WIDTH = 32, HEIGHT = 8;

// Fixed 6×6×6 palette
const PALETTE = Buffer.alloc(768);
let idx = 0;
for (let r = 0; r < 6; r++)
  for (let g = 0; g < 6; g++)
    for (let b = 0; b < 6; b++) {
      PALETTE[idx++] = r * 51;
      PALETTE[idx++] = g * 51;
      PALETTE[idx++] = b * 51;
    }
for (let i = 216; i < 256; i++) {
  const v = (i - 216) * 6 + 3;
  PALETTE[i * 3] = v; PALETTE[i * 3 + 1] = v; PALETTE[i * 3 + 2] = v;
}

function buildGammaLUT(gamma) {
  const lut = new Uint8Array(256);
  for (let i = 0; i < 256; i++) {
    lut[i] = Math.round(Math.pow(i / 255, 1 / gamma) * 255);
  }
  return lut;
}

function colorToIndex(r, g, b) {
  const ri = Math.min(5, Math.round(r / 51));
  const gi = Math.min(5, Math.round(g / 51));
  const bi = Math.min(5, Math.round(b / 51));
  return ri * 36 + gi * 6 + bi;
}

function buildScaledFrame(rgb, scale, gap, gammaLUT) {
  const outW = WIDTH * scale + (WIDTH - 1) * gap;
  const outH = HEIGHT * scale + (HEIGHT - 1) * gap;
  const indexed = new Uint8Array(outW * outH); // 0 = black (gaps)

  for (let y = 0; y < HEIGHT; y++) {
    for (let x = 0; x < WIDTH; x++) {
      const si = (y * WIDTH + x) * 3;
      const r = gammaLUT[rgb[si]];
      const g = gammaLUT[rgb[si + 1]];
      const b = gammaLUT[rgb[si + 2]];
      const ci = colorToIndex(r, g, b);
      const startX = x * (scale + gap);
      const startY = y * (scale + gap);
      for (let sy = 0; sy < scale; sy++) {
        for (let sx = 0; sx < scale; sx++) {
          indexed[(startY + sy) * outW + (startX + sx)] = ci;
        }
      }
    }
  }
  return indexed;
}

function writeLZW(pixels, out) {
  out.push(8); // min code size
  const clearCode = 256, eoiCode = 257;
  let bitBuf = 0, bitCount = 0;
  const blocks = [];
  let block = [];

  function flush() {
    while (bitCount >= 8) {
      block.push(bitBuf & 0xFF);
      bitBuf >>= 8; bitCount -= 8;
      if (block.length === 255) { blocks.push(block); block = []; }
    }
  }
  function emit(code) {
    bitBuf |= (code << bitCount);
    bitCount += 9;
    flush();
  }

  emit(clearCode);
  let since = 0;
  for (let i = 0; i < pixels.length; i++) {
    emit(pixels[i]);
    since++;
    if (since >= 254) { emit(clearCode); since = 0; }
  }
  emit(eoiCode);
  if (bitCount > 0) block.push(bitBuf & 0xFF);
  if (block.length > 0) blocks.push(block);

  for (const b of blocks) { out.push(b.length); out.push(...b); }
  out.push(0); // terminator
}

/**
 * Encode raw RGB frame buffers into an animated GIF.
 * @param {Buffer[]} frameBufs - Array of 768-byte RGB buffers
 * @param {number} scale - Pixel scale factor
 * @param {number} gap - Gap pixels between LEDs
 * @param {number} gamma - Gamma × 10 (18 = 1.8)
 * @returns {Buffer} GIF binary
 */
export function encodeGif(frameBufs, scale = 5, gap = 1, gamma = 18) {
  const gammaLUT = buildGammaLUT(gamma / 10);
  const outW = WIDTH * scale + (WIDTH - 1) * gap;
  const outH = HEIGHT * scale + (HEIGHT - 1) * gap;
  const delay = 7; // centiseconds (~15fps)

  const out = [];

  // Header
  out.push(0x47, 0x49, 0x46, 0x38, 0x39, 0x61); // GIF89a
  out.push(outW & 0xFF, (outW >> 8) & 0xFF);
  out.push(outH & 0xFF, (outH >> 8) & 0xFF);
  out.push(0xF7, 0, 0); // GCT 256 colors
  for (let i = 0; i < 768; i++) out.push(PALETTE[i]);
  // NETSCAPE loop
  out.push(0x21, 0xFF, 0x0B);
  out.push(...Buffer.from('NETSCAPE2.0'));
  out.push(0x03, 0x01, 0x00, 0x00, 0x00);

  let prevIndexed = null;
  let pendingDelay = delay;

  for (let f = 0; f < frameBufs.length; f++) {
    const indexed = buildScaledFrame(frameBufs[f], scale, gap, gammaLUT);

    // Frame dedup
    if (prevIndexed && Buffer.from(indexed).equals(Buffer.from(prevIndexed))) {
      pendingDelay += delay;
      continue;
    }

    if (prevIndexed) {
      // Write previous frame with accumulated delay
      out.push(0x21, 0xF9, 0x04, 0x04);
      out.push(pendingDelay & 0xFF, (pendingDelay >> 8) & 0xFF);
      out.push(0x00, 0x00);
      out.push(0x2C, 0, 0, 0, 0);
      out.push(outW & 0xFF, (outW >> 8) & 0xFF);
      out.push(outH & 0xFF, (outH >> 8) & 0xFF);
      out.push(0x00);
      writeLZW(prevIndexed, out);
      pendingDelay = delay;
    }

    prevIndexed = indexed;
  }

  // Write final frame
  if (prevIndexed) {
    out.push(0x21, 0xF9, 0x04, 0x04);
    out.push(pendingDelay & 0xFF, (pendingDelay >> 8) & 0xFF);
    out.push(0x00, 0x00);
    out.push(0x2C, 0, 0, 0, 0);
    out.push(outW & 0xFF, (outW >> 8) & 0xFF);
    out.push(outH & 0xFF, (outH >> 8) & 0xFF);
    out.push(0x00);
    writeLZW(prevIndexed, out);
  }

  out.push(0x3B); // trailer
  return Buffer.from(out);
}
