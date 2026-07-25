@echo off
setlocal
cd /d "%~dp0"

echo [CYDify] Criando ambiente virtual...
python -m venv .venv
if errorlevel 1 goto :error

echo [CYDify] Atualizando pip...
.venv\Scripts\python.exe -m pip install --upgrade pip
if errorlevel 1 goto :error

echo [CYDify] Instalando dependencias...
.venv\Scripts\python.exe -m pip install -r requirements.txt
if errorlevel 1 goto :error

if not exist .env copy .env.example .env >nul

echo.
echo Instalacao concluida.
echo Agora edite o arquivo .env e depois execute start_server.bat.
pause
exit /b 0

:error
echo.
echo A instalacao falhou. Copie toda a mensagem desta janela.
pause
exit /b 1
