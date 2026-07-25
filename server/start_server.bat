@echo off
setlocal
cd /d "%~dp0"

if not exist .venv\Scripts\python.exe (
  echo O servidor ainda nao foi instalado. Execute setup_server.bat primeiro.
  pause
  exit /b 1
)

if not exist .env (
  echo Arquivo .env ausente. Copie .env.example para .env e preencha o Spotify.
  pause
  exit /b 1
)

echo CYDify Server iniciando em http://127.0.0.1:8000
echo Mantenha esta janela aberta.
.venv\Scripts\python.exe run.py
pause
