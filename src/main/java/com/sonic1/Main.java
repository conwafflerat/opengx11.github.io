package com.sonic1;

import com.sonic1.engine.Display;
import com.sonic1.engine.Input;
import com.sonic1.engine.Renderer;
import com.sonic1.game.Constants;
import com.sonic1.game.Game;

public final class Main {

    public static void main(String[] args) {
        for (String a : args) {
            if (a.equals("--selftest")) { System.exit(runSelfTest() ? 0 : 1); }
        }
        runGame();
    }

    private static void runGame() {
        Input input = new Input();
        Renderer renderer = new Renderer(Constants.SCREEN_W, Constants.SCREEN_H);
        Display display = new Display(Constants.SCREEN_W, Constants.SCREEN_H, 3, "Sonic 1 (Java / LWJGL)", input);
        Game game = new Game();

        final long step = (long) (1_000_000_000.0 / Constants.FPS);
        long last = System.nanoTime();
        long acc = 0;

        while (!display.shouldClose()) {
            display.pollEvents();

            long now = System.nanoTime();
            acc += now - last;
            last = now;

            int safety = 0;
            while (acc >= step && safety++ < 5) {
                input.poll();
                game.update(input);
                acc -= step;
            }

            game.render(renderer);
            display.present(renderer.pixels);
        }

        display.destroy();
    }

    /**
     * Headless physics check (no window): drops Sonic, runs him right to top
     * speed, jumps him, and confirms he lands again. Lets the engine be verified
     * in environments with no display.
     */
    private static boolean runSelfTest() {
        Game g = new Game();
        Input in = new Input();
        boolean ok = true;

        for (int i = 0; i < 120; i++) { in.poll(); g.update(in); }
        ok &= expect(g.player.onGround, "Sonic settles on the ground");

        int startX = g.player.pixelX();
        in.scriptHold(Input.RIGHT, true);
        // 140 frames: enough to hit top speed, short enough to stop before the pit.
        for (int i = 0; i < 140; i++) { in.poll(); g.update(in); }
        ok &= expect(g.player.pixelX() > startX, "running right advances X (" + startX + " -> " + g.player.pixelX() + ")");
        ok &= expect(Math.abs(g.player.groundVel) >= Constants.TOP_SPEED - 0x80,
                "reaches ~top speed (groundVel=" + g.player.groundVel + ", top=" + Constants.TOP_SPEED + ")");

        in.scriptHold(Input.RIGHT, false);
        in.scriptHold(Input.A, true);
        in.poll(); g.update(in);
        ok &= expect(!g.player.onGround && g.player.yVel < 0, "jump leaves the ground (yVel=" + g.player.yVel + ")");

        in.scriptHold(Input.A, false);
        boolean landed = false;
        for (int i = 0; i < 240; i++) {
            in.poll(); g.update(in);
            if (g.player.onGround) { landed = true; break; }
        }
        ok &= expect(landed, "lands after the jump");

        System.out.println(ok ? "SELF-TEST PASSED" : "SELF-TEST FAILED");
        return ok;
    }

    private static boolean expect(boolean cond, String label) {
        System.out.println((cond ? "  [ok]   " : "  [FAIL] ") + label);
        return cond;
    }
}
