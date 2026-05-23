"""Display + input front-end.

Uses pygame if it is installed; otherwise falls back to a headless mode that
can write frames to disk as PPM images (no third-party dependency required).
"""

from __future__ import annotations

try:
    import pygame
    HAVE_PYGAME = True
except Exception:  # pragma: no cover - pygame is optional
    HAVE_PYGAME = False


KEYMAP = {}
if HAVE_PYGAME:
    KEYMAP = {
        pygame.K_UP: "up",
        pygame.K_DOWN: "down",
        pygame.K_LEFT: "left",
        pygame.K_RIGHT: "right",
        pygame.K_z: "a",
        pygame.K_x: "b",
        pygame.K_c: "c",
        pygame.K_RETURN: "start",
    }


def save_ppm(path: str, width: int, height: int, rgb: bytes):
    """Write an RGB framebuffer as a binary PPM (P6) image."""
    with open(path, "wb") as fh:
        fh.write(f"P6\n{width} {height}\n255\n".encode("ascii"))
        fh.write(bytes(rgb))


class PygameDisplay:
    def __init__(self, scale: int = 2, title: str = "pygen"):
        if not HAVE_PYGAME:
            raise RuntimeError("pygame is not installed")
        pygame.init()
        self.scale = scale
        self.title = title
        self.screen = None
        self.clock = pygame.time.Clock()

    def _ensure(self, width: int, height: int):
        size = (width * self.scale, height * self.scale)
        if self.screen is None or self.screen.get_size() != size:
            self.screen = pygame.display.set_mode(size)
            pygame.display.set_caption(self.title)

    def pump(self, io) -> bool:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
            if event.type in (pygame.KEYDOWN, pygame.KEYUP):
                if event.key == pygame.K_ESCAPE:
                    return False
                btn = KEYMAP.get(event.key)
                if btn:
                    io.set_button(btn, event.type == pygame.KEYDOWN)
        return True

    def show(self, width: int, height: int, rgb: bytes, fps: int = 60):
        self._ensure(width, height)
        surf = pygame.image.frombuffer(bytes(rgb), (width, height), "RGB")
        if self.scale != 1:
            surf = pygame.transform.scale(
                surf, (width * self.scale, height * self.scale))
        self.screen.blit(surf, (0, 0))
        pygame.display.flip()
        self.clock.tick(fps)

    def quit(self):
        pygame.quit()
