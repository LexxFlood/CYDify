from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from app.statistics import StatisticsService


def player_state(
    track_id: str,
    progress_ms: int,
    *,
    playing: bool = True,
    title: str = "Faixa",
    artist: str = "Artista",
    album: str = "Album",
) -> dict[str, object]:
    return {
        "playing": playing,
        "progress_ms": progress_ms,
        "track": {
            "id": track_id,
            "title": title,
            "artist": artist,
            "album": album,
        },
    }


class StatisticsServiceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.database = Path(self.temporary.name) / "statistics.db"
        self.service = StatisticsService(self.database)

    def tearDown(self) -> None:
        self.service.close()
        self.temporary.cleanup()

    def test_counts_time_plays_and_rankings(self) -> None:
        states = [
            player_state("a", 1000, title="Primeira", artist="Alpha"),
            player_state("a", 3000, title="Primeira", artist="Alpha"),
            player_state("b", 500, title="Segunda", artist="Beta"),
            player_state("b", 3500, title="Segunda", artist="Beta"),
        ]
        with patch(
            "app.statistics.time.monotonic",
            side_effect=[10.0, 12.0, 14.0, 17.0],
        ):
            snapshot = {}
            for state in states:
                snapshot = self.service.observe(state)

        self.assertEqual(snapshot["today_seconds"], 5)
        self.assertEqual(snapshot["week_seconds"], 5)
        self.assertEqual(snapshot["tracks_today"], 2)
        self.assertEqual(snapshot["artists_today"], 2)
        self.assertEqual(snapshot["albums_today"], 1)
        self.assertEqual(snapshot["top_artist"], "Beta")
        self.assertEqual(snapshot["top_track"], "Segunda")

    def test_pause_does_not_add_time_or_duplicate_play(self) -> None:
        states = [
            player_state("a", 1000),
            player_state("a", 2000, playing=False),
            player_state("a", 4000),
            player_state("a", 6000),
        ]
        with patch(
            "app.statistics.time.monotonic",
            side_effect=[10.0, 11.0, 13.0, 15.0],
        ):
            snapshot = {}
            for state in states:
                snapshot = self.service.observe(state)

        self.assertEqual(snapshot["today_seconds"], 2)
        self.assertEqual(snapshot["tracks_today"], 1)

    def test_data_survives_server_restart(self) -> None:
        with patch(
            "app.statistics.time.monotonic",
            side_effect=[10.0, 12.0],
        ):
            self.service.observe(player_state("a", 1000))
            self.service.observe(player_state("a", 3000))

        self.service.close()
        reopened = StatisticsService(self.database)
        try:
            snapshot = reopened.snapshot()
            self.assertEqual(snapshot["today_seconds"], 2)
            self.assertEqual(snapshot["tracks_today"], 1)
        finally:
            reopened.close()

        # Evita segundo close no tearDown.
        self.service = StatisticsService(self.database)


if __name__ == "__main__":
    unittest.main()
