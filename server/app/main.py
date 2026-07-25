from __future__ import annotations

import socket
import ipaddress
from datetime import UTC, datetime
from typing import Any

import httpx
from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import HTMLResponse, RedirectResponse, Response
from pydantic import BaseModel, Field

from .config import DATA_DIR, settings
from .covers import (
    BACKGROUND_HEIGHT,
    BACKGROUND_WIDTH,
    COVER_HEIGHT,
    COVER_WIDTH,
    CoverNotFound,
    CoverService,
)
from .lyrics import LyricsService
from .spotify import (
    SpotifyApiError,
    SpotifyClient,
    SpotifyNotAuthenticated,
    SpotifyNotConfigured,
)
from .statistics import StatisticsService
from .token_store import TokenStore


APP_VERSION = "0.6.0"
token_store = TokenStore(DATA_DIR / "spotify_token.json")
spotify = SpotifyClient(settings, token_store)
cover_service = CoverService(DATA_DIR)
lyrics_service = LyricsService(DATA_DIR)
statistics_service = StatisticsService(DATA_DIR / "statistics.db")

app = FastAPI(
    title="CYDify Server",
    version=APP_VERSION,
    description="Backend local do CYDify para Spotify e ESP32.",
)


class LikeRequest(BaseModel):
    uri: str = Field(pattern=r"^spotify:(track|episode):[A-Za-z0-9]+$")
    liked: bool


def local_ip() -> str:
    # Prefere enderecos LAN privados e evita VPNs como Radmin (faixa 26.x).
    try:
        addresses = {
            info[4][0]
            for info in socket.getaddrinfo(
                socket.gethostname(), None, family=socket.AF_INET
            )
        }
        private_addresses = [
            address
            for address in addresses
            if ipaddress.ip_address(address).is_private
            and not ipaddress.ip_address(address).is_loopback
        ]
        private_addresses.sort(
            key=lambda address: (
                0 if address.startswith("192.168.") else 1,
                0 if address.startswith("10.") else 1,
                address,
            )
        )
        if private_addresses:
            return private_addresses[0]
    except (OSError, ValueError):
        pass

    connection = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        connection.connect(("8.8.8.8", 80))
        return str(connection.getsockname()[0])
    except OSError:
        return "127.0.0.1"
    finally:
        connection.close()


def page(title: str, content: str) -> str:
    return f"""<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title} | CYDify</title>
<style>
*{{box-sizing:border-box}}body{{margin:0;background:#090b0a;color:#fff;font-family:Arial,sans-serif}}
main{{max-width:680px;margin:0 auto;padding:36px 20px}}h1{{color:#1ed760;margin-bottom:4px}}
.card{{background:#171a18;border:1px solid #2a2e2b;border-radius:18px;padding:20px;margin-top:22px}}
.ok{{color:#1ed760}}.warn{{color:#f7b84b}}p,li{{color:#c5c9c6;line-height:1.55}}
a.button{{display:inline-block;background:#1ed760;color:#071009;padding:13px 20px;border-radius:24px;text-decoration:none;font-weight:bold}}
code{{color:#b9f6cf;background:#222622;padding:3px 6px;border-radius:5px}}
</style></head><body><main><h1>CYDify Server</h1><small>v{APP_VERSION}</small>{content}</main></body></html>"""


def spotify_http_error(error: SpotifyApiError) -> HTTPException:
    headers = {"Retry-After": error.retry_after} if error.retry_after else None
    return HTTPException(
        status_code=error.status_code,
        detail=error.detail,
        headers=headers,
    )


@app.get("/", response_class=HTMLResponse)
async def home() -> str:
    configured = settings.spotify_configured
    authenticated = spotify.authenticated
    ip = local_ip()

    if not configured:
        status = "<p class='warn'>Spotify ainda não configurado.</p>"
        action = "<p>Copie <code>.env.example</code> para <code>.env</code> e preencha Client ID e Client Secret.</p>"
    elif not authenticated:
        status = "<p class='warn'>Aplicativo configurado, aguardando login.</p>"
        action = "<p><a class='button' href='/auth/login'>Conectar Spotify</a></p>"
    else:
        status = "<p class='ok'>Spotify conectado.</p>"
        action = "<p><a class='button' href='/api/player'>Ver player JSON</a></p>"

    content = f"""
<div class="card"><h2>Estado</h2>{status}{action}</div>
<div class="card"><h2>Endereços</h2>
<p>Neste computador: <code>http://127.0.0.1:{settings.port}</code></p>
<p>Para o CYD: <code>http://{ip}:{settings.port}</code></p>
<p>Documentação da API: <a href="/docs">/docs</a></p></div>"""
    return page("Início", content)


@app.get("/health")
async def health() -> dict[str, object]:
    return {
        "ok": True,
        "service": "cydify-server",
        "version": APP_VERSION,
        "spotify_configured": settings.spotify_configured,
        "spotify_authenticated": spotify.authenticated,
        "server_time": datetime.now(UTC).isoformat(),
    }


@app.get("/auth/login")
async def auth_login() -> RedirectResponse:
    try:
        return RedirectResponse(spotify.create_authorization_url(), status_code=302)
    except SpotifyNotConfigured as error:
        raise HTTPException(status_code=503, detail=str(error)) from error


@app.get("/callback", response_class=HTMLResponse)
async def callback(
    code: str | None = Query(default=None),
    state: str | None = Query(default=None),
    error: str | None = Query(default=None),
) -> str:
    if error:
        raise HTTPException(status_code=400, detail=f"Spotify recusou a autorização: {error}")
    if not code or not state or not spotify.validate_state(state):
        raise HTTPException(status_code=400, detail="Callback OAuth inválido ou expirado.")

    try:
        await spotify.exchange_code(code)
    except SpotifyApiError as spotify_error:
        raise spotify_http_error(spotify_error) from spotify_error

    content = """
<div class="card"><h2 class="ok">Spotify conectado!</h2>
<p>O token foi salvo no computador e será renovado automaticamente.</p>
<p>Agora coloque uma música para tocar e abra o endpoint do player.</p>
<p><a class="button" href="/api/player">Testar player</a></p></div>"""
    return page("Spotify conectado", content)


@app.get("/api/player")
async def player() -> dict[str, object]:
    try:
        state = await spotify.player_state()
    except SpotifyNotConfigured as error:
        raise HTTPException(status_code=503, detail=str(error)) from error
    except SpotifyNotAuthenticated as error:
        raise HTTPException(status_code=401, detail=str(error)) from error
    except SpotifyApiError as error:
        raise spotify_http_error(error) from error

    track = state.get("track") if isinstance(state, dict) else None
    if isinstance(track, dict):
        cover_service.register(track.get("id"), track.get("cover_url"))
        lyrics_service.register(track, int(state.get("duration_ms") or 0))

    return {
        "ok": True,
        "server_time": datetime.now(UTC).isoformat(),
        "player": state,
        "statistics": statistics_service.observe(state),
    }


@app.get("/api/statistics")
async def statistics() -> dict[str, object]:
    return {
        "ok": True,
        "statistics": statistics_service.snapshot(),
    }


@app.get("/api/cover/{track_id}.rgb565")
async def album_cover(track_id: str) -> Response:
    try:
        content = await cover_service.rgb565(track_id)
    except CoverNotFound as error:
        raise HTTPException(status_code=404, detail=str(error)) from error
    except (httpx.HTTPError, OSError, ValueError) as error:
        raise HTTPException(status_code=502, detail=f"Falha ao preparar capa: {error}") from error

    return Response(
        content=content,
        media_type="application/octet-stream",
        headers={
            "X-CYDify-Format": "RGB565-LE",
            "X-CYDify-Width": str(COVER_WIDTH),
            "X-CYDify-Height": str(COVER_HEIGHT),
            "X-CYDify-Dominant": f"{cover_service.dominant_color(content):06X}",
            "Cache-Control": "public, max-age=31536000, immutable",
        },
    )


@app.get("/api/background/{track_id}.rgb565")
async def album_background(track_id: str) -> Response:
    try:
        content = await cover_service.background_rgb565(track_id)
    except CoverNotFound as error:
        raise HTTPException(status_code=404, detail=str(error)) from error
    except (httpx.HTTPError, OSError, ValueError) as error:
        raise HTTPException(
            status_code=502,
            detail=f"Falha ao preparar fundo: {error}",
        ) from error

    return Response(
        content=content,
        media_type="application/octet-stream",
        headers={
            "X-CYDify-Format": "RGB565-LE",
            "X-CYDify-Width": str(BACKGROUND_WIDTH),
            "X-CYDify-Height": str(BACKGROUND_HEIGHT),
            "Cache-Control": "public, max-age=31536000, immutable",
        },
    )


@app.get("/api/lyrics/{track_id}")
async def lyrics_window(track_id: str, progress_ms: int = Query(ge=0)) -> dict[str, Any]:
    if not track_id.isalnum():
        raise HTTPException(status_code=400, detail="Identificador de faixa invalido.")
    return lyrics_service.window(track_id, progress_ms)


@app.get("/api/lyrics/{track_id}/full")
async def lyrics_full(track_id: str) -> dict[str, Any]:
    if not track_id.isalnum():
        raise HTTPException(status_code=400, detail="Identificador de faixa invalido.")
    return lyrics_service.full(track_id)


@app.post("/api/player/{action}")
async def player_action(action: str) -> dict[str, object]:
    if action not in {"play", "pause", "next", "previous"}:
        raise HTTPException(status_code=404, detail="Ação desconhecida.")
    try:
        await spotify.playback_action(action)
    except SpotifyNotAuthenticated as error:
        raise HTTPException(status_code=401, detail=str(error)) from error
    except SpotifyApiError as error:
        raise spotify_http_error(error) from error
    return {"ok": True, "action": action}


@app.post("/api/player/liked/set")
async def set_liked(request: LikeRequest) -> dict[str, object]:
    try:
        applied = await spotify.set_liked(request.uri, request.liked)
    except SpotifyNotAuthenticated as error:
        raise HTTPException(status_code=401, detail=str(error)) from error
    except SpotifyApiError as error:
        raise spotify_http_error(error) from error
    return {
        "ok": True,
        "uri": request.uri,
        "liked": request.liked,
        "queued": not applied,
    }


@app.post("/auth/disconnect")
async def disconnect() -> dict[str, bool]:
    token_store.clear()
    return {"ok": True}
