/** A soft synthesized purr: filtered noise pulsing about 25 times a second */
export class Purr {
  private ctx: AudioContext | null = null;
  private gain: GainNode | null = null;

  set(on: boolean): void {
    if (on && !this.ctx) this.build();
    if (!this.ctx || !this.gain) return;
    if (on) void this.ctx.resume();
    this.gain.gain.setTargetAtTime(on ? 0.35 : 0, this.ctx.currentTime, on ? 0.15 : 0.4);
  }

  private build(): void {
    const ctx = new AudioContext();
    const noise = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate);
    const data = noise.getChannelData(0);
    let last = 0;
    for (let i = 0; i < data.length; i++) data[i] = last = (last + 0.02 * (Math.random() * 2 - 1)) / 1.02; // brown noise
    const source = ctx.createBufferSource();
    source.buffer = noise;
    source.loop = true;
    const filter = ctx.createBiquadFilter();
    filter.type = "lowpass";
    filter.frequency.value = 180;
    // amplitude pulses: the rumble of a purr
    const pulse = ctx.createGain();
    const lfo = ctx.createOscillator();
    const depth = ctx.createGain();
    lfo.frequency.value = 25;
    depth.gain.value = 0.5;
    pulse.gain.value = 0.5;
    lfo.connect(depth).connect(pulse.gain);
    this.gain = ctx.createGain();
    this.gain.gain.value = 0;
    source.connect(filter).connect(pulse).connect(this.gain).connect(ctx.destination);
    source.start();
    lfo.start();
    this.ctx = ctx;
  }
}
