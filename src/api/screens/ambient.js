export const name = 'Ambient Gradient';
export const enabled = true;
export const priority = 1;
export const tags = ['ambient', 'mood'];
export const schedule = 'evening';

export const screen = (config) => {
  // Slowly rotate hue based on time — shifts every config poll (30s)
  const t = (Date.now() / 120000) % 1; // cycles every 2 minutes

  function hslToHex(h) {
    const s = 0.8,
      l = 0.35;
    const c = (1 - Math.abs(2 * l - 1)) * s;
    const x = c * (1 - Math.abs(((h * 6) % 2) - 1));
    const m = l - c / 2;
    let r, g, b;
    const i = Math.floor(h * 6);
    if (i === 0) {
      r = c;
      g = x;
      b = 0;
    } else if (i === 1) {
      r = x;
      g = c;
      b = 0;
    } else if (i === 2) {
      r = 0;
      g = c;
      b = x;
    } else if (i === 3) {
      r = 0;
      g = x;
      b = c;
    } else if (i === 4) {
      r = x;
      g = 0;
      b = c;
    } else {
      r = c;
      g = 0;
      b = x;
    }
    const toHex = (v) =>
      Math.round((v + m) * 255)
        .toString(16)
        .padStart(2, '0');
    return toHex(r) + toHex(g) + toHex(b);
  }

  const h1 = t;
  const h2 = (t + 0.33) % 1;
  const h3 = (t + 0.66) % 1;

  return {
    duration: 30000,
    layers: [
      {
        type: 'gradient',
        x: 0,
        y: 0,
        width: 32,
        height: 8,
        direction: 'diagonal',
        colors: {
          min: 0,
          max: 1,
          stops: [
            [0, hslToHex(h1)],
            [0.5, hslToHex(h2)],
            [1, hslToHex(h3)],
          ],
        },
      },
    ],
  };
};
