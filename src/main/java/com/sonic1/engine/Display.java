package com.sonic1.engine;

import org.lwjgl.glfw.GLFWFramebufferSizeCallback;
import org.lwjgl.opengl.GL;
import org.lwjgl.system.MemoryUtil;

import java.nio.ByteBuffer;

import static org.lwjgl.glfw.GLFW.*;
import static org.lwjgl.opengl.GL11.*;

/**
 * LWJGL backend: a GLFW window with an OpenGL context. The CPU framebuffer is
 * uploaded to a single texture each frame and drawn as a screen-filling quad
 * with nearest-neighbour filtering, so the low-res image scales up crisply.
 */
public final class Display {
    private final int fbWidth;
    private final int fbHeight;
    private final String title;

    private long window;
    private int texture;
    private ByteBuffer pixelBuffer;
    private final Input input;

    public Display(int fbWidth, int fbHeight, int scale, String title, Input input) {
        this.fbWidth = fbWidth;
        this.fbHeight = fbHeight;
        this.title = title;
        this.input = input;
        init(scale);
    }

    private void init(int scale) {
        if (!glfwInit()) throw new IllegalStateException("Unable to initialize GLFW");

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(fbWidth * scale, fbHeight * scale, title, 0L, 0L);
        if (window == 0L) throw new RuntimeException("Failed to create GLFW window");

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // vsync
        GL.createCapabilities();

        glEnable(GL_TEXTURE_2D);
        glClearColor(0f, 0f, 0f, 1f);
        glColor3f(1f, 1f, 1f);

        texture = glGenTextures();
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbWidth, fbHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, (ByteBuffer) null);

        pixelBuffer = MemoryUtil.memAlloc(fbWidth * fbHeight * 4);

        glfwSetKeyCallback(window, this::onKey);
        glfwSetFramebufferSizeCallback(window, new GLFWFramebufferSizeCallback() {
            @Override public void invoke(long w, int width, int height) { glViewport(0, 0, width, height); }
        });

        glfwShowWindow(window);
    }

    private void onKey(long win, int key, int scancode, int action, int mods) {
        if (action == GLFW_REPEAT) return;
        boolean down = action == GLFW_PRESS;
        switch (key) {
            case GLFW_KEY_UP    -> input.set(Input.UP, down);
            case GLFW_KEY_DOWN  -> input.set(Input.DOWN, down);
            case GLFW_KEY_LEFT  -> input.set(Input.LEFT, down);
            case GLFW_KEY_RIGHT -> input.set(Input.RIGHT, down);
            case GLFW_KEY_Z, GLFW_KEY_SPACE -> input.set(Input.A, down);
            case GLFW_KEY_X     -> input.set(Input.B, down);
            case GLFW_KEY_C     -> input.set(Input.C, down);
            case GLFW_KEY_ENTER -> input.set(Input.START, down);
            case GLFW_KEY_ESCAPE -> { if (down) glfwSetWindowShouldClose(win, true); }
            default -> { }
        }
    }

    public void pollEvents() { glfwPollEvents(); }

    public boolean shouldClose() { return glfwWindowShouldClose(window); }

    public void present(int[] pixels) {
        pixelBuffer.clear();
        for (int p : pixels) {
            pixelBuffer.put((byte) ((p >> 16) & 0xFF)); // R
            pixelBuffer.put((byte) ((p >> 8) & 0xFF));  // G
            pixelBuffer.put((byte) (p & 0xFF));         // B
            pixelBuffer.put((byte) 0xFF);               // A
        }
        pixelBuffer.flip();

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fbWidth, fbHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer);

        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);  glLoadIdentity();

        glBegin(GL_QUADS);
        glTexCoord2f(0f, 0f); glVertex2f(-1f,  1f); // top-left
        glTexCoord2f(1f, 0f); glVertex2f( 1f,  1f); // top-right
        glTexCoord2f(1f, 1f); glVertex2f( 1f, -1f); // bottom-right
        glTexCoord2f(0f, 1f); glVertex2f(-1f, -1f); // bottom-left
        glEnd();

        glfwSwapBuffers(window);
    }

    public void destroy() {
        if (pixelBuffer != null) MemoryUtil.memFree(pixelBuffer);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}
