/**
 * LZW encoder for GIF image data blocks.
 * @module api/lib/lzw
 */

/**
 * Write LZW-compressed pixel data into out array.
 * @param {Uint8Array} pixels
 * @param {number[]} out
 */
export function writeLZW(pixels, out) {
  out.push(8); // min code size
  const clearCode = 256,
    eoiCode = 257;
  let bitBuf = 0,
    bitCount = 0;
  const blocks = [];
  let block = [];

  /** @returns {void} */
  function flush() {
    while (bitCount >= 8) {
      block.push(bitBuf & 0xff);
      bitBuf >>= 8;
      bitCount -= 8;
      if (block.length === 255) {
        blocks.push(block);
        block = [];
      }
    }
  }

  /** @param {number} code */
  function emit(code) {
    bitBuf |= code << bitCount;
    bitCount += 9;
    flush();
  }

  emit(clearCode);
  let since = 0;
  for (let i = 0; i < pixels.length; i++) {
    emit(pixels[i]);
    if (++since >= 254) {
      emit(clearCode);
      since = 0;
    }
  }
  emit(eoiCode);
  if (bitCount > 0) block.push(bitBuf & 0xff);
  if (block.length > 0) blocks.push(block);
  for (const b of blocks) {
    out.push(b.length);
    out.push(...b);
  }
  out.push(0);
}
