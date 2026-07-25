from __future__ import annotations

import asyncio
import base64
from copy import deepcopy
import secrets
import time
from typing import Any
from urllib.parse import urlencode

import httpx

from .config import Settings
from .token_store import TokenStore


ACCOUNTS_URL = "https://accounts.spotify.com"
API_URL = "https://api.spotify.com/v1"
SCOPES = (
    "user-read-playback-state",
    "user-read-currently-playing",
    "user-modify-playback-state",
    "user-library-read",
    "user-library-modify",
)

PLAYER_CACHE_SECONDS = 5.0
LIKED_CACHE_SECONDS = 120.0


class SpotifyNotConfigured(RuntimeError):
    pass


class SpotifyNotAuthenticated(RuntimeError):
    pass


class SpotifyApiError(RuntimeError):
    def __init__(self, status_code: int, detail: str, retry_after: str | None = None):
        super().__init__(detail)
        self.status_code = status_code
        self.detail = detail
        self.retry_after = retry_after


class SpotifyClient:
    def __init__(self, settings: Settings, token_store: TokenStore) -> None:
        self.settings = settings
        self.token_store = token_store
        self._oauth_states: dict[str, float] = {}
        self._player_cache: dict[str, Any] | None = None
        self._player_cache_at = 0.0
        self._player_retry_at = 0.0
        self._liked_retry_at = 0.0
        self._liked_cache: dict[str, tuple[bool, float]] = {}
        self._pending_liked: dict[str, bool] = {}
        self._liked_retry_task: asyncio.Task[None] | None = None

    @property
    def authenticated(self) -> bool:
        token = self.token_store.load()
        return bool(token and token.get("refresh_token"))

    def _require_config(self) -> None:
        if not self.settings.spotify_configured:
            raise SpotifyNotConfigured(
                "Preencha SPOTIFY_CLIENT_ID e SPOTIFY_CLIENT_SECRET no arquivo .env."
            )

    def create_authorization_url(self) -> str:
        self._require_config()
        now = time.time()
        self._oauth_states = {
            state: expires
            for state, expires in self._oauth_states.items()
            if expires > now
        }
        state = secrets.token_urlsafe(32)
        self._oauth_states[state] = now + 600
        query = urlencode(
            {
                "client_id": self.settings.spotify_client_id,
                "response_type": "code",
                "redirect_uri": self.settings.spotify_redirect_uri,
                "state": state,
                "scope": " ".join(SCOPES),
                "show_dialog": "true",
            }
        )
        return f"{ACCOUNTS_URL}/authorize?{query}"

    def validate_state(self, state: str) -> bool:
        expires = self._oauth_states.pop(state, None)
        return expires is not None and expires > time.time()

    def _basic_authorization(self) -> str:
        credentials = (
            f"{self.settings.spotify_client_id}:{self.settings.spotify_client_secret}"
        ).encode("utf-8")
        return "Basic " + base64.b64encode(credentials).decode("ascii")

    async def exchange_code(self, code: str) -> None:
        self._require_config()
        async with httpx.AsyncClient(timeout=20) as client:
            response = await client.post(
                f"{ACCOUNTS_URL}/api/token",
                headers={"Authorization": self._basic_authorization()},
                data={
                    "grant_type": "authorization_code",
                    "code": code,
                    "redirect_uri": self.settings.spotify_redirect_uri,
                },
            )
        self._raise_for_response(response)
        self._save_token_response(response.json())

    async def _refresh_access_token(self, refresh_token: str) -> dict[str, Any]:
        async with httpx.AsyncClient(timeout=20) as client:
            response = await client.post(
                f"{ACCOUNTS_URL}/api/token",
                headers={"Authorization": self._basic_authorization()},
                data={
                    "grant_type": "refresh_token",
                    "refresh_token": refresh_token,
                },
            )
        self._raise_for_response(response)
        refreshed = response.json()
        if "refresh_token" not in refreshed:
            refreshed["refresh_token"] = refresh_token
        self._save_token_response(refreshed)
        return self.token_store.load() or {}

    def _save_token_response(self, token: dict[str, Any]) -> None:
        token["expires_at"] = int(time.time()) + int(token.get("expires_in", 3600))
        self.token_store.save(token)

    async def _access_token(self) -> str:
        token = self.token_store.load()
        if not token:
            raise SpotifyNotAuthenticated("Conecte a conta Spotify em /auth/login.")

        if int(token.get("expires_at", 0)) <= int(time.time()) + 60:
            refresh_token = token.get("refresh_token")
            if not refresh_token:
                raise SpotifyNotAuthenticated("Token expirado. Conecte o Spotify novamente.")
            token = await self._refresh_access_token(str(refresh_token))

        access_token = token.get("access_token")
        if not access_token:
            raise SpotifyNotAuthenticated("Access token ausente.")
        return str(access_token)

    async def request(
        self,
        method: str,
        path: str,
        *,
        params: dict[str, Any] | None = None,
        json: dict[str, Any] | None = None,
    ) -> httpx.Response:
        token = await self._access_token()
        async with httpx.AsyncClient(timeout=20) as client:
            response = await client.request(
                method,
                f"{API_URL}{path}",
                headers={"Authorization": f"Bearer {token}"},
                params=params,
                json=json,
            )

        if response.status_code == 401:
            stored = self.token_store.load() or {}
            refresh_token = stored.get("refresh_token")
            if not refresh_token:
                raise SpotifyNotAuthenticated("Sessao expirada.")
            refreshed = await self._refresh_access_token(str(refresh_token))
            token = str(refreshed.get("access_token", ""))
            async with httpx.AsyncClient(timeout=20) as client:
                response = await client.request(
                    method,
                    f"{API_URL}{path}",
                    headers={"Authorization": f"Bearer {token}"},
                    params=params,
                    json=json,
                )

        self._raise_for_response(response)
        return response

    @staticmethod
    def _raise_for_response(response: httpx.Response) -> None:
        if response.is_success:
            return
        try:
            payload = response.json()
            error = payload.get("error", payload)
            detail = error.get("message", str(error)) if isinstance(error, dict) else str(error)
        except (ValueError, AttributeError):
            detail = response.text or f"Spotify retornou HTTP {response.status_code}."
        raise SpotifyApiError(
            response.status_code,
            detail,
            response.headers.get("Retry-After"),
        )

    @staticmethod
    def _retry_seconds(retry_after: str | None) -> float:
        try:
            return max(5.0, float(retry_after or 0))
        except ValueError:
            return 5.0

    def _cached_player_state(self, now: float) -> dict[str, Any] | None:
        if self._player_cache is None:
            return None

        state = deepcopy(self._player_cache)
        if now < self._player_retry_at:
            state["rate_limited"] = True
            state["retry_after_seconds"] = max(
                1, int(self._player_retry_at - now + 0.999)
            )
        else:
            state.pop("rate_limited", None)
            state.pop("retry_after_seconds", None)
        if state.get("playing"):
            progress = int(state.get("progress_ms") or 0)
            duration = int(state.get("duration_ms") or 0)
            progress += max(0, int((now - self._player_cache_at) * 1000))
            state["progress_ms"] = min(progress, duration) if duration else progress
        return state

    def _save_player_cache(self, state: dict[str, Any], now: float) -> dict[str, Any]:
        self._player_cache = deepcopy(state)
        self._player_cache_at = now
        return deepcopy(state)

    async def player_state(self) -> dict[str, Any]:
        now = time.monotonic()
        cache_is_fresh = (
            self._player_cache is not None
            and now - self._player_cache_at < PLAYER_CACHE_SECONDS
        )
        if cache_is_fresh or now < self._player_retry_at:
            cached = self._cached_player_state(now)
            if cached is not None:
                return cached

        try:
            response = await self.request("GET", "/me/player")
        except SpotifyApiError as error:
            if error.status_code == 429:
                retry_seconds = self._retry_seconds(error.retry_after)
                self._player_retry_at = now + retry_seconds
                cached = self._cached_player_state(now)
                if cached is not None:
                    return cached
                return self._save_player_cache(
                    {
                        "available": False,
                        "playing": False,
                        "track": None,
                        "rate_limited": True,
                        "retry_after_seconds": int(retry_seconds),
                    },
                    now,
                )
            raise

        self._player_retry_at = 0.0

        if response.status_code == 204 or not response.content:
            return self._save_player_cache(
                {"available": False, "playing": False, "track": None}, now
            )

        raw = response.json()
        item = raw.get("item") or {}
        item_type = item.get("type", "unknown")
        album = item.get("album") or {}
        artists = [
            artist.get("name", "")
            for artist in item.get("artists", [])
            if artist.get("name")
        ]
        images = album.get("images") or []
        cover_url = images[0].get("url") if images else None
        track_id = item.get("id")
        uri = item.get("uri")
        liked = False

        if uri and item_type == "track":
            cached_liked = self._liked_cache.get(str(track_id)) if track_id else None
            if cached_liked and now - cached_liked[1] < LIKED_CACHE_SECONDS:
                liked = cached_liked[0]
            elif now < self._liked_retry_at:
                liked = cached_liked[0] if cached_liked else False
            else:
                try:
                    contains = await self.request(
                        "GET", "/me/library/contains", params={"uris": uri}
                    )
                    values = contains.json() if contains.content else []
                    liked = bool(values and values[0])
                    self._liked_retry_at = 0.0
                    if track_id:
                        self._liked_cache[str(track_id)] = (liked, now)
                except SpotifyApiError as error:
                    if error.status_code != 429:
                        raise
                    self._liked_retry_at = now + self._retry_seconds(error.retry_after)
                    if cached_liked:
                        liked = cached_liked[0]

        device = raw.get("device") or {}
        state = {
            "available": True,
            "playing": bool(raw.get("is_playing")),
            "progress_ms": raw.get("progress_ms") or 0,
            "duration_ms": item.get("duration_ms") or 0,
            "volume_percent": device.get("volume_percent"),
            "shuffle": bool(raw.get("shuffle_state")),
            "repeat_state": raw.get("repeat_state", "off"),
            "device": {
                "id": device.get("id"),
                "name": device.get("name"),
                "type": device.get("type"),
                "active": bool(device.get("is_active")),
            },
            "track": {
                "id": track_id,
                "uri": uri,
                "type": item_type,
                "title": item.get("name", ""),
                "artist": ", ".join(artists),
                "artists": artists,
                "album": album.get("name", ""),
                "cover_url": cover_url,
                "release_date": album.get("release_date"),
                "track_number": item.get("track_number"),
                "disc_number": item.get("disc_number"),
                "popularity": item.get("popularity"),
                "spotify_url": (item.get("external_urls") or {}).get("spotify"),
                "liked": liked,
            },
        }
        return self._save_player_cache(state, now)

    async def playback_action(self, action: str) -> None:
        routes = {
            "play": ("PUT", "/me/player/play"),
            "pause": ("PUT", "/me/player/pause"),
            "next": ("POST", "/me/player/next"),
            "previous": ("POST", "/me/player/previous"),
        }
        if action not in routes:
            raise ValueError("Acao de player invalida.")
        method, path = routes[action]
        await self.request(method, path)
        self._player_cache_at = 0.0

    def _remember_liked(self, uri: str, liked: bool) -> None:
        track_id = uri.rsplit(":", 1)[-1]
        self._liked_cache[track_id] = (liked, time.monotonic())
        if self._player_cache:
            track = self._player_cache.get("track")
            if isinstance(track, dict) and track.get("id") == track_id:
                track["liked"] = liked

    def _schedule_liked_retry(self) -> None:
        if self._liked_retry_task is None or self._liked_retry_task.done():
            self._liked_retry_task = asyncio.create_task(self._flush_liked_queue())

    async def _flush_liked_queue(self) -> None:
        try:
            while self._pending_liked:
                delay = max(0.0, self._liked_retry_at - time.monotonic())
                if delay:
                    await asyncio.sleep(delay)

                uri, liked = next(iter(self._pending_liked.items()))
                method = "PUT" if liked else "DELETE"
                try:
                    await self.request(method, "/me/library", params={"uris": uri})
                except SpotifyApiError as error:
                    if error.status_code == 429:
                        self._liked_retry_at = (
                            time.monotonic() + self._retry_seconds(error.retry_after)
                        )
                        continue
                    if self._pending_liked.get(uri) == liked:
                        self._pending_liked.pop(uri, None)
                    self._liked_cache.pop(uri.rsplit(":", 1)[-1], None)
                    self._player_cache_at = 0.0
                    continue
                except Exception:
                    self._liked_retry_at = time.monotonic() + 10.0
                    continue

                self._liked_retry_at = 0.0
                if self._pending_liked.get(uri) == liked:
                    self._pending_liked.pop(uri, None)
                    self._remember_liked(uri, liked)
        finally:
            self._liked_retry_task = None

    async def set_liked(self, uri: str, liked: bool) -> bool:
        track_id = uri.rsplit(":", 1)[-1]
        cached = self._liked_cache.get(track_id)
        if cached and cached[0] == liked and uri not in self._pending_liked:
            self._remember_liked(uri, liked)
            return True

        self._pending_liked[uri] = liked
        self._remember_liked(uri, liked)
        self._schedule_liked_retry()
        return False
