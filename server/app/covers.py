from __future__ import annotations

import asyncio
from io import BytesIO
from pathlib import Path

import httpx
from PIL import Image, ImageEnhance, ImageFilter, ImageOps


COVER_WIDTH = 100
COVER_HEIGHT = 100
COVER_BYTES = COVER_WIDTH * COVER_HEIGHT * 2
BACKGROUND_WIDTH = 64
BACKGROUND_HEIGHT = 48
BACKGROUND_BYTES = BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 2


class CoverNotFound(RuntimeError):
    pass


class CoverService:
    def __init__(self, data_dir: Path) -> None:
        self.cache_dir = data_dir / "covers"
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self._sources: dict[str, str] = {}
        self._lock = asyncio.Lock()

    def register(self, track_id: str | None, cover_url: str | None) -> None:
        if track_id and cover_url and track_id.isalnum():
            self._sources[track_id] = cover_url

    async def rgb565(self, track_id: str) -> bytes:
        self._validate_track_id(track_id)
        cache_file = self._cover_cache_file(track_id)
        if cache_file.exists() and cache_file.stat().st_size == COVER_BYTES:
            return cache_file.read_bytes()

        source_url = self._sources.get(track_id)
        if not source_url:
            raise CoverNotFound("Capa ainda nao registrada. Consulte /api/player primeiro.")

        async with self._lock:
            if cache_file.exists() and cache_file.stat().st_size == COVER_BYTES:
                return cache_file.read_bytes()

            async with httpx.AsyncClient(timeout=20, follow_redirects=True) as client:
                response = await client.get(source_url)
                response.raise_for_status()

            raw = self._convert_cover(response.content)
            self._write_cache(cache_file, raw)

            # A primeira requisicao da capa ja deixa o fundo pronto. Assim o
            # ESP32 nao causa um segundo download da imagem original.
            background_file = self._background_cache_file(track_id)
            if not background_file.exists():
                background = self._convert_background(response.content)
                self._write_cache(background_file, background)
            return raw

    async def background_rgb565(self, track_id: str) -> bytes:
        self._validate_track_id(track_id)
        cache_file = self._background_cache_file(track_id)
        if cache_file.exists() and cache_file.stat().st_size == BACKGROUND_BYTES:
            return cache_file.read_bytes()

        async with self._lock:
            if cache_file.exists() and cache_file.stat().st_size == BACKGROUND_BYTES:
                return cache_file.read_bytes()

            cover_file = self._cover_cache_file(track_id)
            if cover_file.exists() and cover_file.stat().st_size == COVER_BYTES:
                image = self._decode_rgb565(
                    cover_file.read_bytes(),
                    COVER_WIDTH,
                    COVER_HEIGHT,
                )
                raw = self._prepare_background(image)
            else:
                source_url = self._sources.get(track_id)
                if not source_url:
                    raise CoverNotFound(
                        "Capa ainda nao registrada. Consulte /api/player primeiro."
                    )
                async with httpx.AsyncClient(
                    timeout=20,
                    follow_redirects=True,
                ) as client:
                    response = await client.get(source_url)
                    response.raise_for_status()
                raw = self._convert_background(response.content)

            self._write_cache(cache_file, raw)
            return raw

    def _cover_cache_file(self, track_id: str) -> Path:
        return self.cache_dir / f"{track_id}.rgb565"

    def _background_cache_file(self, track_id: str) -> Path:
        return self.cache_dir / f"{track_id}.background.rgb565"

    @staticmethod
    def _validate_track_id(track_id: str) -> None:
        if not track_id.isalnum():
            raise CoverNotFound("Identificador de faixa invalido.")

    @staticmethod
    def _write_cache(cache_file: Path, content: bytes) -> None:
        temporary = cache_file.with_suffix(cache_file.suffix + ".tmp")
        temporary.write_bytes(content)
        temporary.replace(cache_file)

    @classmethod
    def _convert_cover(cls, source: bytes) -> bytes:
        with Image.open(BytesIO(source)) as opened:
            image = opened.convert("RGB").resize(
                (COVER_WIDTH, COVER_HEIGHT),
                Image.Resampling.LANCZOS,
            )
        return cls._encode_rgb565(image)

    @classmethod
    def _convert_background(cls, source: bytes) -> bytes:
        with Image.open(BytesIO(source)) as opened:
            image = opened.convert("RGB")
        return cls._prepare_background(image)

    @classmethod
    def _prepare_background(cls, image: Image.Image) -> bytes:
        background = ImageOps.fit(
            image,
            (BACKGROUND_WIDTH, BACKGROUND_HEIGHT),
            method=Image.Resampling.LANCZOS,
            centering=(0.5, 0.5),
        )
        background = background.filter(ImageFilter.GaussianBlur(radius=5))
        background = ImageEnhance.Color(background).enhance(1.12)
        background = ImageEnhance.Brightness(background).enhance(0.62)
        return cls._encode_rgb565(background)

    @staticmethod
    def _encode_rgb565(image: Image.Image) -> bytes:
        output = bytearray(image.width * image.height * 2)
        offset = 0
        for red, green, blue in image.getdata():
            value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            output[offset] = value & 0xFF
            output[offset + 1] = value >> 8
            offset += 2
        return bytes(output)

    @staticmethod
    def _decode_rgb565(
        content: bytes,
        width: int,
        height: int,
    ) -> Image.Image:
        rgb = bytearray(width * height * 3)
        destination = 0
        for offset in range(0, len(content) - 1, 2):
            value = content[offset] | (content[offset + 1] << 8)
            rgb[destination] = ((value >> 11) & 0x1F) * 255 // 31
            rgb[destination + 1] = ((value >> 5) & 0x3F) * 255 // 63
            rgb[destination + 2] = (value & 0x1F) * 255 // 31
            destination += 3
        return Image.frombytes("RGB", (width, height), bytes(rgb))

    @staticmethod
    def dominant_color(content: bytes) -> int:
        """Extrai uma cor representativa do RGB565 sem reabrir a imagem original."""
        buckets: dict[int, list[int]] = {}
        fallback_red = fallback_green = fallback_blue = fallback_count = 0

        for offset in range(0, len(content) - 1, 8):
            value = content[offset] | (content[offset + 1] << 8)
            red = ((value >> 11) & 0x1F) * 255 // 31
            green = ((value >> 5) & 0x3F) * 255 // 63
            blue = (value & 0x1F) * 255 // 31
            fallback_red += red
            fallback_green += green
            fallback_blue += blue
            fallback_count += 1

            brightest = max(red, green, blue)
            darkest = min(red, green, blue)
            if brightest < 28 or darkest > 232:
                continue

            key = ((red >> 4) << 8) | ((green >> 4) << 4) | (blue >> 4)
            bucket = buckets.setdefault(key, [0, 0, 0, 0, 0])
            saturation = brightest - darkest
            bucket[0] += 2 + saturation // 32
            bucket[1] += red
            bucket[2] += green
            bucket[3] += blue
            bucket[4] += 1

        if buckets:
            winner = max(buckets.values(), key=lambda item: item[0])
            red = winner[1] // winner[4]
            green = winner[2] // winner[4]
            blue = winner[3] // winner[4]
        elif fallback_count:
            red = fallback_red // fallback_count
            green = fallback_green // fallback_count
            blue = fallback_blue // fallback_count
        else:
            return 0x1ED760

        brightest = max(red, green, blue)
        if brightest < 96:
            scale = 96 / max(1, brightest)
            red, green, blue = (
                min(255, int(red * scale)),
                min(255, int(green * scale)),
                min(255, int(blue * scale)),
            )
        elif brightest > 210:
            scale = 210 / brightest
            red, green, blue = int(red * scale), int(green * scale), int(blue * scale)

        return (red << 16) | (green << 8) | blue
