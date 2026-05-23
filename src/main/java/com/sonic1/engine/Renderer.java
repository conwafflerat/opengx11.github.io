package com.sonic1.engine;

import java.util.Arrays;

/**
 * Software framebuffer. The game rasterises into {@link #pixels} (ARGB, one int
 * per pixel) and the active backend (LWJGL) uploads it to a GL texture. Keeping
 * a CPU framebuffer makes the output pixel-exact and the rendering code
 * independent of any graphics API.
 */
public final class Renderer {
    public final int width;
    public final int height;
    public final int[] pixels;

    public Renderer(int width, int height) {
        this.width = width;
        this.height = height;
        this.pixels = new int[width * height];
    }

    public void clear(int rgb) {
        Arrays.fill(pixels, 0xFF000000 | rgb);
    }

    public void setPixel(int x, int y, int rgb) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        pixels[y * width + x] = 0xFF000000 | rgb;
    }

    public void fillRect(int x, int y, int w, int h, int rgb) {
        int x2 = x + w, y2 = y + h;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x2 > width) x2 = width;
        if (y2 > height) y2 = height;
        int c = 0xFF000000 | rgb;
        for (int yy = y; yy < y2; yy++) {
            int row = yy * width;
            for (int xx = x; xx < x2; xx++) pixels[row + xx] = c;
        }
    }

    public void fillCircle(int cx, int cy, int r, int rgb) {
        int c = 0xFF000000 | rgb;
        int r2 = r * r;
        for (int dy = -r; dy <= r; dy++) {
            int py = cy + dy;
            if (py < 0 || py >= height) continue;
            int span = (int) Math.sqrt(r2 - dy * dy);
            int xa = cx - span, xb = cx + span;
            if (xa < 0) xa = 0;
            if (xb >= width) xb = width - 1;
            int row = py * width;
            for (int xx = xa; xx <= xb; xx++) pixels[row + xx] = c;
        }
    }
}
