/**
 * Live MJPEG-style stream route — taps the WS framebuffer relay.
 * Mounted at /api/device/stream by device-proxy.js.
 */

import { encodeGif } from './gif.js';
import { frameListeners } from './ws-render.js';

export function registerStreamRoute(app) {
  app.get('/api/device/stream', (req, res) => {
    res.set({
      'Content-Type': 'multipart/x-mixed-replace; boundary=frame',
      'Cache-Control': 'no-store',
      Connection: 'keep-alive',
    });
    res.flushHeaders();

    function push(data) {
      const frame = encodeGif([data], 5, 1, 18);
      res.write('--frame\r\nContent-Type: image/gif\r\n\r\n');
      res.write(frame);
      res.write('\r\n');
    }

    frameListeners.add(push);
    req.on('close', () => frameListeners.delete(push));
  });
}
