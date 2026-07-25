from __future__ import annotations

import asyncio
import bisect
import json
import re
from pathlib import Path
from typing import Any

import httpx


LRCLIB_URL = "https://lrclib.net/api/get"
TIMESTAMP = re.compile(r"\[(\d{1,3}):(\d{2})(?:[\.:](\d{1,3}))?\]")


class LyricsService:
    def __init__(self, data_dir: Path) -> None:
        self.cache_dir = data_dir / "lyrics"
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self._tracks: dict[str, dict[str, Any]] = {}
        self._tasks: dict[str, asyncio.Task[None]] = {}
        self._errors: dict[str, str] = {}

    def register(self, track: dict[str, Any] | None, duration_ms: int) -> None:
        if not track:
            return
        track_id = str(track.get("id") or "")
        if not track_id.isalnum():
            return
        self._tracks[track_id] = {
            "track_name": str(track.get("title") or ""),
            "artist_name": str(track.get("artist") or ""),
            "album_name": str(track.get("album") or ""),
            "duration": max(1, round(duration_ms / 1000)),
        }
        self.prefetch(track_id)

    def prefetch(self, track_id: str) -> None:
        if self._cache_file(track_id).exists() or track_id in self._tasks:
            return
        if track_id not in self._tracks:
            return
        task = asyncio.create_task(self._download(track_id))
        self._tasks[track_id] = task
        task.add_done_callback(lambda _: self._tasks.pop(track_id, None))

    def window(self, track_id: str, progress_ms: int) -> dict[str, Any]:
        cached = self._load(track_id)
        if cached is None:
            self.prefetch(track_id)
            return {
                "available": False,
                "loading": track_id in self._tasks,
                "message": self._errors.get(track_id, "BUSCANDO LETRA..."),
                "lines": [],
            }

        if not cached.get("available"):
            return {**cached, "loading": False, "lines": []}
        if cached.get("instrumental"):
            return {
                "available": False,
                "loading": False,
                "instrumental": True,
                "message": "FAIXA INSTRUMENTAL",
                "lines": [],
            }

        lines = cached.get("lines") or []
        if not lines:
            return {
                "available": False,
                "loading": False,
                "message": "LETRA NAO ENCONTRADA",
                "lines": [],
            }

        synced = bool(cached.get("synced"))
        if synced:
            times = [int(line["time_ms"]) for line in lines]
            current = max(0, bisect.bisect_right(times, progress_ms) - 1)
            start = max(0, min(current - 2, len(lines) - 5))
        else:
            current = 0
            start = 0

        selected = lines[start : start + 5]
        return {
            "available": True,
            "loading": False,
            "synced": synced,
            "instrumental": False,
            "current_index": current,
            "current_slot": current - start if synced else 0,
            "lines": selected,
            "message": "" if synced else "LETRA SEM SINCRONIZACAO",
        }

    def full(self, track_id: str) -> dict[str, Any]:
        cached = self._load(track_id)
        if cached is None:
            self.prefetch(track_id)
            return {
                "available": False,
                "loading": track_id in self._tasks,
                "message": self._errors.get(track_id, "BUSCANDO LETRA..."),
                "lines": [],
            }
        return {**cached, "loading": False}

    async def _download(self, track_id: str) -> None:
        metadata = self._tracks[track_id]
        try:
            async with httpx.AsyncClient(timeout=25, follow_redirects=True) as client:
                response = await client.get(
                    LRCLIB_URL,
                    params=metadata,
                    headers={"User-Agent": "CYDify/0.6 (ESP32 music dashboard)"},
                )

            if response.status_code == 404:
                self._save(track_id, {
                    "available": False,
                    "message": "LETRA NAO ENCONTRADA",
                })
                return
            response.raise_for_status()
            payload = response.json()
            instrumental = bool(payload.get("instrumental"))
            synced_text = str(payload.get("syncedLyrics") or "")
            plain_text = str(payload.get("plainLyrics") or "")

            if instrumental:
                result = {"available": True, "instrumental": True, "synced": False, "lines": []}
            elif synced_text:
                result = {
                    "available": True,
                    "instrumental": False,
                    "synced": True,
                    "lines": self._parse_lrc(synced_text),
                }
            elif plain_text:
                result = {
                    "available": True,
                    "instrumental": False,
                    "synced": False,
                    "lines": [
                        {"time_ms": 0, "text": line.strip()}
                        for line in plain_text.splitlines()
                        if line.strip()
                    ],
                }
            else:
                result = {"available": False, "message": "LETRA NAO ENCONTRADA"}
            self._save(track_id, result)
            self._errors.pop(track_id, None)
        except (httpx.HTTPError, ValueError, OSError) as error:
            self._errors[track_id] = "LRCLIB INDISPONIVEL"
            print(f"[CYDify] Falha LRCLIB para {track_id}: {error}")

    @staticmethod
    def _parse_lrc(content: str) -> list[dict[str, Any]]:
        parsed: list[dict[str, Any]] = []
        for raw_line in content.splitlines():
            matches = list(TIMESTAMP.finditer(raw_line))
            text = TIMESTAMP.sub("", raw_line).strip()
            if not matches or not text:
                continue
            for match in matches:
                minutes = int(match.group(1))
                seconds = int(match.group(2))
                fraction = match.group(3) or "0"
                milliseconds = int(fraction.ljust(3, "0")[:3])
                parsed.append({
                    "time_ms": (minutes * 60 + seconds) * 1000 + milliseconds,
                    "text": text,
                })
        parsed.sort(key=lambda line: line["time_ms"])
        return parsed

    def _cache_file(self, track_id: str) -> Path:
        return self.cache_dir / f"{track_id}.json"

    def _load(self, track_id: str) -> dict[str, Any] | None:
        path = self._cache_file(track_id)
        if not path.exists():
            return None
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None

    def _save(self, track_id: str, value: dict[str, Any]) -> None:
        path = self._cache_file(track_id)
        temporary = path.with_suffix(".tmp")
        temporary.write_text(
            json.dumps(value, ensure_ascii=False, separators=(",", ":")),
            encoding="utf-8",
        )
        temporary.replace(path)
