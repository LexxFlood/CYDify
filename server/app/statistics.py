from __future__ import annotations

import sqlite3
import threading
import time
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Any


class StatisticsService:
    """Registra tempo ouvido sem fazer chamadas adicionais ao Spotify."""

    def __init__(self, database_path: Path) -> None:
        database_path.parent.mkdir(parents=True, exist_ok=True)
        self._connection = sqlite3.connect(
            database_path,
            check_same_thread=False,
        )
        self._connection.row_factory = sqlite3.Row
        self._lock = threading.RLock()
        self._last_track_id = ""
        self._last_day = ""
        self._last_progress_ms = 0
        self._last_observed_at = 0.0
        self._last_playing = False
        self._create_schema()

    def _create_schema(self) -> None:
        with self._lock:
            self._connection.execute(
                """
                CREATE TABLE IF NOT EXISTS daily_tracks (
                    day TEXT NOT NULL,
                    track_id TEXT NOT NULL,
                    title TEXT NOT NULL,
                    artist TEXT NOT NULL,
                    album TEXT NOT NULL,
                    listened_ms INTEGER NOT NULL DEFAULT 0,
                    plays INTEGER NOT NULL DEFAULT 0,
                    PRIMARY KEY (day, track_id)
                )
                """
            )
            self._connection.execute(
                "CREATE INDEX IF NOT EXISTS idx_daily_tracks_day "
                "ON daily_tracks(day)"
            )
            self._connection.commit()

    @staticmethod
    def _today() -> date:
        return datetime.now().astimezone().date()

    def _upsert_track(
        self,
        day: str,
        track: dict[str, Any],
        *,
        played: bool,
    ) -> None:
        self._connection.execute(
            """
            INSERT INTO daily_tracks (
                day, track_id, title, artist, album, listened_ms, plays
            ) VALUES (?, ?, ?, ?, ?, 0, ?)
            ON CONFLICT(day, track_id) DO UPDATE SET
                title = excluded.title,
                artist = excluded.artist,
                album = excluded.album,
                plays = daily_tracks.plays + excluded.plays
            """,
            (
                day,
                str(track.get("id") or ""),
                str(track.get("title") or "Faixa desconhecida"),
                str(track.get("artist") or "Artista desconhecido"),
                str(track.get("album") or "Album desconhecido"),
                1 if played else 0,
            ),
        )

    def _add_listening_time(
        self,
        day: str,
        track_id: str,
        listened_ms: int,
    ) -> None:
        self._connection.execute(
            """
            UPDATE daily_tracks
            SET listened_ms = listened_ms + ?
            WHERE day = ? AND track_id = ?
            """,
            (listened_ms, day, track_id),
        )

    def observe(self, state: dict[str, Any]) -> dict[str, Any]:
        now = time.monotonic()
        today = self._today().isoformat()
        track = state.get("track") if isinstance(state, dict) else None
        playing = bool(state.get("playing")) if isinstance(state, dict) else False

        if not isinstance(track, dict) or not track.get("id"):
            with self._lock:
                self._last_track_id = ""
                self._last_day = today
                self._last_progress_ms = 0
                self._last_observed_at = now
                self._last_playing = False
                return self._snapshot_locked(self._today())

        track_id = str(track["id"])
        progress_ms = max(0, int(state.get("progress_ms") or 0))

        with self._lock:
            same_session = (
                track_id == self._last_track_id
                and today == self._last_day
                and self._last_playing
                and playing
            )
            restarted = (
                track_id == self._last_track_id
                and progress_ms + 5000 < self._last_progress_ms
            )
            new_play = playing and (
                track_id != self._last_track_id
                or today != self._last_day
                or restarted
            )

            self._upsert_track(today, track, played=new_play)

            if same_session and not restarted:
                progress_delta = progress_ms - self._last_progress_ms
                elapsed_ms = max(
                    0,
                    int((now - self._last_observed_at) * 1000),
                )
                # Limita correções bruscas do Spotify e longos períodos sem
                # observação. O CYD consulta normalmente a cada dois segundos.
                if 0 < progress_delta <= 30000 and elapsed_ms <= 30000:
                    accounted_ms = min(progress_delta, elapsed_ms + 1500)
                    self._add_listening_time(
                        today,
                        track_id,
                        accounted_ms,
                    )

            self._connection.commit()
            self._last_track_id = track_id
            self._last_day = today
            self._last_progress_ms = progress_ms
            self._last_observed_at = now
            self._last_playing = playing
            return self._snapshot_locked(self._today())

    def snapshot(self) -> dict[str, Any]:
        with self._lock:
            return self._snapshot_locked(self._today())

    def _snapshot_locked(self, today: date) -> dict[str, Any]:
        today_text = today.isoformat()
        week_start = (today - timedelta(days=today.weekday())).isoformat()

        totals = self._connection.execute(
            """
            SELECT
                COALESCE(SUM(listened_ms), 0) AS listened_ms,
                COALESCE(SUM(plays), 0) AS plays,
                COUNT(DISTINCT artist) AS artists,
                COUNT(DISTINCT album) AS albums
            FROM daily_tracks
            WHERE day = ?
            """,
            (today_text,),
        ).fetchone()

        week = self._connection.execute(
            """
            SELECT COALESCE(SUM(listened_ms), 0) AS listened_ms
            FROM daily_tracks
            WHERE day >= ? AND day <= ?
            """,
            (week_start, today_text),
        ).fetchone()

        top_artist = self._connection.execute(
            """
            SELECT artist, SUM(listened_ms) AS total_ms, SUM(plays) AS plays
            FROM daily_tracks
            WHERE day = ?
            GROUP BY artist
            ORDER BY total_ms DESC, plays DESC, artist ASC
            LIMIT 1
            """,
            (today_text,),
        ).fetchone()

        top_track = self._connection.execute(
            """
            SELECT title, listened_ms, plays
            FROM daily_tracks
            WHERE day = ?
            ORDER BY listened_ms DESC, plays DESC, title ASC
            LIMIT 1
            """,
            (today_text,),
        ).fetchone()

        return {
            "day": today_text,
            "today_seconds": int((totals["listened_ms"] or 0) // 1000),
            "week_seconds": int((week["listened_ms"] or 0) // 1000),
            "tracks_today": int(totals["plays"] or 0),
            "artists_today": int(totals["artists"] or 0),
            "albums_today": int(totals["albums"] or 0),
            "top_artist": str(top_artist["artist"]) if top_artist else "",
            "top_track": str(top_track["title"]) if top_track else "",
        }

    def close(self) -> None:
        with self._lock:
            self._connection.close()
