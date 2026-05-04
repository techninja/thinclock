exports.name = 'Ambient Gradient';
exports.enabled = true;

exports.screen = () => ({
  duration: 15000,
  layers: [
    // Base gradient
    { type: 'gradient', x: 0, y: 0, width: 32, height: 8, direction: 'horizontal',
      colors: { min: 0, max: 1, stops: [[0,'220044'],[0.5,'004466'],[1,'442200']] } },
    // Overlay gradient that fades in/out with tween, creating a shifting effect
    { type: 'gradient', x: 0, y: 0, width: 32, height: 8, direction: 'diagonal',
      colors: { min: 0, max: 1, stops: [[0,'440066'],[0.5,'006644'],[1,'664400']] },
      opacity: 0,
      tweens: [
        { prop: 'opacity', from: 0, to: 200, duration: 5000, easing: 'sine', loop: 'pingpong' },
      ] },
    // Sliding highlight
    { type: 'gradient', x: -8, y: 0, width: 8, height: 8, direction: 'vertical',
      colors: { min: 0, max: 1, stops: [[0,'FFFFFF'],[1,'000000']] },
      opacity: 60,
      tweens: [
        { prop: 'x', from: -8, to: 32, duration: 8000, easing: 'ease_in_out', loop: 'pingpong' },
      ] },
  ],
});
