/**
 * WebSocket render queue — device connection, job queue, browser framebuffer relay.
 * @module api/lib/ws-render
 */

import { WebSocketServer } from 'ws';

let deviceWs = null;
let deviceIP = null;
let currentJob = null;
let jobFrames = [];
const renderQueue = [];

export const wssBrowser = new WebSocketServer({ noServer: true });
export const wssDevice = new WebSocketServer({ noServer: true });

wssDevice.on('connection', (ws, req) => {
  deviceIP = req.socket.remoteAddress?.replace('::ffff:', '') || null;
  console.log(`[ws/device] connected ${deviceIP}`);
  deviceWs = ws;
  ws.on('message', (data, isBinary) => {
    if (isBinary) {
      if (currentJob) {
        jobFrames.push(Buffer.from(data));
        if (jobFrames.length >= currentJob.frames) finishJob();
      } else {
        for (const c of wssBrowser.clients) if (c.readyState === 1) c.send(data);
      }
    } else {
      try {
        const m = JSON.parse(data);
        if (m.type === 'done') finishJob();
        else if (m.type === 'error' || m.type === 'busy') failJob(m.msg || m.type);
      } catch {
        /* ignore malformed */
      }
    }
  });
  ws.on('close', () => {
    console.log('[ws/device] disconnected');
    deviceWs = null;
    deviceIP = null;
    if (currentJob) failJob('disconnected');
  });
});

wssBrowser.on('connection', () => {});

/** @param {object} req @param {object} socket @param {object} head */
export function handleUpgrade(req, socket, head) {
  if (req.url === '/ws/device') {
    wssDevice.handleUpgrade(req, socket, head, (ws) => wssDevice.emit('connection', ws, req));
  } else if (req.url === '/ws/framebuffer') {
    wssBrowser.handleUpgrade(req, socket, head, (ws) => wssBrowser.emit('connection', ws, req));
  } else {
    socket.destroy();
  }
}

/** @returns {string|null} */
export function getConnectedDeviceIP() { return deviceIP; }

/** @param {object} command */
export function queueRender(command) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => failJob('timeout'), 30000);
    renderQueue.push({
      command,
      frames: command.frames || 30,
      resolve: (f) => {
        clearTimeout(timeout);
        resolve(f);
      },
      reject: (e) => {
        clearTimeout(timeout);
        reject(e);
      },
    });
    processQueue();
  });
}

/** @returns {void} */
function finishJob() {
  if (!currentJob) return;
  currentJob.resolve(jobFrames);
  currentJob = null;
  jobFrames = [];
  processQueue();
}

/** @param {string} r */
function failJob(r) {
  if (!currentJob) return;
  currentJob.reject(new Error(r));
  currentJob = null;
  jobFrames = [];
  processQueue();
}

/** @returns {void} */
function processQueue() {
  if (currentJob || !renderQueue.length || !deviceWs) return;
  currentJob = renderQueue.shift();
  jobFrames = [];
  deviceWs.send(JSON.stringify(currentJob.command));
}
