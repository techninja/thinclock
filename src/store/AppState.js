/**
 * Global UI state — singleton, persisted to localStorage.
 * Theme, sidebar, active filters. No API calls.
 * @module store/AppState
 */

import { store } from 'hybrids';

/**
 * @typedef {Object} AppState
 * @property {string} theme - 'light' or 'dark'
 * @property {boolean} sidebarOpen - Sidebar visibility
 * @property {string} activeFilter - Current task filter value
 */

/** @type {import('hybrids').Model<AppState>} */
const AppState = {
  theme: 'light',
  sidebarOpen: true,
  activeFilter: 'all',
  [store.connect]: {
    get: () => {
      const raw = localStorage.getItem('appState');
      return raw ? JSON.parse(raw) : {};
    },
    set: (id, values) => {
      localStorage.setItem('appState', JSON.stringify(values));
      return values;
    },
  },
};

export default AppState;
