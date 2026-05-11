/**
 * Particle system — port of firmware particle physics.
 * @module utils/renderer/particles
 */

import { setPixel, addPixel, colorFromStops, WIDTH, HEIGHT } from './core.js';

const MAX_PARTICLES = 48;

/** Initialize particle state for a layer */
export function initParticles(layer) {
  const particles = [];
  for (let i = 0; i < MAX_PARTICLES; i++) {
    particles.push({ alive: false, x: 0, y: 0, vx: 0, vy: 0, age: 0, lifetime: 0, size: 1, color: [255,255,255] });
  }
  const state = { particles, emitters: (layer.emitters || []).map(e => ({ ...e, acc: 0 })) };
  // Warmup
  const warmup = layer.warmup || 0;
  if (warmup > 0) {
    const steps = Math.floor(warmup / 20);
    for (let i = 0; i < steps; i++) tickParticles(state, layer, 20);
  }
  return state;
}

function randomColor(colors) {
  if (!colors?.stops?.length) return [255, 255, 255];
  const pos = Math.random();
  const val = (colors.min || 0) + pos * ((colors.max || 1) - (colors.min || 0));
  return colorFromStops(colors.stops, val);
}

function spawn(state, em, colors) {
  for (const p of state.particles) {
    if (p.alive) continue;
    p.alive = true;
    p.x = em.x < 0 ? Math.random() * WIDTH : em.x;
    p.y = em.y < 0 ? Math.random() * HEIGHT : em.y;
    p.vx = em.vx_min + Math.random() * (em.vx_max - em.vx_min);
    p.vy = em.vy_min + Math.random() * (em.vy_max - em.vy_min);
    p.lifetime = em.life_min + Math.random() * (em.life_max - em.life_min);
    p.age = 0;
    p.size = em.size || 1;
    p.color = randomColor(colors);
    return;
  }
}

/** Advance particle simulation by dt_ms */
export function tickParticles(state, layer, dt_ms = 16) {
  const dt = dt_ms / 1000;
  const gravity = layer.gravity || 0;
  const edge = layer.edge || 'die';

  for (const em of state.emitters) {
    em.acc += (em.rate || 1) * dt;
    while (em.acc >= 1) {
      spawn(state, em, layer.colors);
      em.acc -= 1;
    }
  }

  for (const p of state.particles) {
    if (!p.alive) continue;
    p.age += dt_ms;
    if (p.age >= p.lifetime) { p.alive = false; continue; }
    p.vy += gravity * dt;
    p.x += p.vx * dt;
    p.y += p.vy * dt;

    if (edge === 'bounce') {
      if (p.x < 0) { p.x = 0; p.vx *= -0.8; }
      if (p.x >= WIDTH - 1) { p.x = WIDTH - 1.01; p.vx *= -0.8; }
      if (p.y < 0) { p.y = 0; p.vy *= -0.8; }
      if (p.y >= HEIGHT - 1) { p.y = HEIGHT - 1.01; p.vy *= -0.8; }
    } else if (edge === 'wrap') {
      if (p.x < 0) p.x += WIDTH;
      if (p.x >= WIDTH) p.x -= WIDTH;
      if (p.y < 0) p.y += HEIGHT;
      if (p.y >= HEIGHT) p.y -= HEIGHT;
    } else {
      if (p.x < -2 || p.x >= WIDTH + 2 || p.y < -2 || p.y >= HEIGHT + 2) {
        p.alive = false;
      }
    }
  }
}

/** Render particles to buffer */
export function renderParticles(buf, layer, _icons, state) {
  if (!state._particles) state._particles = initParticles(layer);
  tickParticles(state._particles, layer);

  const blend = layer.blend === 'add' ? addPixel : setPixel;
  for (const p of state._particles.particles) {
    if (!p.alive) continue;
    let [r, g, b] = p.color;
    const progress = p.age / p.lifetime;
    if (progress > 0.75) {
      const fade = 1 - (progress - 0.75) * 4;
      r = Math.round(r * fade);
      g = Math.round(g * fade);
      b = Math.round(b * fade);
    }
    const px = Math.floor(p.x), py = Math.floor(p.y);
    blend(buf, px, py, r, g, b);
    if (p.size >= 2) {
      blend(buf, px + 1, py, r, g, b);
      blend(buf, px, py + 1, r, g, b);
      blend(buf, px + 1, py + 1, r, g, b);
    }
  }
}
