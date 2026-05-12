/**
 * App nav — main navigation links.
 * @module components/molecules/app-nav
 */

import { html, define } from 'hybrids';

/** @type {import('hybrids').Component<{}>} */
export default define({
  tag: 'app-nav',
  render: {
    value: () => html`
      <nav class="app-nav">
        <a href="/" class="nav-link">Dashboard</a>
        <a href="/rotation" class="nav-link">Rotation</a>
        <a href="/editor" class="nav-link">Editor</a>
        <a href="/settings" class="nav-link">Settings</a>
        <a href="/notify" class="nav-link">Notify</a>
      </nav>
    `,
    shadow: false,
  },
});
