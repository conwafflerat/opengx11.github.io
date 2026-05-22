#pragma once
#include <SDL.h>
#include <cstdint>
#include <array>
#include <vector>

// ============================================================
// Sound system: YM2612 (FM) + SN76489 (PSG)
//
// In the original game these are driven by the Z80 sound driver.
// In the native port the sound driver (music_ids, sfx_ids) maps
// directly to calls here rather than going through Z80 bus.
// ============================================================

static constexpr int AUDIO_RATE    = 44100;
static constexpr int AUDIO_BUFSIZE = 1024;

// ---------- PSG (SN76489) channel --------------------------------
struct PsgChannel {
    uint16_t period     = 0;
    uint8_t  attenuation = 15;  // 0 = full volume, 15 = silent
    uint16_t counter    = 0;
    int      output     = 1;
};

struct PsgNoise {
    uint8_t  mode        = 0;   // 0 = periodic, 1 = white
    uint8_t  shift_rate  = 0;   // 0-3
    uint8_t  attenuation = 15;
    uint16_t lfsr        = 0x8000;
    uint16_t counter     = 0;
    int      output      = 1;
};

class PSG {
public:
    void reset();
    void write(uint8_t data);
    void generate(int16_t *buf, int samples, double clk_per_sample);

private:
    PsgChannel m_tone[3];
    PsgNoise   m_noise;
    uint8_t    m_latch_ch   = 0;
    bool       m_latch_type = false;
    double     m_clk_acc    = 0.0;
};

// ---------- YM2612 wrapper around Nuked-OPN2 ---------------------
struct ym3438_t;  // forward-declared; definition comes from ym3438.h

class YM2612 {
public:
    YM2612();
    ~YM2612();

    void reset();
    // port: 0 or 1; is_data: true = data byte, false = address latch
    void write(int port, bool is_data, uint8_t value);
    void generate(int16_t *stereo_buf, int samples, double clk_per_sample);

private:
    ym3438_t *m_chip = nullptr;
    uint8_t   m_addr[2]{};
    double    m_clk_acc = 0.0;
};

// ---------- Sound system -----------------------------------------
class Sound {
public:
    Sound();
    ~Sound();

    bool init();
    void shutdown();

    PSG    &psg()    { return m_psg; }
    YM2612 &ym2612() { return m_ym2612; }

    // Called every frame to push a mixed audio frame into the SDL ring buffer
    void submit_frame();

private:
    static void sdl_callback(void *userdata, uint8_t *stream, int len);
    void mix_into(int16_t *dst, int stereo_samples);

    PSG             m_psg;
    YM2612          m_ym2612;
    SDL_AudioDeviceID m_dev = 0;

    std::vector<int16_t> m_ring;
    int m_write = 0, m_read = 0;
    SDL_mutex *m_lock = nullptr;

    static constexpr double FM_CLOCK  = 7670454.0;
    static constexpr double PSG_CLOCK = 3579545.0;
    static constexpr int    SPF       = AUDIO_RATE / 60 + 2;  // samples per frame
};
