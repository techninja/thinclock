/**
 * User preferences — singleton, persisted to localStorage.
 * Display preferences that don't need an API.
 * @module store/UserPrefs
 */

import { store } from 'hybrids';

/**
 * @typedef {Object} UserPrefs
 * @property {'board'|'list'} defaultView - Preferred task view mode
 * @property {boolean} compactMode - Dense UI toggle
 */

/** @type {import('hybrids').Model<UserPrefs>} */
const UserPrefs = {
  defaultView: 'list',
  compactMode: false,
  [store.connect]: {
    get: () => {
      const raw = localStorage.getItem('userPrefs');
      return raw ? JSON.parse(raw) : {};
    },
    set: (id, values) => {
      localStorage.setItem('userPrefs', JSON.stringify(values));
      return values;
    },
  },
};

export default UserPrefs;
