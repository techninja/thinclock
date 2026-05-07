/**
 * App router shell — manages view stack and realtime sync.
 * @module router
 */

import { html, define, router } from 'hybrids';
import HomeView from '#pages/home/home-view.js';

export default define({
  tag: 'app-router',
  stack: router(HomeView, { url: '/' }),
  render: {
    value: ({ stack }) => html`<div class="app-router">${stack}</div>`,
    shadow: false,
  },
});
