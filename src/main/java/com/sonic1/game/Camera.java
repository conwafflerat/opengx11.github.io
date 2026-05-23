package com.sonic1.game;

import static com.sonic1.game.Constants.SCREEN_H;
import static com.sonic1.game.Constants.SCREEN_W;

/**
 * Scroll camera matching Sonic 1's behaviour: a 16px horizontal dead-zone,
 * ground-follow capped at 6 px/frame (16 when moving fast), and a wider vertical
 * band while airborne.
 */
public final class Camera {
    public int x;
    public int y;

    public void snapTo(Player p, Level lvl) {
        x = p.pixelX() - SCREEN_W / 2;
        y = p.pixelY() - 96;
        clamp(lvl);
    }

    public void update(Player p, Level lvl) {
        int px = p.pixelX();
        int py = p.pixelY();

        int left = x + 144, right = x + 160;
        if (px < left) x -= Math.min(left - px, 16);
        else if (px > right) x += Math.min(px - right, 16);

        if (p.onGround) {
            int target = py - 96;
            int max = Math.abs(p.groundVel) >= 0x0800 ? 16 : 6;
            int dy = clampMag(target - y, max);
            y += dy;
        } else {
            int top = y + 64, bottom = y + 128;
            if (py < top) y -= Math.min(top - py, 16);
            else if (py > bottom) y += Math.min(py - bottom, 16);
        }

        clamp(lvl);
    }

    private void clamp(Level lvl) {
        int maxX = Math.max(0, lvl.pixelWidth() - SCREEN_W);
        int maxY = Math.max(0, lvl.pixelHeight() - SCREEN_H);
        if (x < 0) x = 0; if (x > maxX) x = maxX;
        if (y < 0) y = 0; if (y > maxY) y = maxY;
    }

    private static int clampMag(int v, int max) {
        if (v > max) return max;
        if (v < -max) return -max;
        return v;
    }
}
