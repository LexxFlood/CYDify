from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from dotenv import load_dotenv


SERVER_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = SERVER_ROOT / "data"
ENV_FILE = SERVER_ROOT / ".env"

load_dotenv(ENV_FILE)


@dataclass(frozen=True, slots=True)
class Settings:
    spotify_client_id: str
    spotify_client_secret: str
    spotify_redirect_uri: str
    host: str
    port: int
    log_level: str

    @property
    def spotify_configured(self) -> bool:
        invalid_values = {"", "cole_seu_client_id_aqui", "cole_seu_client_secret_aqui"}
        return (
            self.spotify_client_id not in invalid_values
            and self.spotify_client_secret not in invalid_values
        )


def load_settings() -> Settings:
    try:
        port = int(os.getenv("CYDIFY_SERVER_PORT", "8000"))
    except ValueError:
        port = 8000

    return Settings(
        spotify_client_id=os.getenv("SPOTIFY_CLIENT_ID", "").strip(),
        spotify_client_secret=os.getenv("SPOTIFY_CLIENT_SECRET", "").strip(),
        spotify_redirect_uri=os.getenv(
            "SPOTIFY_REDIRECT_URI", "http://127.0.0.1:8000/callback"
        ).strip(),
        host=os.getenv("CYDIFY_SERVER_HOST", "0.0.0.0").strip(),
        port=port,
        log_level=os.getenv("CYDIFY_LOG_LEVEL", "info").strip().lower(),
    )


settings = load_settings()
DATA_DIR.mkdir(parents=True, exist_ok=True)
