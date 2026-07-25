from __future__ import annotations

import json
import os
from pathlib import Path
from threading import Lock
from typing import Any


class TokenStore:
    def __init__(self, path: Path) -> None:
        self._path = path
        self._lock = Lock()

    def load(self) -> dict[str, Any] | None:
        with self._lock:
            if not self._path.exists():
                return None
            try:
                data = json.loads(self._path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                return None
            return data if isinstance(data, dict) else None

    def save(self, token: dict[str, Any]) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self._path.with_suffix(".tmp")
        payload = json.dumps(token, ensure_ascii=False, indent=2)

        with self._lock:
            temporary.write_text(payload, encoding="utf-8")
            os.replace(temporary, self._path)

    def clear(self) -> None:
        with self._lock:
            self._path.unlink(missing_ok=True)
