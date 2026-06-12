'use strict';
// ============================================================
//  RUSTY THE RAT - a 16-bit Sega Genesis style platformer
//  320x224 internal resolution, momentum physics, parallax,
//  line-scroll water, a loop, a corkscrew, and a pseudo-3D
//  half-pipe bonus stage.
// ============================================================

const W = 320, H = 224, TILE = 16;
const cv = document.getElementById('game');
const ctx = cv.getContext('2d');
ctx.imageSmoothingEnabled = false;

// ---------------- input ----------------
const keys = {};
let anyKeyPressed = false;
const KEYMAP = {
  ArrowLeft: 'left', KeyA: 'left',
  ArrowRight: 'right', KeyD: 'right',
  ArrowUp: 'up', KeyW: 'up',
  ArrowDown: 'down', KeyS: 'down',
  Space: 'jump', KeyZ: 'jump', KeyX: 'jump',
  Enter: 'start', KeyM: 'mute',
};
let pressed = {}; // edge-triggered, cleared each frame
window.addEventListener('keydown', e => {
  const k = KEYMAP[e.code];
  if (k) {
    if (!keys[k]) pressed[k] = true;
    keys[k] = true;
    e.preventDefault();
  }
  anyKeyPressed = true;
  initAudio();
});
window.addEventListener('keyup', e => {
  const k = KEYMAP[e.code];
  if (k) keys[k] = false;
});

// ---------------- audio ----------------
let AC = null, masterGain = null, muted = false;
function initAudio() {
  if (AC) return;
  try {
    AC = new (window.AudioContext || window.webkitAudioContext)();
    masterGain = AC.createGain();
    masterGain.gain.value = 0.4;
    masterGain.connect(AC.destination);
  } catch (e) { AC = null; }
}
function tone(type, f0, f1, dur, vol, when) {
  if (!AC || muted) return;
  const t = (when !== undefined ? when : AC.currentTime);
  const o = AC.createOscillator(), g = AC.createGain();
  o.type = type;
  o.frequency.setValueAtTime(f0, t);
  if (f1 !== f0) o.frequency.exponentialRampToValueAtTime(Math.max(1, f1), t + dur);
  g.gain.setValueAtTime(vol, t);
  g.gain.exponentialRampToValueAtTime(0.001, t + dur);
  o.connect(g); g.connect(masterGain);
  o.start(t); o.stop(t + dur + 0.02);
}
let noiseBuf = null;
function noise(dur, vol, when) {
  if (!AC || muted) return;
  if (!noiseBuf) {
    noiseBuf = AC.createBuffer(1, AC.sampleRate * 0.2, AC.sampleRate);
    const d = noiseBuf.getChannelData(0);
    for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  }
  const t = (when !== undefined ? when : AC.currentTime);
  const s = AC.createBufferSource(), g = AC.createGain();
  s.buffer = noiseBuf;
  g.gain.setValueAtTime(vol, t);
  g.gain.exponentialRampToValueAtTime(0.001, t + dur);
  s.connect(g); g.connect(masterGain);
  s.start(t); s.stop(t + dur + 0.02);
}
const sfx = {
  jump:   () => tone('square', 280, 620, 0.13, 0.16),
  cheese: () => { tone('square', 988, 988, 0.06, 0.12); tone('square', 1319, 1319, 0.09, 0.12, AC && AC.currentTime + 0.05); },
  hurt:   () => { tone('sawtooth', 220, 55, 0.28, 0.2); noise(0.15, 0.12); },
  spring: () => tone('triangle', 180, 950, 0.2, 0.2),
  pop:    () => { noise(0.12, 0.18); tone('square', 180, 60, 0.15, 0.15); },
  check:  () => { tone('square', 660, 660, 0.07, 0.12); tone('square', 880, 880, 0.12, 0.12, AC && AC.currentTime + 0.07); },
  goal:   () => { [523, 659, 784, 1047].forEach((f, i) => tone('square', f, f, 0.12, 0.13, AC && AC.currentTime + i * 0.09)); },
  die:    () => tone('sawtooth', 400, 40, 0.5, 0.2),
  bonus:  () => tone('square', 1175, 1760, 0.1, 0.12),
  bomb:   () => { noise(0.3, 0.2); tone('sawtooth', 120, 30, 0.3, 0.18); },
};

// --- music sequencer: square lead + triangle bass + noise hat ---
const NOTE = n => 440 * Math.pow(2, (n - 69) / 12); // midi -> Hz
const R = null;
const MUSIC = {
  sewer: {
    bpm: 122,
    bass: [38,R,38,R, 41,R,38,R, 36,R,36,R, 43,R,41,R],
    lead: [62,R,65,67, R,69,R,67, 65,R,62,R, 60,62,R,R,
           62,R,65,67, R,72,R,69, 67,65,67,R, 62,R,R,R],
  },
  roof: {
    bpm: 140,
    bass: [40,R,40,52, 45,R,45,57, 38,R,38,50, 43,R,45,47],
    lead: [64,R,67,R, 71,R,69,67, 69,R,67,R, 64,R,62,64,
           64,R,67,R, 71,R,74,71, 69,67,69,R, 71,R,R,R],
  },
  bonus: {
    bpm: 160,
    bass: [45,R,45,R, 41,R,41,R, 43,R,43,R, 48,R,47,R],
    lead: [69,72,76,72, 69,72,76,72, 65,69,72,69, 67,71,74,71,
           69,72,76,72, 81,R,79,R, 76,R,72,R, 74,76,R,R],
  },
};
let curTrack = null, seqStep = 0, nextNoteTime = 0;
function setMusic(name) {
  curTrack = name ? MUSIC[name] : null;
  seqStep = 0;
  if (AC) nextNoteTime = AC.currentTime + 0.06;
}
setInterval(() => {
  if (!AC || !curTrack || muted) return;
  const stepDur = 60 / curTrack.bpm / 2; // 8th notes
  while (nextNoteTime < AC.currentTime + 0.14) {
    const b = curTrack.bass[seqStep % curTrack.bass.length];
    const l = curTrack.lead[seqStep % curTrack.lead.length];
    if (b !== R) tone('triangle', NOTE(b), NOTE(b), stepDur * 0.95, 0.16, nextNoteTime);
    if (l !== R) tone('square', NOTE(l), NOTE(l), stepDur * 0.85, 0.07, nextNoteTime);
    if (seqStep % 4 === 2) noise(0.03, 0.05, nextNoteTime);
    seqStep++;
    nextNoteTime += stepDur;
  }
}, 40);

// ---------------- tiny 3x5 pixel font ----------------
const FONT = {
  A:'010101111101101', B:'110101110101110', C:'011100100100011', D:'110101101101110',
  E:'111100110100111', F:'111100110100100', G:'011100101101011', H:'101101111101101',
  I:'111010010010111', J:'001001001101010', K:'101110100110101', L:'100100100100111',
  M:'101111111101101', N:'110101101101101', O:'010101101101010', P:'110101110100100',
  Q:'010101101110011', R:'110101110101101', S:'011100010001110', T:'111010010010010',
  U:'101101101101111', V:'101101101101010', W:'101101111111101', X:'101101010101101',
  Y:'101101010010010', Z:'111001010100111',
  '0':'111101101101111', '1':'010110010010111', '2':'111001111100111', '3':'111001011001111',
  '4':'101101111001001', '5':'111100111001111', '6':'111100111101111', '7':'111001001010010',
  '8':'111101111101111', '9':'111101111001111',
  ':':'000010000010000', '.':'000000000000010', '!':'010010010000010', '-':'000000111000000',
  '/':'001001010100100', "'":'010010000000000', ' ':'000000000000000',
};
function drawText(g, str, x, y, color, s) {
  s = s || 1;
  g.fillStyle = color;
  for (let i = 0; i < str.length; i++) {
    const gl = FONT[str[i]] || FONT[' '];
    for (let r = 0; r < 5; r++)
      for (let c = 0; c < 3; c++)
        if (gl[r * 3 + c] === '1') g.fillRect(x + i * 4 * s + c * s, y + r * s, s, s);
  }
}
function textW(str, s) { return str.length * 4 * (s || 1) - (s || 1); }
function drawTextC(g, str, cx, y, color, s) { drawText(g, str, Math.floor(cx - textW(str, s) / 2), y, color, s); }

// ---------------- procedural pixel art ----------------
function mkCanvas(w, h, fn) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  const g = c.getContext('2d');
  g.imageSmoothingEnabled = false;
  fn(g);
  return c;
}
function px(g, x, y, c) { g.fillStyle = c; g.fillRect(x, y, 1, 1); }
function rect(g, x, y, w, h, c) { g.fillStyle = c; g.fillRect(x, y, w, h); }
function ell(g, cx, cy, rx, ry, c) {
  g.fillStyle = c;
  for (let y = Math.floor(cy - ry); y <= cy + ry; y++)
    for (let x = Math.floor(cx - rx); x <= cx + rx; x++) {
      const dx = (x - cx) / rx, dy = (y - cy) / ry;
      if (dx * dx + dy * dy <= 1) g.fillRect(x, y, 1, 1);
    }
}

// rat palette
const RP = { body:'#9aa8bc', shade:'#5e6c84', belly:'#e8ecf4', pink:'#f08caa', dark:'#1c2030', red:'#d04848' };

// running / standing rat, 26x18, facing right
function ratFrame(legPhase, bob, blink) {
  return mkCanvas(26, 18, g => {
    const oy = bob ? 1 : 0;
    // tail
    const wave = legPhase * 1.5;
    for (let i = 0; i < 8; i++)
      px(g, 1 + i, 10 + oy + Math.round(Math.sin(i * 0.7 + wave) * 1.5), RP.pink);
    // legs
    const lo = Math.round(Math.sin(legPhase) * 2);
    rect(g, 10 + lo, 14 + oy, 2, 3, RP.shade);
    rect(g, 17 - lo, 14 + oy, 2, 3, RP.shade);
    rect(g, 12 - lo, 14 + oy, 2, 3, RP.body);
    rect(g, 19 + lo, 14 + oy, 2, 3, RP.body);
    // body
    ell(g, 14, 9 + oy, 7, 5, RP.body);
    ell(g, 14, 11 + oy, 6, 3, RP.belly);
    rect(g, 8, 6 + oy, 8, 3, RP.shade); // back shading
    ell(g, 14, 9 + oy, 7, 5, 'rgba(0,0,0,0)'); // noop keep shape
    // head
    ell(g, 20, 7 + oy, 4, 3.5, RP.body);
    // snout
    px(g, 24, 7 + oy, RP.body); px(g, 24, 8 + oy, RP.body);
    px(g, 25, 8 + oy, RP.pink); // nose
    // ear
    ell(g, 18, 3 + oy, 2, 2, RP.body);
    px(g, 18, 3 + oy, RP.pink);
    // eye
    if (!blink) px(g, 21, 6 + oy, RP.dark); else rect(g, 20, 6 + oy, 2, 1, RP.shade);
    // outline-ish bottom shade
    rect(g, 9, 13 + oy, 11, 1, RP.shade);
  });
}
// curled ball (jump/roll), 16x16 - rotated at draw time
const ratBall = mkCanvas(16, 16, g => {
  ell(g, 8, 8, 7, 7, RP.body);
  ell(g, 8, 8, 7, 7, RP.body);
  // dark crescent for spin readability
  g.fillStyle = RP.shade;
  for (let a = -0.6; a < 1.8; a += 0.08) {
    px(g, Math.round(8 + Math.cos(a) * 5.5), Math.round(8 + Math.sin(a) * 5.5), RP.shade);
    px(g, Math.round(8 + Math.cos(a) * 4.5), Math.round(8 + Math.sin(a) * 4.5), RP.shade);
  }
  // wrapped tail
  for (let a = 2.2; a < 4.4; a += 0.12)
    px(g, Math.round(8 + Math.cos(a) * 6.5), Math.round(8 + Math.sin(a) * 6.5), RP.pink);
  px(g, 11, 4, RP.pink); // ear
  px(g, 12, 6, RP.dark); // eye
});
// hurt frame, 26x18
const ratHurt = mkCanvas(26, 18, g => {
  ell(g, 13, 9, 7, 5, RP.body);
  ell(g, 13, 11, 6, 3, RP.belly);
  ell(g, 20, 6, 4, 3.5, RP.body);
  px(g, 25, 7, RP.pink);
  ell(g, 18, 2, 2, 2, RP.body); px(g, 18, 2, RP.pink);
  // X eye
  px(g, 20, 5, RP.dark); px(g, 22, 5, RP.dark); px(g, 21, 6, RP.dark);
  px(g, 20, 7, RP.dark); px(g, 22, 7, RP.dark);
  // splayed legs
  rect(g, 8, 13, 2, 3, RP.shade); rect(g, 18, 13, 2, 3, RP.shade);
  rect(g, 5, 11, 2, 2, RP.body); rect(g, 21, 12, 2, 2, RP.body);
  for (let i = 0; i < 7; i++) px(g, 1 + i, 7 + ((i % 2) ? 1 : 0), RP.pink);
});
const ratRun = [ratFrame(0, false, false), ratFrame(Math.PI / 2, true, false),
                ratFrame(Math.PI, false, false), ratFrame(3 * Math.PI / 2, true, false)];
const ratIdle = ratFrame(0, false, false);
const ratIdleBlink = ratFrame(0, false, true);

// enemy: "MAUSER" the patrol robo-cat, 18x14, 2 frames
function catbotFrame(f) {
  return mkCanvas(18, 14, g => {
    rect(g, 2, 4, 14, 7, '#8878a8');        // body
    rect(g, 2, 4, 14, 2, '#a89cc4');        // top highlight
    rect(g, 12, 2, 4, 4, '#8878a8');        // head
    px(g, 11, 0, '#8878a8'); px(g, 15, 0, '#8878a8'); // ears
    px(g, 12, 1, '#6a5c8a'); px(g, 15, 1, '#6a5c8a');
    rect(g, 13, 3, 3, 2, '#301828');        // visor
    px(g, 13 + f, 4, '#ff4040');            // scanning eye
    // wheels
    const wo = f ? 1 : 0;
    ell(g, 5, 11, 2.5, 2.5, '#383848');
    ell(g, 13, 11, 2.5, 2.5, '#383848');
    px(g, 5 - 1 + wo, 11, '#707088'); px(g, 13 - 1 + wo, 11, '#707088');
    // tail antenna
    rect(g, 1, 2, 1, 4, '#6a5c8a'); px(g, 1, 1, '#ff4040');
  });
}
const catbot = [catbotFrame(0), catbotFrame(1)];

// cheese wedge 12x10
const cheeseSpr = mkCanvas(12, 10, g => {
  g.fillStyle = '#f8d838';
  g.beginPath(); g.moveTo(0, 9); g.lineTo(11, 9); g.lineTo(11, 0); g.closePath(); g.fill();
  g.fillStyle = '#c8a020';
  g.fillRect(0, 9, 12, 1);
  px(g, 8, 4, '#c8a020'); px(g, 6, 7, '#c8a020'); px(g, 9, 7, '#c8a020');
  px(g, 10, 2, '#fff0a0');
});
// spikes tile
const spikeSpr = mkCanvas(16, 16, g => {
  g.fillStyle = '#b8c0d0';
  for (let i = 0; i < 2; i++) {
    g.beginPath();
    g.moveTo(i * 8, 16); g.lineTo(i * 8 + 4, 4); g.lineTo(i * 8 + 8, 16);
    g.closePath(); g.fill();
  }
  g.fillStyle = '#707a90';
  for (let i = 0; i < 2; i++) { rect(g, i * 8 + 4, 6, 1, 10, '#707a90'); }
});
// spring (2 frames: compressed, extended)
const springSpr = [
  mkCanvas(16, 16, g => {
    rect(g, 2, 10, 12, 3, '#e0e040');
    rect(g, 4, 13, 8, 3, '#a83030');
    rect(g, 4, 13, 8, 1, '#d05050');
  }),
  mkCanvas(16, 16, g => {
    rect(g, 2, 2, 12, 3, '#e0e040');
    rect(g, 6, 5, 4, 1, '#888'); rect(g, 5, 7, 6, 1, '#888'); rect(g, 6, 9, 4, 1, '#888');
    rect(g, 4, 13, 8, 3, '#a83030');
    rect(g, 4, 13, 8, 1, '#d05050');
  }),
];
// goal sign faces 20x20: cat face -> rat face
const signCat = mkCanvas(20, 20, g => {
  rect(g, 0, 0, 20, 20, '#283048');
  rect(g, 1, 1, 18, 18, '#e8e8f0');
  ell(g, 10, 11, 6, 5, '#8878a8');
  px(g, 5, 5, '#8878a8'); px(g, 4, 4, '#8878a8'); px(g, 15, 5, '#8878a8'); px(g, 16, 4, '#8878a8');
  px(g, 8, 10, '#ff4040'); px(g, 12, 10, '#ff4040');
  rect(g, 9, 13, 3, 1, '#301828');
});
const signRat = mkCanvas(20, 20, g => {
  rect(g, 0, 0, 20, 20, '#283048');
  rect(g, 1, 1, 18, 18, '#e8e8f0');
  ell(g, 10, 12, 6, 5, RP.body);
  ell(g, 6, 5, 2, 2, RP.body); px(g, 6, 5, RP.pink);
  ell(g, 14, 5, 2, 2, RP.body); px(g, 14, 5, RP.pink);
  px(g, 8, 11, RP.dark); px(g, 12, 11, RP.dark);
  px(g, 10, 14, RP.pink);
});
// checkpoint lamp 10x28
function lampSpr(on) {
  return mkCanvas(10, 28, g => {
    rect(g, 4, 8, 2, 20, '#506070');
    rect(g, 2, 26, 6, 2, '#384858');
    ell(g, 5, 5, 4, 4, on ? '#f8d838' : '#3858c8');
    ell(g, 4, 4, 1.5, 1.5, on ? '#fff0a0' : '#80a0f0');
  });
}
const lampOff = lampSpr(false), lampOn = lampSpr(true);

// bomb (bonus stage) 14x14
const bombSpr = mkCanvas(14, 14, g => {
  ell(g, 7, 7, 5, 5, '#404050');
  ell(g, 5, 5, 1.5, 1.5, '#707088');
  for (let a = 0; a < 6.28; a += 0.785)
    px(g, Math.round(7 + Math.cos(a) * 6), Math.round(7 + Math.sin(a) * 6), '#888898');
});

// ---------------- tiles per theme ----------------
function sewerBrick() {
  return mkCanvas(16, 16, g => {
    rect(g, 0, 0, 16, 16, '#3d5a66');
    g.fillStyle = '#27424d';
    rect(g, 0, 3, 16, 1, '#27424d'); rect(g, 0, 7, 16, 1, '#27424d');
    rect(g, 0, 11, 16, 1, '#27424d'); rect(g, 0, 15, 16, 1, '#27424d');
    rect(g, 4, 0, 1, 3, '#27424d'); rect(g, 12, 4, 1, 3, '#27424d');
    rect(g, 8, 8, 1, 3, '#27424d'); rect(g, 2, 12, 1, 3, '#27424d');
    px(g, 6, 1, '#557a87'); px(g, 13, 9, '#557a87'); px(g, 3, 5, '#557a87');
    px(g, 10, 13, '#5d8a55'); px(g, 14, 5, '#5d8a55');
  });
}
function roofBrick() {
  return mkCanvas(16, 16, g => {
    rect(g, 0, 0, 16, 16, '#5a5a74');
    rect(g, 0, 0, 16, 2, '#8484a4');
    rect(g, 0, 2, 16, 1, '#44445c');
    g.fillStyle = '#44445c';
    rect(g, 0, 8, 16, 1, '#44445c'); rect(g, 5, 3, 1, 5, '#44445c');
    rect(g, 11, 9, 1, 7, '#44445c'); rect(g, 0, 15, 16, 1, '#3a3a50');
    px(g, 8, 5, '#6e6e8c'); px(g, 3, 12, '#6e6e8c');
  });
}
function metalTile() {
  return mkCanvas(16, 16, g => {
    rect(g, 0, 0, 16, 16, '#4a7858');
    rect(g, 0, 0, 16, 2, '#6c9c78');
    rect(g, 0, 14, 16, 2, '#2e5440');
    px(g, 2, 4, '#2e5440'); px(g, 13, 4, '#2e5440');
    px(g, 2, 11, '#2e5440'); px(g, 13, 11, '#2e5440');
    rect(g, 7, 2, 2, 12, '#3a6448');
  });
}
function ledgeTile(c1, c2) {
  return mkCanvas(16, 16, g => {
    rect(g, 0, 0, 16, 5, c1);
    rect(g, 0, 0, 16, 1, '#d8dce8');
    rect(g, 0, 4, 16, 1, c2);
    px(g, 4, 2, c2); px(g, 11, 2, c2);
  });
}
function pipeTile() {
  return mkCanvas(16, 16, g => {
    rect(g, 4, 0, 8, 16, '#355248');
    rect(g, 5, 0, 2, 16, '#4d7264');
    rect(g, 10, 0, 1, 16, '#24382e');
  });
}
const TILES = {
  sewer: { '#': sewerBrick(), 'B': metalTile(), '-': ledgeTile('#557a87', '#3d5a66'), '|': pipeTile() },
  roof:  { '#': roofBrick(),  'B': metalTile(), '-': ledgeTile('#8484a4', '#5a5a74'), '|': pipeTile() },
};

// ---------------- parallax backgrounds ----------------
// each theme renders procedural layers onto wide offscreen strips
function makeSewerBG() {
  const far = mkCanvas(256, 224, g => {
    // dark wall with arches
    rect(g, 0, 0, 256, 224, '#16242c');
    for (let i = 0; i < 4; i++) {
      const x = i * 64 + 8;
      g.fillStyle = '#1e3038';
      g.fillRect(x, 60, 48, 120);
      g.beginPath(); g.arc(x + 24, 60, 24, Math.PI, 0); g.fill();
      g.fillStyle = '#101c22';
      g.fillRect(x + 6, 72, 36, 108);
      g.beginPath(); g.arc(x + 24, 72, 18, Math.PI, 0); g.fill();
    }
    // glow drips
    for (let i = 0; i < 14; i++) px(g, (i * 37) % 256, 30 + (i * 53) % 140, '#3d7a55');
  });
  const mid = mkCanvas(256, 224, g => {
    // horizontal pipes
    for (const y of [40, 110, 150]) {
      rect(g, 0, y, 256, 10, '#2a4438');
      rect(g, 0, y + 1, 256, 2, '#3f6450');
      rect(g, 0, y + 8, 256, 2, '#1c3026');
      for (let x = 12; x < 256; x += 48) { rect(g, x, y - 2, 6, 14, '#22382c'); }
    }
  });
  return { far, mid, sky: '#0c161c', waterY: 168 };
}
function makeRoofBG() {
  const far = mkCanvas(256, 224, g => {
    // dusk sky bands (genesis dither vibes)
    const bands = ['#283068', '#383878', '#584888', '#806098', '#b87898', '#e89878'];
    bands.forEach((c, i) => rect(g, 0, i * 22, 256, 22, c));
    rect(g, 0, 132, 256, 92, '#e8a878');
    // sun
    ell(g, 190, 118, 18, 18, '#f8e0a0');
    ell(g, 190, 118, 12, 12, '#fff8d8');
    // far skyline
    g.fillStyle = '#403060';
    for (let i = 0; i < 16; i++) {
      const x = i * 17, h2 = 30 + ((i * 73) % 50);
      g.fillRect(x, 224 - 70 - h2, 15, h2 + 70);
    }
    // lit windows
    for (let i = 0; i < 40; i++)
      px(g, (i * 29) % 256, 130 + (i * 41) % 80, '#f8d838');
  });
  const mid = mkCanvas(256, 224, g => {
    g.fillStyle = '#2a2048';
    for (let i = 0; i < 8; i++) {
      const x = i * 33, h2 = 60 + ((i * 97) % 70);
      g.fillRect(x, 224 - h2, 28, h2);
      for (let wy = 224 - h2 + 6; wy < 218; wy += 10)
        for (let wx = x + 4; wx < x + 24; wx += 8)
          rect(g, wx, wy, 3, 4, ((wx * wy) % 7 < 3) ? '#f8d838' : '#181030');
    }
  });
  const clouds = mkCanvas(256, 60, g => {
    for (let i = 0; i < 7; i++) {
      const x = (i * 41) % 256, y = 8 + (i * 23) % 40;
      ell(g, x, y, 16, 4, 'rgba(248,232,216,0.85)');
      ell(g, x + 10, y + 2, 10, 3, 'rgba(248,232,216,0.85)');
    }
  });
  return { far, mid, clouds, sky: '#283068' };
}
const BGS = { sewer: makeSewerBG(), roof: makeRoofBG() };

// ---------------- level construction ----------------
function makeMap(w, h) { return Array.from({ length: h }, () => Array(w).fill(' ')); }
function fillR(m, c0, r0, c1, r1, ch) {
  for (let r = r0; r <= r1; r++) for (let c = c0; c <= c1; c++) m[r][c] = ch;
}
function put(m, c, r, ch) { m[r][c] = ch; }
function ground(m, c0, c1, top) { fillR(m, c0, top, c1, m.length - 1, '#'); }

function buildLevel1() {
  const w = 180, h = 18, m = makeMap(w, h);
  ground(m, 0, 30, 15);
  put(m, 3, 13, 'P');
  for (let c = 8; c <= 11; c++) put(m, c, 13, 'C');
  put(m, 5, 10, '|'); put(m, 5, 11, '|'); put(m, 5, 12, '|'); put(m, 5, 13, '|'); put(m, 5, 14, '|');
  put(m, 22, 14, 'E');
  fillR(m, 26, 13, 30, 17, '#');
  for (let c = 27; c <= 29; c++) put(m, c, 11, 'C');
  // pit with floating ledges
  for (let c = 31; c <= 33; c++) put(m, c, 12, '-');
  ground(m, 36, 69, 15);
  put(m, 38, 14, 'S');
  for (let c = 42; c <= 47; c++) put(m, c, 9, '-');
  for (let c = 43; c <= 46; c++) put(m, c, 8, 'C');
  // THE LOOP - flat runway, center col 58
  for (let c = 51; c <= 55; c++) put(m, c, 13, 'C');
  put(m, 58, 12, 'O');
  for (let c = 61; c <= 65; c++) put(m, c, 13, 'C');
  // pit
  for (let c = 70; c <= 72; c++) put(m, c, 12, '-');
  ground(m, 74, 109, 15);
  put(m, 78, 14, '|'); put(m, 78, 13, '|');
  put(m, 82, 14, 'E');
  put(m, 86, 14, '^'); put(m, 87, 14, '^');
  put(m, 90, 14, 'E');
  fillR(m, 94, 13, 99, 17, '#');
  for (let c = 95; c <= 98; c++) put(m, c, 11, 'C');
  put(m, 104, 14, '*');
  // climb over pit
  for (let c = 111; c <= 113; c++) put(m, c, 12, '-');
  for (let c = 113; c <= 114; c++) put(m, c, 9, '-');
  ground(m, 116, 150, 15);
  // metal upper deck
  fillR(m, 118, 9, 132, 9, 'B');
  for (let c = 120; c <= 130; c += 2) put(m, c, 8, 'C');
  put(m, 126, 8, 'E');
  put(m, 135, 14, 'E');
  put(m, 140, 14, 'S');
  put(m, 142, 9, 'C'); put(m, 143, 8, 'C'); put(m, 144, 7, 'C'); put(m, 145, 8, 'C'); put(m, 146, 9, 'C');
  // last pit
  for (let c = 151; c <= 153; c++) put(m, c, 12, '-');
  ground(m, 156, 179, 15);
  put(m, 160, 14, '^'); put(m, 161, 14, '^');
  put(m, 165, 14, 'E');
  put(m, 172, 14, 'G');
  fillR(m, 178, 8, 179, 17, '#');
  return { map: m, name: 'SEWER ZONE', theme: 'sewer', music: 'sewer' };
}

function buildLevel2() {
  const w = 200, h = 18, m = makeMap(w, h);
  ground(m, 0, 17, 14);
  put(m, 3, 12, 'P');
  for (let c = 8; c <= 12; c++) put(m, c, 12, 'C');
  ground(m, 20, 43, 14);
  for (let c = 26; c <= 29; c++) put(m, c, 11, '-');
  for (let c = 26; c <= 29; c++) put(m, c, 10, 'C');
  put(m, 34, 13, 'E');
  put(m, 40, 13, '^');
  // long roof with CORKSCREW (rail starts at ground level, col 50)
  ground(m, 47, 79, 15);
  put(m, 50, 14, 'Q');
  for (let c = 64; c <= 70; c++) put(m, c, 13, 'C');
  put(m, 73, 14, 'E');
  put(m, 77, 14, 'S');
  // high building - reach via spring
  ground(m, 83, 105, 11);
  for (let c = 86; c <= 89; c++) put(m, c, 9, 'C');
  put(m, 90, 10, 'E');
  put(m, 94, 10, '^'); put(m, 95, 10, '^');
  put(m, 98, 10, 'E');
  for (let c = 100; c <= 103; c++) put(m, c, 9, 'C');
  // mid roof with LOOP
  ground(m, 109, 133, 13);
  for (let c = 112; c <= 115; c++) put(m, c, 11, 'C');
  put(m, 120, 10, 'O');
  for (let c = 124; c <= 128; c++) put(m, c, 11, 'C');
  // lower roof, one-way tower, checkpoint
  ground(m, 137, 165, 15);
  for (let c = 140; c <= 142; c++) put(m, c, 12, '-');
  for (let c = 144; c <= 146; c++) put(m, c, 9, '-');
  put(m, 145, 7, 'C'); put(m, 144, 7, 'C'); put(m, 146, 7, 'C');
  put(m, 150, 14, '*');
  put(m, 155, 14, 'E');
  put(m, 160, 14, 'S');
  // final roof
  ground(m, 169, 199, 14);
  put(m, 175, 13, '^'); put(m, 176, 13, '^');
  for (let c = 180; c <= 185; c++) put(m, c, 12, 'C');
  put(m, 186, 13, 'E');
  put(m, 192, 13, 'G');
  fillR(m, 198, 7, 199, 17, '#');
  return { map: m, name: 'ROOFTOP RUN', theme: 'roof', music: 'roof' };
}
const LEVEL_BUILDERS = [buildLevel1, buildLevel2];

// ---------------- game state ----------------
const SOLID = { '#': 1, 'B': 1 };
let state = 'title', stateT = 0;
let levelIdx = 0;
let lvl = null;          // {map, w, h, name, theme, music}
let objects = [];        // cheese, enemies, springs, lamps, goal
let loops = [];          // {cx, cy, r}
let corks = [];          // {x0, x1, cy, amp}
let particles = [];
let scattered = [];      // bouncing cheese after a hit
let cam = { x: 0, y: 0 };
let p = null;            // player
let score = 0, cheese = 0, time = 0;
let checkpoint = null;
let shake = 0, flash = 0;
let paused = false;
let totalCheese = 0;
let bonusResult = 0;

function newPlayer(x, y) {
  return {
    x, y, vx: 0, vy: 0, w: 12, h: 16,
    onGround: false, facing: 1, ball: false,
    mode: 'normal', // normal | loop | cork | hurtfly | dead | goal
    loopA: 0, loopRef: null, corkRef: null,
    rollA: 0, anim: 0, invuln: 0, deadT: 0, blink: 0,
  };
}

function loadLevel(idx, fromCheckpoint) {
  lvl = LEVEL_BUILDERS[idx]();
  lvl.w = lvl.map[0].length; lvl.h = lvl.map.length;
  objects = []; loops = []; corks = []; particles = []; scattered = [];
  let start = { x: 48, y: 100 };
  for (let r = 0; r < lvl.h; r++) for (let c = 0; c < lvl.w; c++) {
    const ch = lvl.map[r][c], x = c * TILE, y = r * TILE;
    switch (ch) {
      case 'P': start = { x: x + 2, y: y }; lvl.map[r][c] = ' '; break;
      case 'C': objects.push({ t: 'cheese', x: x + 2, y: y + 3, w: 12, h: 10, live: true }); lvl.map[r][c] = ' '; break;
      case 'E': objects.push({ t: 'enemy', x: x - 1, y: y + 2, w: 18, h: 14, vx: -0.45, live: true, f: 0 }); lvl.map[r][c] = ' '; break;
      case 'S': objects.push({ t: 'spring', x: x, y: y, w: 16, h: 16, anim: 0, live: true }); lvl.map[r][c] = ' '; break;
      case '*': objects.push({ t: 'lamp', x: x + 3, y: y + 4 - TILE, w: 10, h: 28, on: false, live: true }); lvl.map[r][c] = ' '; break;
      case 'G': objects.push({ t: 'goal', x: x - 2, y: y - 14, w: 20, h: 30, spin: 0, hit: false, live: true }); lvl.map[r][c] = ' '; break;
      case 'O': loops.push({ cx: x + 8, cy: y + 8, r: 40, cool: 0 }); lvl.map[r][c] = ' '; break;
      case 'Q': corks.push({ x0: x, x1: x + 176, cy: (r + 1) * TILE, amp: 26 }); lvl.map[r][c] = ' '; break;
    }
  }
  if (fromCheckpoint && checkpoint) {
    start = { x: checkpoint.x, y: checkpoint.y };
    // re-light the lamp we respawned at
    for (const o of objects)
      if (o.t === 'lamp' && Math.abs(o.x - checkpoint.lx) < 20) o.on = true;
  } else {
    checkpoint = null;
  }
  p = newPlayer(start.x, start.y);
  if (!fromCheckpoint) time = 0;
  cam.x = Math.max(0, p.x - 120); cam.y = 0;
}

// ---------------- tile collision ----------------
function tileAt(px_, py_) {
  const c = Math.floor(px_ / TILE), r = Math.floor(py_ / TILE);
  if (c < 0 || c >= lvl.w) return '#';
  if (r < 0 || r >= lvl.h) return ' ';
  return lvl.map[r][c];
}
function solidAt(px_, py_) { return SOLID[tileAt(px_, py_)] === 1; }

function moveAndCollide(e) {
  // horizontal
  e.x += e.vx;
  if (e.vx > 0) {
    if (solidAt(e.x + e.w, e.y + 2) || solidAt(e.x + e.w, e.y + e.h - 2)) {
      e.x = Math.floor((e.x + e.w) / TILE) * TILE - e.w - 0.01;
      e.vx = 0; e.hitWall = true;
    }
  } else if (e.vx < 0) {
    if (solidAt(e.x, e.y + 2) || solidAt(e.x, e.y + e.h - 2)) {
      e.x = (Math.floor(e.x / TILE) + 1) * TILE + 0.01;
      e.vx = 0; e.hitWall = true;
    }
  }
  // vertical
  const wasAbove = e.y + e.h;
  e.y += e.vy;
  e.onGround = false;
  if (e.vy >= 0) {
    const bl = tileAt(e.x + 1, e.y + e.h), br = tileAt(e.x + e.w - 1, e.y + e.h);
    const onSolid = SOLID[bl] || SOLID[br];
    const rowTop = Math.floor((e.y + e.h) / TILE) * TILE;
    const onLedge = (bl === '-' || br === '-') && wasAbove <= rowTop + Math.max(4, e.vy + 1);
    if (onSolid || onLedge) {
      e.y = rowTop - e.h;
      e.vy = 0; e.onGround = true;
    }
  } else {
    if (solidAt(e.x + 1, e.y) || solidAt(e.x + e.w - 1, e.y)) {
      e.y = (Math.floor(e.y / TILE) + 1) * TILE + 0.01;
      e.vy = 0;
    }
  }
}

// ---------------- player ----------------
const ACC = 0.05, DEC = 0.4, FRC = 0.047, TOP = 4.0, AIR = 0.09;
const JUMP = 6.6, GRAV = 0.22, MAXFALL = 8;

function hurtPlayer(fromX) {
  if (p.invuln > 0 || p.mode === 'dead' || p.mode === 'goal') return;
  if (cheese > 0) {
    sfx.hurt();
    const n = Math.min(cheese, 10);
    for (let i = 0; i < n; i++) {
      const a = -Math.PI / 2 + (i - n / 2) * 0.35;
      scattered.push({
        x: p.x + p.w / 2, y: p.y + 4,
        vx: Math.cos(a) * (1.5 + (i % 3)), vy: Math.sin(a) * 3 - 1.5,
        t: 0, live: true,
      });
    }
    cheese = 0;
    p.invuln = 120;
    p.mode = 'hurtfly';
    p.vy = -4; p.vx = (p.x + p.w / 2 < fromX ? -2 : 2);
    shake = 6;
  } else {
    killPlayer();
  }
}
function killPlayer() {
  if (p.mode === 'dead') return;
  sfx.die();
  p.mode = 'dead'; p.deadT = 0; p.vy = -6; p.vx = 0;
  setMusic(null);
}

function enterLoop(L) {
  p.mode = 'loop';
  p.loopRef = L;
  p.loopA = -Math.PI / 2;
  p.loopSpd = Math.max(Math.abs(p.vx), 3.2) * (p.vx >= 0 ? 1 : 1);
  p.ball = false;
}

function updatePlayer() {
  if (p.invuln > 0) p.invuln--;
  p.blink = (p.blink + 1) % 180;

  if (p.mode === 'dead') {
    p.deadT++;
    p.vy = Math.min(p.vy + GRAV, MAXFALL);
    p.y += p.vy;
    if (p.deadT > 120) {
      loadLevel(levelIdx, !!checkpoint);
      setMusic(lvl.music);
    }
    return;
  }

  if (p.mode === 'goal') {
    // auto-run off right side
    p.vx = Math.min(p.vx + ACC, 2.2);
    p.vy = Math.min(p.vy + GRAV, MAXFALL);
    moveAndCollide(p);
    p.anim += Math.abs(p.vx) * 0.12;
    return;
  }

  if (p.mode === 'loop') {
    const L = p.loopRef;
    p.loopA += p.loopSpd / L.r;
    if (p.loopA >= Math.PI * 1.5) {
      p.mode = 'normal';
      p.vx = p.loopSpd; p.vy = 0;
      p.x = L.cx + L.r * 0.4; p.y = L.cy + L.r - p.h;
      L.cool = 30;
    } else {
      p.x = L.cx + Math.cos(p.loopA) * (L.r - 8) - p.w / 2;
      p.y = L.cy + Math.sin(p.loopA) * (L.r - 8) - p.h / 2;
    }
    p.anim += 0.4;
    return;
  }

  if (p.mode === 'cork') {
    const ck = p.corkRef;
    p.x += p.vx;
    if (keys.right) p.vx = Math.min(p.vx + ACC, TOP + 1);
    else if (keys.left) p.vx -= DEC * 0.5;
    const prog = (p.x + p.w / 2 - ck.x0) / (ck.x1 - ck.x0);
    if (prog < 0 || prog > 1) {
      p.mode = 'normal';
      p.corkAngle = 0;
    } else if (Math.abs(p.vx) < 1.6) {
      p.mode = 'normal'; p.corkAngle = 0; // too slow: fall off
    } else {
      p.corkAngle = prog * Math.PI * 2;
      p.y = ck.cy - (1 - Math.cos(prog * Math.PI * 2)) * ck.amp - p.h;
      p.vy = 0;
    }
    p.anim += Math.abs(p.vx) * 0.15;
    return;
  }

  // ---- normal & hurtfly ----
  const control = p.mode !== 'hurtfly';
  if (control) {
    if (keys.left) {
      p.facing = -1;
      if (p.vx > 0) p.vx -= DEC; else p.vx = Math.max(p.vx - (p.onGround ? ACC : AIR), -TOP);
    } else if (keys.right) {
      p.facing = 1;
      if (p.vx < 0) p.vx += DEC; else p.vx = Math.min(p.vx + (p.onGround ? ACC : AIR), TOP);
    } else if (p.onGround) {
      if (Math.abs(p.vx) < FRC) p.vx = 0; else p.vx -= Math.sign(p.vx) * FRC;
    }
    if (pressed.jump && p.onGround) {
      p.vy = -JUMP; p.ball = true; p.onGround = false;
      sfx.jump();
    }
    // variable jump height
    if (!keys.jump && p.vy < -3 && p.ball) p.vy = -3;
  }
  p.vy = Math.min(p.vy + GRAV, MAXFALL);
  moveAndCollide(p);
  if (p.onGround) {
    p.ball = false;
    if (p.mode === 'hurtfly') p.mode = 'normal';
  }
  if (p.ball) p.rollA += p.vx * 0.15 + 0.25 * Math.sign(p.vx || 1);
  p.anim += Math.abs(p.vx) * 0.14;

  // loop entry: grounded, fast, near loop base
  for (const L of loops) {
    if (L.cool > 0) { L.cool--; continue; }
    const baseY = L.cy + L.r;
    if (p.onGround && p.vx >= 3 &&
        Math.abs((p.y + p.h) - baseY) < 6 &&
        p.x + p.w / 2 > L.cx - L.r - 8 && p.x + p.w / 2 < L.cx - L.r + 26) {
      enterLoop(L);
      return;
    }
  }
  // corkscrew entry
  for (const ck of corks) {
    if (p.onGround && p.vx >= 2.2 &&
        p.x + p.w / 2 >= ck.x0 - 4 && p.x + p.w / 2 <= ck.x0 + 16 &&
        Math.abs((p.y + p.h) - ck.cy) < 8) {
      p.mode = 'cork'; p.corkRef = ck; p.ball = false; p.corkAngle = 0;
      return;
    }
  }

  // spike tiles
  const feet = tileAt(p.x + p.w / 2, p.y + p.h + 1);
  if (feet === '^' && p.vy >= 0) hurtPlayer(p.x);
  // fell off the world
  if (p.y > lvl.h * TILE + 48) killPlayer();
}

// ---------------- objects ----------------
function overlap(a, b) {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}
function sparkle(x, y, color, n) {
  for (let i = 0; i < (n || 4); i++)
    particles.push({ x, y, vx: (Math.random() - 0.5) * 2, vy: (Math.random() - 0.5) * 2 - 0.5, life: 20 + Math.random() * 10, color });
}

function updateObjects() {
  for (const o of objects) {
    if (!o.live) continue;
    if (o.t === 'enemy') {
      o.f = (o.f + 1) % 20;
      o.x += o.vx;
      // turn at walls / ledge edges
      const ahead = o.vx < 0 ? o.x - 1 : o.x + o.w + 1;
      if (solidAt(ahead, o.y + o.h / 2) || !solidAt(ahead, o.y + o.h + 2)) o.vx = -o.vx;
      if (p.mode !== 'dead' && p.mode !== 'goal' && overlap(p, o)) {
        if (p.ball || (p.vy > 0 && p.y + p.h < o.y + o.h * 0.7)) {
          o.live = false;
          score += 100;
          sfx.pop();
          sparkle(o.x + o.w / 2, o.y + o.h / 2, '#ff8040', 8);
          sparkle(o.x + o.w / 2, o.y + o.h / 2, '#fff0a0', 6);
          if (p.vy > 0) p.vy = -5;
        } else {
          hurtPlayer(o.x + o.w / 2);
        }
      }
    } else if (o.t === 'cheese') {
      if (p.mode !== 'dead' && overlap(p, o)) {
        o.live = false; cheese++; totalCheese++; score += 10;
        sfx.cheese();
        sparkle(o.x + 6, o.y + 5, '#f8d838');
      }
    } else if (o.t === 'spring') {
      if (o.anim > 0) o.anim--;
      if (p.vy >= 0 && p.mode !== 'dead' &&
          p.x + p.w > o.x + 2 && p.x < o.x + 14 &&
          p.y + p.h > o.y + 8 && p.y + p.h < o.y + 16) {
        p.vy = -8.6; p.ball = true; p.y = o.y + 8 - p.h;
        o.anim = 12;
        sfx.spring();
      }
    } else if (o.t === 'lamp') {
      if (!o.on && p.mode !== 'dead' && overlap(p, o)) {
        o.on = true;
        checkpoint = { x: o.x, y: o.y + 8, lx: o.x };
        sfx.check();
        sparkle(o.x + 5, o.y + 5, '#f8d838', 8);
      }
    } else if (o.t === 'goal') {
      if (o.hit) {
        o.spin++;
        if (o.spin === 160) {
          score += 1000 + cheese * 100;
          state = 'results'; stateT = 0;
        }
      } else if (p.mode !== 'dead' && p.x + p.w / 2 > o.x + o.w / 2) {
        o.hit = true;
        p.mode = 'goal';
        sfx.goal();
        setMusic(null);
      }
    }
  }
  // scattered cheese physics
  for (const s of scattered) {
    if (!s.live) continue;
    s.t++;
    s.vy = Math.min(s.vy + 0.18, 6);
    s.x += s.vx; s.y += s.vy;
    if (s.vy > 0 && solidAt(s.x, s.y + 8)) { s.y = Math.floor((s.y + 8) / TILE) * TILE - 8; s.vy *= -0.6; }
    if (s.t > 300) s.live = false;
    if (s.t > 15 && s.live && p.mode !== 'dead' &&
        overlap(p, { x: s.x - 6, y: s.y - 5, w: 12, h: 10 })) {
      s.live = false; cheese++; totalCheese++; score += 10; sfx.cheese();
    }
  }
  scattered = scattered.filter(s => s.live);
  for (const pt of particles) {
    pt.x += pt.vx; pt.y += pt.vy; pt.vy += 0.04; pt.life--;
  }
  particles = particles.filter(pt => pt.life > 0);
}

// ---------------- bonus stage (pseudo-3D half-pipe) ----------------
const bonus = {
  t: 0, ang: 0, angV: 0, items: [], got: 0, over: false, overT: 0, hurtT: 0,
};
function initBonus() {
  bonus.t = 0; bonus.ang = 0; bonus.angV = 0; bonus.got = 0;
  bonus.over = false; bonus.overT = 0; bonus.hurtT = 0;
  bonus.items = [];
  // generate a course: waves of cheese, occasional bombs
  let z = 260;
  for (let i = 0; i < 64; i++) {
    const patt = i % 8;
    let a;
    if (patt < 3) a = Math.sin(i * 0.9) * 0.9;            // weaving line
    else if (patt < 5) a = (patt === 3 ? -0.7 : 0.7);     // split pair
    else a = ((i * 37) % 17 / 17 - 0.5) * 1.9;            // scattered
    const isBomb = (i % 9 === 5);
    bonus.items.push({ z, a, bomb: isBomb, live: true });
    if (patt === 4) bonus.items.push({ z, a: -a, bomb: false, live: true });
    z += 34;
  }
}
function bonusProj(z, a, tt) {
  const s = 40 / z;
  const cy = 74 + (1 - s) * 14 + Math.sin(tt * 0.013 + z * 0.006) * 16 * (1 - s);
  const cx = 160 + Math.sin(tt * 0.008 + z * 0.004) * 22 * (1 - s);
  const Rr = Math.min(96, 96 * s * 2.4);
  return { s, x: cx + Math.sin(a) * Rr, y: cy + Math.cos(a) * Rr * 0.85, cx, cy, Rr };
}
function updateBonus() {
  bonus.t++;
  if (bonus.hurtT > 0) bonus.hurtT--;
  if (bonus.over) {
    bonus.overT++;
    if (bonus.overT > 200 || (bonus.overT > 60 && pressed.start)) {
      bonusResult = bonus.got;
      score += bonus.got * 50;
      levelIdx = 1;
      state = 'card'; stateT = 0;
      loadLevel(levelIdx, false);
      cheese = 0;
      setMusic(null);
    }
    return;
  }
  // half-pipe physics: input pushes, gravity pulls to bottom
  if (keys.left) bonus.angV -= 0.0045;
  if (keys.right) bonus.angV += 0.0045;
  bonus.angV -= Math.sin(bonus.ang) * 0.0035;
  bonus.angV *= 0.97;
  bonus.ang = Math.max(-1.15, Math.min(1.15, bonus.ang + bonus.angV * 8));

  const dz = 1.9;
  let remaining = 0;
  for (const it of bonus.items) {
    if (!it.live) continue;
    it.z -= dz;
    if (it.z < 34) { it.live = false; continue; }
    remaining++;
    if (it.z <= 46 && it.z > 38) {
      let d = Math.abs(it.a - bonus.ang);
      if (d < 0.3) {
        it.live = false;
        if (it.bomb) {
          sfx.bomb();
          bonus.hurtT = 40; shake = 8;
          bonus.got = Math.max(0, bonus.got - 5);
        } else {
          bonus.got++;
          sfx.bonus();
          const pr = bonusProj(42, it.a, bonus.t);
          sparkle(pr.x, pr.y, '#f8d838', 5);
        }
      }
    }
  }
  for (const pt of particles) { pt.x += pt.vx; pt.y += pt.vy; pt.life--; }
  particles = particles.filter(pt => pt.life > 0);
  if (remaining === 0 && !bonus.over) { bonus.over = true; bonus.overT = 0; setMusic(null); sfx.goal(); }
}
function drawBonus() {
  // deep tunnel background
  ctx.fillStyle = '#100828';
  ctx.fillRect(0, 0, W, H);
  // tunnel rings, far to near
  const RINGS = 11, span = 300;
  const step = span / RINGS;
  const off = (bonus.t * 1.9) % step;
  for (let i = RINGS; i >= 1; i--) {
    const z = 40 + i * step - off;
    const pr = bonusProj(z, 0, bonus.t);
    const Rr = pr.Rr;
    ctx.strokeStyle = (Math.floor((z + bonus.t * 1.9) / step) % 2 === 0) ? '#4838a0' : '#282060';
    ctx.lineWidth = Math.max(1, pr.s * 14);
    ctx.setLineDash([Math.max(2, Rr * 0.18), Math.max(2, Rr * 0.12)]);
    ctx.lineDashOffset = bonus.t * 0.6 * pr.s + i * 3;
    ctx.beginPath();
    ctx.ellipse(pr.cx, pr.cy, Rr, Rr * 0.85, 0, 0, Math.PI * 2);
    ctx.stroke();
  }
  ctx.setLineDash([]);
  // center glow
  const far = bonusProj(340, 0, bonus.t);
  ctx.fillStyle = '#181048';
  ctx.beginPath(); ctx.ellipse(far.cx, far.cy, 14, 12, 0, 0, Math.PI * 2); ctx.fill();

  // items far-to-near
  const sorted = bonus.items.filter(it => it.live).sort((a, b) => b.z - a.z);
  for (const it of sorted) {
    const pr = bonusProj(it.z, it.a, bonus.t);
    const spr = it.bomb ? bombSpr : cheeseSpr;
    const sw = spr.width * Math.min(1.4, pr.s * 2.6), sh = spr.height * Math.min(1.4, pr.s * 2.6);
    if (sw < 1) continue;
    ctx.drawImage(spr, pr.x - sw / 2, pr.y - sh / 2, sw, sh);
  }
  // player ball on near ring
  const pp = bonusProj(42, bonus.ang, bonus.t);
  ctx.save();
  ctx.translate(pp.x, pp.y - 6);
  ctx.rotate(bonus.t * 0.3);
  if (!(bonus.hurtT > 0 && (bonus.t & 4))) ctx.drawImage(ratBall, -8, -8);
  ctx.restore();
  // particles
  for (const pt of particles) { ctx.fillStyle = pt.color; ctx.fillRect(pt.x, pt.y, 2, 2); }

  drawTextC(ctx, 'BONUS STAGE', 160, 8, '#f8d838', 2);
  drawText(ctx, 'CHEESE ' + bonus.got, 8, 26, '#fff', 1);
  if (bonus.t < 180) drawTextC(ctx, 'COLLECT THE CHEESE!', 160, 200, (bonus.t & 16) ? '#fff' : '#f8d838', 1);
  if (bonus.over) {
    ctx.fillStyle = 'rgba(0,0,0,0.5)';
    ctx.fillRect(0, 80, W, 64);
    drawTextC(ctx, 'BONUS COMPLETE!', 160, 92, '#f8d838', 2);
    drawTextC(ctx, 'CHEESE ' + bonus.got + ' X 50 = ' + (bonus.got * 50), 160, 116, '#fff', 1);
  }
  if (bonus.hurtT > 20) {
    ctx.fillStyle = 'rgba(255,64,64,0.25)';
    ctx.fillRect(0, 0, W, H);
  }
}

// ---------------- world drawing ----------------
function drawSewerBG(camX, camY, tt) {
  const bg = BGS.sewer;
  ctx.fillStyle = bg.sky; ctx.fillRect(0, 0, W, H);
  const blit = (img, fac, yo) => {
    let ox = Math.floor((camX * fac) % img.width);
    if (ox < 0) ox += img.width;
    ctx.drawImage(img, -ox, yo);
    ctx.drawImage(img, -ox + img.width, yo);
    if (img.width * 2 - ox < W) ctx.drawImage(img, -ox + img.width * 2, yo);
  };
  blit(bg.far, 0.12, -Math.floor(camY * 0.1));
  blit(bg.mid, 0.3, -Math.floor(camY * 0.15));
  // LINE-SCROLL WATER: reflectively shimmering band, per-scanline offset
  const wy = bg.waterY - Math.floor(camY * 0.15);
  ctx.fillStyle = '#0e2a38';
  ctx.fillRect(0, wy, W, H - wy);
  for (let y = wy; y < H; y += 1) {
    const depth = y - wy;
    const srcY = Math.max(0, wy - depth * 1.5); // mirror-ish sample above waterline
    const ripple = Math.sin(y * 0.35 + tt * 0.07) * (1 + depth * 0.08);
    let ox = Math.floor((camX * (0.3 + depth * 0.002) + ripple) % 256);
    if (ox < 0) ox += 256;
    ctx.globalAlpha = Math.max(0.12, 0.5 - depth * 0.012);
    for (let dx = 0, sx = ox; dx < W; sx = 0) {
      const w2 = Math.min(256 - sx, W - dx);
      ctx.drawImage(BGS.sewer.mid, sx, srcY, w2, 1, dx, y, w2, 1);
      dx += w2;
    }
    ctx.globalAlpha = 1;
  }
  // waterline sparkle
  ctx.fillStyle = '#6ac8e0';
  for (let x = 0; x < W; x += 8)
    if ((x + tt * 2) % 48 < 8) ctx.fillRect(x, wy, 4, 1);
}
function drawRoofBG(camX, camY, tt) {
  const bg = BGS.roof;
  ctx.fillStyle = bg.sky; ctx.fillRect(0, 0, W, H);
  let ox = Math.floor((camX * 0.08) % 256); if (ox < 0) ox += 256;
  ctx.drawImage(bg.far, -ox, -Math.floor(camY * 0.08));
  ctx.drawImage(bg.far, -ox + 256, -Math.floor(camY * 0.08));
  if (512 - ox < W) ctx.drawImage(bg.far, -ox + 512, -Math.floor(camY * 0.08));
  // clouds with per-strip line-scroll drift (each 2px strip moves at its own speed)
  for (let y = 0; y < 60; y += 2) {
    const spd = 0.15 + y * 0.004;
    let cox = Math.floor((camX * spd + tt * (0.1 + y * 0.004)) % 256);
    if (cox < 0) cox += 256;
    for (let dx = 0, sx = cox; dx < W; sx = 0) {
      const w2 = Math.min(256 - sx, W - dx);
      ctx.drawImage(bg.clouds, sx, y, w2, 2, dx, 18 + y, w2, 2);
      dx += w2;
    }
  }
  let mx = Math.floor((camX * 0.28) % 256); if (mx < 0) mx += 256;
  ctx.drawImage(bg.mid, -mx, 30 - Math.floor(camY * 0.18));
  ctx.drawImage(bg.mid, -mx + 256, 30 - Math.floor(camY * 0.18));
  if (512 - mx < W) ctx.drawImage(bg.mid, -mx + 512, 30 - Math.floor(camY * 0.18));
}

function drawLoopBack(L, camX, camY) {
  const x = L.cx - camX, y = L.cy - camY;
  ctx.lineWidth = 7;
  ctx.strokeStyle = lvl.theme === 'sewer' ? '#27424d' : '#3a3a50';
  ctx.beginPath(); ctx.arc(x, y, L.r, 0, Math.PI * 2); ctx.stroke();
  ctx.lineWidth = 3;
  ctx.strokeStyle = lvl.theme === 'sewer' ? '#557a87' : '#8484a4';
  ctx.beginPath(); ctx.arc(x, y, L.r, 0, Math.PI * 2); ctx.stroke();
  // inner rim
  ctx.lineWidth = 1;
  ctx.strokeStyle = '#d8dce8';
  ctx.beginPath(); ctx.arc(x, y, L.r - 5, 0, Math.PI * 2); ctx.stroke();
}
function drawLoopFront(L, camX, camY) {
  // front brace over the crossing point at the loop base
  const x = L.cx - camX, y = L.cy + L.r - camY;
  ctx.fillStyle = lvl.theme === 'sewer' ? '#557a87' : '#8484a4';
  ctx.fillRect(x - 10, y - 6, 20, 8);
  ctx.fillStyle = '#d8dce8';
  ctx.fillRect(x - 10, y - 6, 20, 1);
}
function drawCork(ck, camX, camY, tt) {
  const x0 = ck.x0 - camX, y = ck.cy - camY;
  const wpx = ck.x1 - ck.x0;
  const path = i => y - (1 - Math.cos((i / wpx) * Math.PI * 2)) * ck.amp;
  // twisted ribbon: two edges weave around the running path (cheap pseudo-3D)
  for (let pass = 0; pass < 2; pass++) {
    ctx.beginPath();
    for (let i = 0; i <= wpx; i += 4) {
      const ph = (i / wpx) * Math.PI * 2;
      const weave = Math.sin(ph * 2 + (pass ? Math.PI : 0)) * 5;
      const yy = path(i) + weave;
      if (i === 0) ctx.moveTo(x0 + i, yy); else ctx.lineTo(x0 + i, yy);
    }
    ctx.lineWidth = 3;
    ctx.strokeStyle = pass ? '#2a3a44' : '#7a9aa8';
    ctx.stroke();
  }
  // crossbars between the ribbon edges
  for (let i = 8; i < wpx; i += 12) {
    const ph = (i / wpx) * Math.PI * 2;
    const w1 = Math.sin(ph * 2) * 5, w2 = Math.sin(ph * 2 + Math.PI) * 5;
    ctx.strokeStyle = '#4a5a64';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(x0 + i, path(i) + w1);
    ctx.lineTo(x0 + i, path(i) + w2);
    ctx.stroke();
  }
}

function drawTiles(camX, camY, tt) {
  const set = TILES[lvl.theme];
  const c0 = Math.max(0, Math.floor(camX / TILE)), c1 = Math.min(lvl.w - 1, Math.ceil((camX + W) / TILE));
  const r0 = Math.max(0, Math.floor(camY / TILE)), r1 = Math.min(lvl.h - 1, Math.ceil((camY + H) / TILE));
  for (let r = r0; r <= r1; r++) for (let c = c0; c <= c1; c++) {
    const ch = lvl.map[r][c];
    if (ch === ' ') continue;
    const x = c * TILE - camX, y = r * TILE - camY;
    if (set[ch]) ctx.drawImage(set[ch], Math.floor(x), Math.floor(y));
    else if (ch === '^') ctx.drawImage(spikeSpr, Math.floor(x), Math.floor(y));
  }
}

function drawPlayer(camX, camY) {
  if (p.invuln > 0 && (p.invuln & 4)) return; // blink while invulnerable
  const cx = p.x + p.w / 2 - camX, cy = p.y + p.h / 2 - camY;
  ctx.save();
  ctx.translate(Math.floor(cx), Math.floor(cy));
  if (p.mode === 'loop') {
    ctx.rotate(p.loopA + Math.PI / 2);
    ctx.scale(p.facing >= 0 ? 1 : -1, 1);
    ctx.drawImage(ratRun[Math.floor(p.anim) % 4], -13, -10);
  } else if (p.mode === 'cork') {
    ctx.rotate(p.corkAngle || 0);
    ctx.drawImage(ratRun[Math.floor(p.anim) % 4], -13, -10);
  } else if (p.ball) {
    ctx.rotate(p.rollA);
    ctx.drawImage(ratBall, -8, -8);
  } else if (p.mode === 'hurtfly' || p.mode === 'dead') {
    ctx.scale(p.facing, 1);
    ctx.drawImage(ratHurt, -13, -9);
  } else {
    ctx.scale(p.facing, 1);
    let spr;
    if (Math.abs(p.vx) > 0.3) spr = ratRun[Math.floor(p.anim) % 4];
    else spr = (p.blink > 170) ? ratIdleBlink : ratIdle;
    ctx.drawImage(spr, -13, -10);
  }
  ctx.restore();
}

function drawObjects(camX, camY, tt) {
  for (const o of objects) {
    if (!o.live) continue;
    const x = Math.floor(o.x - camX), y = Math.floor(o.y - camY);
    if (x < -40 || x > W + 40) continue;
    if (o.t === 'cheese') {
      const bob = Math.floor(Math.sin(tt * 0.08 + o.x * 0.3) * 2);
      ctx.drawImage(cheeseSpr, x, y + bob);
    } else if (o.t === 'enemy') {
      ctx.save();
      ctx.translate(x + o.w / 2, y);
      ctx.scale(o.vx > 0 ? -1 : 1, 1);
      ctx.drawImage(catbot[o.f < 10 ? 0 : 1], -o.w / 2, 0);
      ctx.restore();
    } else if (o.t === 'spring') {
      ctx.drawImage(springSpr[o.anim > 0 ? 1 : 0], x, y);
    } else if (o.t === 'lamp') {
      ctx.drawImage(o.on ? lampOn : lampOff, x, y);
    } else if (o.t === 'goal') {
      // post
      ctx.fillStyle = '#506070';
      ctx.fillRect(x + 9, y + 10, 3, 22);
      // spinning sign: squash horizontally, swap face at the flip
      const ph = o.hit ? Math.cos(o.spin * 0.25) : 1;
      const showRat = o.hit && (o.spin > 140 || Math.floor(o.spin * 0.25 / Math.PI) % 2 === 1);
      const sw = Math.max(1, Math.abs(ph) * 20);
      ctx.drawImage(showRat ? signRat : signCat, x + 10 - sw / 2, y, sw, 20);
    }
  }
  for (const s of scattered) {
    if (s.t > 240 && (s.t & 4)) continue;
    ctx.drawImage(cheeseSpr, Math.floor(s.x - 6 - camX), Math.floor(s.y - 5 - camY));
  }
  for (const pt of particles) {
    ctx.fillStyle = pt.color;
    ctx.fillRect(Math.floor(pt.x - camX), Math.floor(pt.y - camY), 2, 2);
  }
}

function drawHUD() {
  drawText(ctx, 'SCORE', 8, 6, '#f8d838', 1);
  drawText(ctx, '' + score, 8, 13, '#fff', 1);
  drawText(ctx, 'TIME', 8, 22, '#f8d838', 1);
  const sec = Math.floor(time / 60);
  drawText(ctx, Math.floor(sec / 60) + ':' + ('0' + sec % 60).slice(-2), 8, 29, '#fff', 1);
  ctx.drawImage(cheeseSpr, 270, 6);
  drawText(ctx, 'X ' + cheese, 286, 8, (cheese === 0 && (time & 16)) ? '#f86060' : '#fff', 1);
}

// ---------------- state updates / draws ----------------
function updateCamera() {
  const tx = p.x + p.w / 2 - W / 2 + p.facing * 24;
  cam.x += (tx - cam.x) * 0.12;
  const ty = p.y + p.h / 2 - H * 0.55;
  cam.y += (ty - cam.y) * 0.1;
  cam.x = Math.max(0, Math.min(cam.x, lvl.w * TILE - W));
  cam.y = Math.max(0, Math.min(cam.y, lvl.h * TILE - H));
}

let frame = 0;
function update() {
  frame++;
  if (state === 'title') {
    if (pressed.start || pressed.jump) {
      state = 'card'; stateT = 0;
      levelIdx = 0; score = 0; totalCheese = 0; bonusResult = 0; cheese = 0;
      checkpoint = null;
      loadLevel(0, false);
      setMusic(null);
    }
  } else if (state === 'card') {
    stateT++;
    if (stateT === 1) setMusic(null);
    if (stateT > 130) {
      state = 'play';
      setMusic(lvl.music);
    }
  } else if (state === 'play') {
    if (pressed.start) paused = !paused;
    if (!paused) {
      time++;
      updatePlayer();
      updateObjects();
      updateCamera();
    }
  } else if (state === 'results') {
    stateT++;
    if (stateT > 90 && (pressed.start || pressed.jump)) {
      if (levelIdx === 0) {
        state = 'bonus'; stateT = 0;
        initBonus();
        setMusic('bonus');
      } else {
        state = 'end'; stateT = 0;
        setMusic(null);
      }
    }
  } else if (state === 'bonus') {
    stateT++;
    updateBonus();
  } else if (state === 'end') {
    stateT++;
    if (stateT > 120 && (pressed.start || pressed.jump)) {
      state = 'title'; stateT = 0;
      setMusic(null);
    }
  }
  if (pressed.mute) {
    muted = !muted;
  }
  if (shake > 0) shake--;
  if (flash > 0) flash--;
  pressed = {};
}

function drawTitle() {
  drawRoofBG(frame * 0.4, 0, frame);
  // ground strip
  ctx.fillStyle = '#5a5a74'; ctx.fillRect(0, 180, W, 44);
  ctx.fillStyle = '#8484a4'; ctx.fillRect(0, 180, W, 2);
  // logo
  ctx.fillStyle = 'rgba(0,0,0,0.35)';
  ctx.fillRect(40, 38, 240, 78);
  drawTextC(ctx, 'RUSTY', 161, 49, '#201828', 6);
  drawTextC(ctx, 'RUSTY', 160, 48, '#f8d838', 6);
  drawTextC(ctx, 'THE RAT', 161, 86, '#201828', 3);
  drawTextC(ctx, 'THE RAT', 160, 85, '#f08caa', 3);
  drawTextC(ctx, '16-BIT TAIL EDITION', 160, 106, '#9aa8bc', 1);
  // running rat
  ctx.drawImage(ratRun[Math.floor(frame / 5) % 4], 148, 162);
  if (frame & 32) drawTextC(ctx, 'PRESS ENTER', 160, 136, '#fff', 2);
  drawTextC(ctx, 'ARROWS/WASD MOVE . Z/SPACE JUMP . M MUTE', 160, 206, '#b0b0c8', 1);
}
function drawCard() {
  // genesis-style level card: sliding color bands
  const t = stateT;
  ctx.fillStyle = '#101018'; ctx.fillRect(0, 0, W, H);
  const slide = Math.min(1, t / 25);
  const out = Math.max(0, (t - 105) / 25);
  const bx = Math.floor(-W + slide * W + out * W);
  ctx.fillStyle = lvl.theme === 'sewer' ? '#27424d' : '#403060';
  ctx.fillRect(bx, 70, W, 40);
  ctx.fillStyle = lvl.theme === 'sewer' ? '#3d5a66' : '#584888';
  ctx.fillRect(-bx, 112, W, 24);
  ctx.fillStyle = '#f8d838';
  ctx.fillRect(bx, 108, W, 3);
  if (t > 25 && t < 110) {
    drawTextC(ctx, lvl.name, 160, 82, '#fff', 3);
    drawTextC(ctx, 'ACT ' + (levelIdx + 1), 160, 118, '#f8d838', 2);
  }
}
function drawPlay() {
  const sx = shake > 0 ? Math.floor((Math.random() - 0.5) * shake) : 0;
  const sy = shake > 0 ? Math.floor((Math.random() - 0.5) * shake) : 0;
  const camX = Math.floor(cam.x) + sx, camY = Math.floor(cam.y) + sy;
  if (lvl.theme === 'sewer') drawSewerBG(camX, camY, frame);
  else drawRoofBG(camX, camY, frame);
  for (const L of loops) drawLoopBack(L, camX, camY);
  for (const ck of corks) drawCork(ck, camX, camY, frame);
  drawTiles(camX, camY, frame);
  drawObjects(camX, camY, frame);
  drawPlayer(camX, camY);
  for (const L of loops) drawLoopFront(L, camX, camY);
  drawHUD();
  if (paused) {
    ctx.fillStyle = 'rgba(0,0,0,0.5)'; ctx.fillRect(0, 0, W, H);
    drawTextC(ctx, 'PAUSED', 160, 104, '#fff', 3);
  }
}
function drawResults() {
  drawPlay();
  ctx.fillStyle = 'rgba(16,16,24,0.75)';
  ctx.fillRect(0, 56, W, 110);
  ctx.fillStyle = '#f8d838'; ctx.fillRect(0, 56, W, 2); ctx.fillRect(0, 164, W, 2);
  drawTextC(ctx, 'RUSTY GOT THROUGH', 160, 68, '#f8d838', 2);
  drawTextC(ctx, lvl.name.replace('ZONE', '') + ' ACT ' + (levelIdx + 1), 160, 88, '#fff', 1);
  if (stateT > 30) drawTextC(ctx, 'CHEESE BONUS  ' + cheese + ' X 100', 160, 108, '#fff', 1);
  if (stateT > 60) {
    const sec = Math.floor(time / 60);
    drawTextC(ctx, 'TIME ' + Math.floor(sec / 60) + ':' + ('0' + sec % 60).slice(-2), 160, 122, '#fff', 1);
  }
  if (stateT > 90) {
    drawTextC(ctx, 'SCORE ' + score, 160, 138, '#f8d838', 1);
    if (stateT & 32) drawTextC(ctx, levelIdx === 0 ? 'PRESS ENTER FOR BONUS STAGE' : 'PRESS ENTER', 160, 152, '#fff', 1);
  }
}
function drawEnd() {
  drawSewerBG(frame * 0.3, 0, frame);
  ctx.fillStyle = 'rgba(12,12,20,0.7)'; ctx.fillRect(0, 0, W, H);
  drawTextC(ctx, 'THE SEWERS ARE SAFE!', 160, 48, '#f8d838', 2);
  ctx.drawImage(ratRun[Math.floor(frame / 6) % 4], 148, 70);
  drawTextC(ctx, 'FINAL SCORE ' + score, 160, 104, '#fff', 2);
  drawTextC(ctx, 'CHEESE COLLECTED ' + totalCheese, 160, 126, '#fff', 1);
  drawTextC(ctx, 'BONUS STAGE CHEESE ' + bonusResult, 160, 138, '#fff', 1);
  drawTextC(ctx, 'THANKS FOR PLAYING', 160, 160, '#f08caa', 1);
  if (stateT > 120 && (frame & 32)) drawTextC(ctx, 'PRESS ENTER', 160, 190, '#fff', 1);
}

function draw() {
  if (state === 'title') drawTitle();
  else if (state === 'card') drawCard();
  else if (state === 'play') drawPlay();
  else if (state === 'results') drawResults();
  else if (state === 'bonus') drawBonus();
  else if (state === 'end') drawEnd();
  if (muted) drawText(ctx, 'MUTE', 290, 214, '#888', 1);
}

// ---------------- main loop (fixed 60fps step) ----------------
let last = 0, acc = 0;
function loop(ts) {
  requestAnimationFrame(loop);
  if (!last) last = ts;
  acc += Math.min(ts - last, 100);
  last = ts;
  const STEP = 1000 / 60;
  let n = 0;
  while (acc >= STEP && n < 4) { update(); acc -= STEP; n++; }
  if (acc >= STEP) acc = 0;
  draw();
}
requestAnimationFrame(loop);
