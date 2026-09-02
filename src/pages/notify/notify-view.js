/**
 * Notifications page — send notifications, manage timer, test beep.
 * @module pages/notify
 */

import { html, define, router } from 'hybrids';
import '#molecules/app-nav/index.js';
function sendNotify(host) {
  import('#utils/device.js').then(({ devicePost }) => {
    devicePost('/notify', {
      text: host._text,
      color: host._color,
      beep: host._beep,
    })
      .then(() => {
        host._status = 'Sent!';
      })
      .catch(() => {
        host._status = 'Failed';
      });
  });
}
function startTimer(host) {
  const seconds = parseInt(host._timerMin) * 60;
  if (!seconds) return;
  import('#utils/device.js').then(({ devicePost }) => {
    devicePost('/timer', { seconds }).then(() => {
      host._status = 'Timer started';
    });
  });
}
function testBeep(host) {
  import('#utils/device.js').then(({ devicePost }) => {
    devicePost('/beep', { pattern: host._pattern });
  });
}

export default define({
  tag: 'notify-view',
  [router.connect]: { url: '/notify' },
  _text: '',
  _color: '#ff0000',
  _beep: true,
  _timerMin: '5',
  _pattern: 'default',
  _status: '',
  render: {
    value: ({ _text, _color, _beep, _timerMin, _pattern, _status }) => html`
      <app-nav></app-nav>
      <div class="page-notify">
        <h1>Notifications</h1>
        ${_status ? html`<p class="success-message">${_status}</p>` : html``}
        <section class="card">
          <h2>Send Notification</h2>
          <div class="form-group">
            <label>Text</label>
            <input
              type="text"
              value="${_text}"
              oninput="${html.set('_text')}"
              placeholder="Hello!"
            />
          </div>
          <div class="form-row">
            <div class="form-group">
              <label>Color</label>
              <input type="color" value="${_color}" oninput="${html.set('_color')}" />
            </div>
            <div class="form-group">
              <label
                ><input type="checkbox" checked="${_beep}" onchange="${html.set('_beep')}" />
                Beep</label
              >
            </div>
          </div>
          <button class="btn btn-primary" onclick="${sendNotify}">Send</button>
        </section>
        <section class="card">
          <h2>Timer</h2>
          <div class="form-row">
            <input
              type="number"
              value="${_timerMin}"
              oninput="${html.set('_timerMin')}"
              min="1"
              max="99"
            />
            <span>minutes</span>
            <button class="btn btn-secondary" onclick="${startTimer}">Start Timer</button>
          </div>
        </section>
        <section class="card">
          <h2>Test Beep</h2>
          <div class="form-row">
            <select value="${_pattern}" onchange="${html.set('_pattern')}">
              <option value="default">Default</option>
              <option value="alert">Alert</option>
              <option value="success">Success</option>
            </select>
            <button class="btn btn-secondary" onclick="${testBeep}">Beep</button>
          </div>
        </section>
      </div>
    `,
    shadow: false,
  },
});
