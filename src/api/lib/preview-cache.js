/**
 * Preview disk cache — queue, generate, and serve GIF previews.
 * @module api/lib/preview-cache
 */

import fs from 'fs';
import path from 'path';

export class PreviewCache {
  /** @param {string} cacheDir */
  constructor(cacheDir) {
    this.dir = cacheDir;
    fs.mkdirSync(cacheDir, { recursive: true });
    this._queue = [];
    this._running = false;
    this._generate = null; // set by server after init
  }

  /** @param {string} id */
  has(id) {
    return fs.existsSync(path.join(this.dir, `${id}.gif`));
  }

  /** @param {string} id */
  enqueue(id) {
    if (!this._queue.includes(id)) this._queue.push(id);
    this._run();
  }

  /** @param {Array} modules */
  enqueueAll(modules) {
    for (const mod of modules) if (!this.has(mod._id)) this.enqueue(mod._id);
  }

  clear() {
    for (const f of fs.readdirSync(this.dir)) fs.unlinkSync(path.join(this.dir, f));
  }

  /**
   * Serve cached GIF or queue generation and return 202.
   * @param {string} id
   * @param {object} res
   */
  serve(id, res) {
    const file = path.join(this.dir, `${id}.gif`);
    if (fs.existsSync(file)) {
      res.set('Content-Type', 'image/gif');
      res.set('Cache-Control', 'public, max-age=300');
      return res.sendFile(file);
    }
    this.enqueue(id);
    res.status(202).json({ status: 'generating' });
  }

  async _run() {
    if (this._running || !this._queue.length) return;
    this._running = true;
    const id = this._queue.shift();
    console.log(`[preview] generating ${id} (${this._queue.length} remaining)`);
    const buf = await (this._generate?.(id) ?? Promise.resolve(null));
    if (buf?.length > 10) fs.writeFileSync(path.join(this.dir, `${id}.gif`), buf);
    await new Promise((r) => setTimeout(r, 500));
    this._running = false;
    this._run();
  }
}
