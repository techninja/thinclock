/**
 * Named schedule definitions — reusable time-based visibility rules.
 * Screens reference schedules by name. Users can override via data/schedules.json.
 * @module api/lib/schedules
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const DATA_FILE = path.join(__dirname, '..', '..', '..', 'data', 'schedules.json');

/** Default schedule presets */
const DEFAULTS = {
  night: { hours: [22, 23, 0, 1, 2, 3, 4, 5, 6] },
  evening: { hours: [18, 19, 20, 21, 22, 23] },
  work: { hours: [8, 9, 10, 11, 12, 13, 14, 15, 16, 17], days: [1, 2, 3, 4, 5] },
  daytime: { hours: [7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18] },
  weekend: { days: [0, 6] },
  winter: { months: [11, 12, 1, 2] },
  summer: { months: [6, 7, 8] },
  christmas: { months: [12], days_of_month: [20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31] },
  july4th: { dateRange: [701, 704] },
  halloween: { dateRange: [1025, 1031] },
  newyear: { dateRange: [1231, 101] },
};

let userOverrides = {};

/** Load user overrides from disk */
export function loadSchedules() {
  try {
    if (fs.existsSync(DATA_FILE)) {
      userOverrides = JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
    }
  } catch (e) {
    console.warn('[schedules] Failed to load overrides:', e.message);
  }
}

/** Save user overrides to disk */
export function saveSchedules() {
  fs.mkdirSync(path.dirname(DATA_FILE), { recursive: true });
  fs.writeFileSync(DATA_FILE, JSON.stringify(userOverrides, null, 2));
}

/** Get all schedules (defaults merged with user overrides) */
export function getSchedules() {
  return { ...DEFAULTS, ...userOverrides };
}

/** Get a single schedule by name (resolved) */
export function getSchedule(name) {
  if (!name) return null;
  return userOverrides[name] || DEFAULTS[name] || null;
}

/** Set/update a named schedule */
export function setSchedule(name, definition) {
  userOverrides[name] = definition;
  saveSchedules();
}

/** Delete a user-defined schedule (can't delete defaults, only override) */
export function deleteSchedule(name) {
  delete userOverrides[name];
  saveSchedules();
}

/**
 * Resolve a screen's schedule field to a schedule object.
 * Accepts: string name, inline object, or null.
 */
export function resolveSchedule(schedule) {
  if (!schedule) return null;
  if (typeof schedule === 'string') return getSchedule(schedule);
  return schedule; // inline object
}

/**
 * Evaluate if a schedule passes right now.
 * @param {object|string|null} schedule
 * @returns {boolean}
 */
export function isScheduleActive(schedule) {
  const s = resolveSchedule(schedule);
  if (!s) return true; // no schedule = always active
  const now = new Date();
  if (s.months?.length > 0) {
    if (!s.months.includes(now.getMonth() + 1)) return false;
  }
  if (s.hours?.length > 0) {
    if (!s.hours.includes(now.getHours())) return false;
  }
  if (s.days?.length > 0) {
    if (!s.days.includes(now.getDay())) return false;
  }
  if (s.days_of_month?.length > 0) {
    if (!s.days_of_month.includes(now.getDate())) return false;
  }
  if (s.dateRange) {
    const mmdd = (now.getMonth() + 1) * 100 + now.getDate();
    const [start, end] = s.dateRange;
    if (start <= end) {
      if (mmdd < start || mmdd > end) return false;
    } else {
      // Wraps around year (e.g. Dec 31 - Jan 1)
      if (mmdd < start && mmdd > end) return false;
    }
  }
  return true;
}

// Load on import
loadSchedules();
