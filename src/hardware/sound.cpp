#include "sound.hpp"
#include "ym3438.h"
#include <cstring>
#include <algorithm>
#include <cmath>

// ---- PSG ---------------------------------------------------------------

void PSG::reset()
{
    for (auto &c : m_tone) { c.period = 0; c.attenuation = 15; c.counter = 0; c.output = 1; }
    m_noise = {};
    m_noise.attenuation = 15;
    m_noise.lfsr = 0x8000;
    m_noise.output = 1;
    m_clk_acc = 0.0;
}

void PSG::write(uint8_t data)
{
    if (data & 0x80) {
        m_latch_ch   = (data >> 5) & 3;
        m_latch_type = (data >> 4) & 1;
        uint8_t v    = data & 0x0F;
        if (m_latch_ch < 3) {
            if (m_latch_type) m_tone[m_latch_ch].attenuation = v;
            else              m_tone[m_latch_ch].period = (m_tone[m_latch_ch].period & 0x3F0) | v;
        } else {
            if (m_latch_type) m_noise.attenuation = v;
            else { m_noise.mode = (v >> 2) & 1; m_noise.shift_rate = v & 3; m_noise.lfsr = 0x8000; }
        }
    } else {
        if (!m_latch_type && m_latch_ch < 3)
            m_tone[m_latch_ch].period = (m_tone[m_latch_ch].period & 0xF) | ((uint16_t)(data & 0x3F) << 4);
    }
}

static const int16_t k_atten[16] = {
    32767,26028,20675,16422,13045,10362,8231,6538,5193,4125,3276,2602,2067,1642,1304,0
};

void PSG::generate(int16_t *buf, int samples, double clk_per_sample)
{
    for (int i = 0; i < samples; i++) {
        int clk = (int)(m_clk_acc + clk_per_sample) - (int)m_clk_acc;
        m_clk_acc += clk_per_sample;

        for (int c2 = 0; c2 < clk; c2++) {
            for (auto &t : m_tone)
                if (t.period && --t.counter == 0) { t.counter = t.period; t.output = -t.output; }
            // Noise
            uint16_t reload;
            switch (m_noise.shift_rate) {
            case 0: reload = 16; break; case 1: reload = 32; break; case 2: reload = 64; break;
            default: reload = m_tone[2].period ? m_tone[2].period : 1; break;
            }
            if (m_noise.counter-- == 0) {
                m_noise.counter = reload;
                uint16_t fb = m_noise.mode
                    ? ((m_noise.lfsr & 1) ^ ((m_noise.lfsr >> 3) & 1))
                    : (m_noise.lfsr & 1);
                m_noise.lfsr = (m_noise.lfsr >> 1) | (fb << 15);
                m_noise.output = (m_noise.lfsr & 1) ? 1 : -1;
            }
        }

        int32_t s = 0;
        for (auto &t : m_tone) s += t.output * k_atten[t.attenuation];
        s += m_noise.output * k_atten[m_noise.attenuation];
        s /= 4;
        buf[i] = (int16_t)std::clamp(s, -32768, 32767);
    }
    m_clk_acc -= (int)m_clk_acc;
}

// ---- YM2612 ------------------------------------------------------------

YM2612::YM2612() : m_chip(new ym3438_t)
{
    OPN2_SetChipType(ym3438_mode_ym2612);
    OPN2_Reset(m_chip);
}

YM2612::~YM2612() { delete m_chip; }

void YM2612::reset() { OPN2_Reset(m_chip); m_clk_acc = 0; }

void YM2612::write(int port, bool is_data, uint8_t value)
{
    if (!is_data) {
        m_addr[port] = value;
        OPN2_Write(m_chip, port * 2, value);
    } else {
        OPN2_Write(m_chip, port * 2 + 1, value);
    }
}

void YM2612::generate(int16_t *stereo_buf, int samples, double clk_per_sample)
{
    for (int i = 0; i < samples; i++) {
        int clk = (int)(m_clk_acc + clk_per_sample) - (int)m_clk_acc;
        m_clk_acc += clk_per_sample;
        int32_t l = 0, r = 0;
        for (int c2 = 0; c2 < clk; c2++) {
            int32_t cl = 0, cr = 0;
            OPN2_Clock(m_chip, &cl, &cr);
            l += cl; r += cr;
        }
        if (clk) { l /= clk; r /= clk; }
        stereo_buf[i*2]   = (int16_t)std::clamp(l, -32768, 32767);
        stereo_buf[i*2+1] = (int16_t)std::clamp(r, -32768, 32767);
    }
    m_clk_acc -= (int)m_clk_acc;
}

// ---- Sound system ------------------------------------------------------

void Sound::sdl_callback(void *userdata, uint8_t *stream, int len)
{
    auto *s = static_cast<Sound *>(userdata);
    int samples = len / sizeof(int16_t);
    SDL_LockMutex(s->m_lock);
    int avail = s->m_write - s->m_read;
    if (avail < 0) avail += (int)s->m_ring.size();
    for (int i = 0; i < samples; i++) {
        if (avail > 0) {
            reinterpret_cast<int16_t *>(stream)[i] = s->m_ring[s->m_read % s->m_ring.size()];
            s->m_read = (s->m_read + 1) % (int)s->m_ring.size();
            avail--;
        } else {
            reinterpret_cast<int16_t *>(stream)[i] = 0;
        }
    }
    SDL_UnlockMutex(s->m_lock);
}

Sound::Sound() { m_ring.resize(AUDIO_RATE * 4); }
Sound::~Sound() { shutdown(); }

bool Sound::init()
{
    m_lock = SDL_CreateMutex();
    m_psg.reset();
    m_ym2612.reset();

    SDL_AudioSpec want{};
    want.freq     = AUDIO_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = AUDIO_BUFSIZE;
    want.callback = sdl_callback;
    want.userdata = this;

    m_dev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (!m_dev) return false;
    SDL_PauseAudioDevice(m_dev, 0);
    return true;
}

void Sound::shutdown()
{
    if (m_dev) { SDL_CloseAudioDevice(m_dev); m_dev = 0; }
    if (m_lock) { SDL_DestroyMutex(m_lock); m_lock = nullptr; }
}

void Sound::mix_into(int16_t *dst, int stereo_samples)
{
    static thread_local std::vector<int16_t> fm_buf, psg_buf;
    fm_buf.resize(stereo_samples * 2);
    psg_buf.resize(stereo_samples);

    double fm_cps  = FM_CLOCK  / AUDIO_RATE;
    double psg_cps = PSG_CLOCK / AUDIO_RATE;

    m_ym2612.generate(fm_buf.data(), stereo_samples, fm_cps);
    m_psg.generate(psg_buf.data(), stereo_samples, psg_cps);

    for (int i = 0; i < stereo_samples; i++) {
        int32_t l = (int32_t)fm_buf[i*2]   + psg_buf[i];
        int32_t r = (int32_t)fm_buf[i*2+1] + psg_buf[i];
        dst[i*2]   = (int16_t)std::clamp(l, -32768, 32767);
        dst[i*2+1] = (int16_t)std::clamp(r, -32768, 32767);
    }
}

void Sound::submit_frame()
{
    std::vector<int16_t> buf(SPF * 2);
    mix_into(buf.data(), SPF);

    SDL_LockMutex(m_lock);
    for (auto s : buf) {
        int next = (m_write + 1) % (int)m_ring.size();
        if (next != m_read) { m_ring[m_write] = s; m_write = next; }
    }
    SDL_UnlockMutex(m_lock);
}
