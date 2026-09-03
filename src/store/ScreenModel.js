/**
 * Screen list store — fetches all screens with enable/disable state.
 * @module store/ScreenModel
 */

import { store } from 'hybrids';

/**
 * @typedef {Object} Screen
 * @property {string} id
 * @property {string} name
 * @property {boolean} enabled
 * @property {number} priority
 * @property {string[]} tags
 * @property {string} contextAction
 */

/** @type {import('hybrids').Model<Screen>} */
const ScreenModel = {
  id: true,
  name: '',
  enabled: false,
  active: false,
  pinned: false,
  priority: 0,
  tags: [''],
  schedule: '',
  contextAction: '',
  [store.connect]: {
    get: (id) =>
      fetch('/api/screens/')
        .then((r) => r.json())
        .then((list) => list.find((s) => s.id === id)),
    list: () =>
      fetch('/api/screens/')
        .then((r) => r.json())
        .then((list) =>
          list.map((s) => ({
            ...s,
            schedule: s.schedule
              ? typeof s.schedule === 'string'
                ? s.schedule
                : JSON.stringify(s.schedule)
              : '',
          })),
        ),
  },
};

export default ScreenModel;
