#include "sound_driver.hpp"
#include "../hardware/sound.hpp"
#include <cstring>
#include <cstdint>

// ============================================================
// SMPS-compatible native sound driver.
//
// The Genesis SMPS (Sega Multi-Processor Sound) driver programs
// YM2612 FM channels and the PSG using a stream of events:
//   - Note on/off (frequency key)
//   - Duration (wait N ticks)
//   - FM operator register writes (instrument change)
//   - Loop/jump markers
//
// To complete this driver:
//   1. Extract the music/sfx binary data from s2disasm:
//      _inc/Sound/Sound.asm includes all the SMPS sequences
//   2. Convert them to the NoteEvent arrays below
//   3. Implement YM2612 key-on/key-off per channel (see fm_key_on below)
//
// Current status: ring SFX and a 4-bar EHZ BGM motif are stubbed.
// ============================================================

// YM2612 register helpers
static Sound *g_snd = nullptr;

// Write to YM2612 register (port 0 or 1, address, data)
static inline void fm_reg(int port, uint8_t addr, uint8_t data)
{
    if (g_snd) g_snd->ym2612().write(port, false, addr);
    if (g_snd) g_snd->ym2612().write(port, true,  data);
}

// PSG tone helper
static inline void psg_write(uint8_t data)
{
    if (g_snd) g_snd->psg().write(data);
}

// YM2612 frequency table for equal temperament (NTSC, fnum block 4)
// Formula: fnum = freq * 2^(20 - block) / FM_CLOCK  (FM_CLOCK ≈ 7.67 MHz)
// Block 4 covers roughly C4–B4.  Values from s2disasm's NoteFreqTable.
static const uint16_t fm_freq_table[12] = {
    0x026A, // C
    0x028F, // C#
    0x02B5, // D
    0x02DE, // D#
    0x030A, // E
    0x0338, // F
    0x036A, // F#
    0x039E, // G
    0x03D6, // G#
    0x0412, // A
    0x0452, // A#
    0x0496, // B
};

// Key-on: set frequency + trigger note for FM channel ch (0–5)
static void fm_key_on(int ch, int note_index, int octave)
{
    if (note_index < 0 || note_index > 11) return;
    int port = (ch >= 3) ? 1 : 0;
    int ch_r  = ch % 3;

    uint16_t fnum  = fm_freq_table[note_index];
    uint8_t  block = (uint8_t)((octave & 7) << 3);
    uint8_t  fnum_hi = block | (uint8_t)((fnum >> 8) & 7);
    uint8_t  fnum_lo = (uint8_t)(fnum & 0xFF);

    fm_reg(port, (uint8_t)(0xA4 + ch_r), fnum_hi);  // Freq MSB + block
    fm_reg(port, (uint8_t)(0xA0 + ch_r), fnum_lo);  // Freq LSB
    fm_reg(0,    0x28, (uint8_t)(0xF0 | (ch < 3 ? ch : ch + 1)));  // Key-on all ops
}

static void fm_key_off(int ch)
{
    fm_reg(0, 0x28, (uint8_t)(ch < 3 ? ch : ch + 1));  // Key-off (ops = 0)
}

// Set a basic FM instrument (4-op FM, algorithm 7 = all additive, simple sine)
static void fm_set_instrument_default(int ch)
{
    int port  = (ch >= 3) ? 1 : 0;
    int ch_r  = ch % 3;

    // Algorithm 7, feedback 0
    fm_reg(port, (uint8_t)(0xB0 + ch_r), 0x07);
    // LR panning: centre
    fm_reg(port, (uint8_t)(0xB4 + ch_r), 0xC0);
    // Operator 1 (0x30 series): DT1/MUL, TL, RS/AR, AM/DR, SR, SL/RR
    static const uint8_t op_offsets[] = {0x00, 0x04, 0x08, 0x0C};
    for (int op = 0; op < 4; ++op) {
        uint8_t o = op_offsets[op];
        fm_reg(port, (uint8_t)(0x30 + o + ch_r), 0x01);  // DT1=0 MUL=1
        fm_reg(port, (uint8_t)(0x40 + o + ch_r), 0x20);  // TL
        fm_reg(port, (uint8_t)(0x50 + o + ch_r), 0x1F);  // RS=0 AR=31
        fm_reg(port, (uint8_t)(0x60 + o + ch_r), 0x00);  // AM=0 DR=0
        fm_reg(port, (uint8_t)(0x70 + o + ch_r), 0x00);  // SR=0
        fm_reg(port, (uint8_t)(0x80 + o + ch_r), 0x0F);  // SL=0 RR=15
    }
}

// ---- Simple note sequence player ------------------------------------
struct NoteEvent {
    int8_t  note;     // semitone 0–11, or -1=rest, -2=end
    int8_t  octave;
    uint8_t duration; // frames to hold
};

// Minimal 8-note EHZ motif — replace with full sequence from s2disasm
static const NoteEvent ehz_bgm_ch0[] = {
    {4,4,8},{4,4,4},{7,4,8},{9,4,4},{11,4,8},{9,4,4},{7,4,8},{5,4,4},
    {4,4,8},{4,4,4},{7,4,8},{9,4,4},{11,4,16},{-1,0,8},{-2,0,0},
};

static const NoteEvent ring_sfx[] = {
    {11,5,4},{-1,0,2},{-2,0,0},
};

static const NoteEvent jump_sfx[] = {
    {7,5,3},{9,5,3},{11,5,3},{-2,0,0},
};

static const NoteEvent hurt_sfx[] = {
    {0,4,8},{-1,0,4},{-2,0,0},
};

// ---- Channel state --------------------------------------------------
struct Channel {
    const NoteEvent *seq       = nullptr;
    int              pos       = 0;
    int              timer     = 0;
    int              fm_ch     = -1;  // -1 = PSG
    bool             active    = false;
    bool             loop      = false;
};

static Channel g_bgm_channels[6];
static Channel g_sfx_channel;

static void channel_start(Channel &c, const NoteEvent *seq,
                           int fm_ch, bool loop)
{
    c.seq    = seq;
    c.pos    = 0;
    c.timer  = 0;
    c.fm_ch  = fm_ch;
    c.active = true;
    c.loop   = loop;
    if (fm_ch >= 0) fm_set_instrument_default(fm_ch);
}

static void channel_tick(Channel &c)
{
    if (!c.seq || !c.active) return;

    if (c.timer > 0) { --c.timer; return; }

    const NoteEvent &ev = c.seq[c.pos];
    if (ev.note == -2) {
        if (c.loop) c.pos = 0;
        else { c.active = false; if (c.fm_ch >= 0) fm_key_off(c.fm_ch); }
        return;
    }

    if (ev.note == -1) {
        if (c.fm_ch >= 0) fm_key_off(c.fm_ch);
    } else {
        if (c.fm_ch >= 0) fm_key_on(c.fm_ch, ev.note, ev.octave);
    }

    c.timer = ev.duration - 1;
    ++c.pos;
}

// ---- Public API -----------------------------------------------------
void sound_driver_init(Sound &snd)
{
    g_snd = &snd;
    memset(g_bgm_channels, 0, sizeof(g_bgm_channels));
    memset(&g_sfx_channel,  0, sizeof(g_sfx_channel));

    // YM2612 global init: LFO off, timer off
    fm_reg(0, 0x22, 0x00);  // LFO off
    fm_reg(0, 0x27, 0x00);  // CH3 normal, timers off
    fm_reg(0, 0x2B, 0x00);  // DAC off
}

void sound_driver_play_bgm(BGM bgm)
{
    sound_driver_stop();
    switch (bgm) {
    case BGM::EmeraldHill:
        channel_start(g_bgm_channels[0], ehz_bgm_ch0, 0, true);
        break;
    default:
        break;
    }
}

void sound_driver_play_sfx(SFX sfx)
{
    const NoteEvent *seq = nullptr;
    switch (sfx) {
    case SFX::Ring:  seq = ring_sfx;  break;
    case SFX::Jump:  seq = jump_sfx;  break;
    case SFX::Hurt:  seq = hurt_sfx;  break;
    default: break;
    }
    if (seq) channel_start(g_sfx_channel, seq, 5, false);
}

void sound_driver_stop()
{
    for (auto &c : g_bgm_channels) {
        c.active = false;
        if (c.fm_ch >= 0) fm_key_off(c.fm_ch);
    }
    g_sfx_channel.active = false;
}

void sound_driver_tick(Sound &snd)
{
    g_snd = &snd;
    for (auto &c : g_bgm_channels) channel_tick(c);
    channel_tick(g_sfx_channel);
}
