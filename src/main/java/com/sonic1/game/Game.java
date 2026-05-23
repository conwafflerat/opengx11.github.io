package com.sonic1.game;

import com.sonic1.engine.Input;
import com.sonic1.engine.Renderer;

/** Owns the world state and drives one frame of simulation and rendering. */
public final class Game {
    public final Level level;
    public final Player player;
    public final Camera camera;

    private static final int SKY = 0x60A0E0;
    private static final int GRASS = 0x38B838;
    private static final int GRASS_DK = 0x208820;
    private static final int DIRT = 0xB06028;
    private static final int DIRT_DK = 0x8A4A1E;

    public Game() {
        level = Level.testZone();
        player = new Player(level.spawnX, level.spawnY);
        camera = new Camera();
        camera.snapTo(player, level);
    }

    public void update(Input in) {
        player.update(in, level);

        // Fell into a pit: respawn.
        if (player.pixelY() > level.pixelHeight() + 64) {
            player.setPixelX(level.spawnX);
            player.setPixelY(level.spawnY);
            player.xVel = player.yVel = player.groundVel = 0;
            player.onGround = true;
            player.rolling = player.jumping = false;
            camera.snapTo(player, level);
        }

        camera.update(player, level);
    }

    public void render(Renderer r) {
        r.clear(SKY);
        drawLevel(r);
        player.render(r, camera.x, camera.y);
    }

    private void drawLevel(Renderer r) {
        int t = Level.TILE;
        int c0 = camera.x / t;
        int r0 = camera.y / t;
        int c1 = (camera.x + r.width) / t + 1;
        int r1 = (camera.y + r.height) / t + 1;

        for (int row = r0; row <= r1; row++) {
            for (int col = c0; col <= c1; col++) {
                if (!level.get(col, row)) continue;
                int sx = col * t - camera.x;
                int sy = row * t - camera.y;
                boolean surface = !level.get(col, row - 1);
                boolean dark = ((col + row) & 1) == 0;
                if (surface) {
                    r.fillRect(sx, sy, t, t, dark ? DIRT : DIRT_DK);
                    r.fillRect(sx, sy, t, 5, dark ? GRASS : GRASS_DK);
                } else {
                    r.fillRect(sx, sy, t, t, dark ? DIRT : DIRT_DK);
                }
            }
        }
    }
}
