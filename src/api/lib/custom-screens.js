/**
 * Custom screens — CRUD for user-created screen JSON files.
 * @module api/lib/custom-screens
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const DIR = path.join(__dirname, '..', '..', '..', 'data', 'custom-screens');
fs.mkdirSync(DIR, { recursive: true });

/** List all custom screens (metadata only) */
export function listCustomScreens() {
  return fs
    .readdirSync(DIR)
    .filter((f) => f.endsWith('.json'))
    .map((f) => {
      const data = JSON.parse(fs.readFileSync(path.join(DIR, f), 'utf8'));
      return { id: f.replace('.json', ''), name: data.name || f, layers: data.layers?.length || 0 };
    });
}

/** Get a custom screen by ID */
export function getCustomScreen(id) {
  const file = path.join(DIR, `${id}.json`);
  if (!fs.existsSync(file)) return null;
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

/** Save a custom screen */
export function saveCustomScreen(id, data) {
  const file = path.join(DIR, `${id}.json`);
  fs.writeFileSync(file, JSON.stringify(data, null, 2));
}

/** Delete a custom screen */
export function deleteCustomScreen(id) {
  const file = path.join(DIR, `${id}.json`);
  if (fs.existsSync(file)) fs.unlinkSync(file);
}
