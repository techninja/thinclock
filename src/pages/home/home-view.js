/**
 * Home page — starting point for your app.
 * @module pages/home
 */

import { html, define } from 'hybrids';

export default define({
  tag: 'home-view',
  render: {
    value: () => html`
      <div class="home-view">
        <h1>thinclock</h1>
        <p>Your Clearstack project is ready. Start building!</p>
      </div>
    `,
    shadow: false,
  },
});
