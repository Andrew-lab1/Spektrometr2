@echo off
chcp 65001 >nul
title Spektrometr Windows Installer

:: ===================================================================
:: SPEKTROMETR - INSTALLER FOR WINDOWS
:: ===================================================================
:: Kompletny skrypt instalacyjny dla systemów Windows
:: Zawiera: instalację Pythona, pakietów, SDK PixeLink, konfigurację
:: ===================================================================

:: Zmienne
set SPEKTROMETR_DIR=C:\Spektrometr
set DESKTOP_SHORTCUT="%USERPROFILE%\Desktop\Spektrometr.lnk"
set PYTHON_VERSION=3.13

echo.
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                    SPEKTROMETR WINDOWS                      ║
echo ║                      Installer v1.0                         ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

goto MENU

:MENU
cls
echo.
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                    SPEKTROMETR WINDOWS                      ║
echo ║                      Installer v1.0                         ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.
echo Wybierz opcję:
echo.
echo 1️⃣  Zainstaluj Spektrometr
echo 2️⃣  Odinstaluj Spektrometr
echo 3️⃣  Konfiguruj PixeLink SDK
echo 4️⃣  Test instalacji
echo 5️⃣  Wyjście
echo.
set /p "choice=Wybór (1-5): "

if "%choice%"=="1" goto INSTALL
if "%choice%"=="2" goto UNINSTALL
if "%choice%"=="3" goto CONFIGURE_SDK
if "%choice%"=="4" goto TEST_INSTALL
if "%choice%"=="5" goto EXIT
echo ❌ Nieprawidłowy wybór!
timeout /t 2 >nul
goto MENU

:INSTALL
cls
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                      INSTALACJA                             ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

:: Sprawdzenie uprawnień administratora
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ⚠️  Wymagane uprawnienia administratora!
    echo 🔄 Uruchamianie jako administrator...
    powershell -Command "Start-Process '%~f0' -Verb RunAs -ArgumentList '%*'"
    exit /b
)

echo ✅ Uprawnienia administratora potwierdzone
echo.

:: Sprawdzenie Pythona
echo 🐍 Sprawdzanie Pythona...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ Python nie znaleziony!
    echo 📥 Pobieranie Python %PYTHON_VERSION%...
    
    :: Pobieranie Pythona
    powershell -Command "& {
        $url = 'https://www.python.org/ftp/python/3.13.0/python-3.13.0-amd64.exe'
        $output = '$env:TEMP\python-installer.exe'
        Write-Host '📥 Pobieranie z: $url'
        try {
            Invoke-WebRequest -Uri $url -OutFile $output -UseBasicParsing
            Write-Host '✅ Pobrano Python installer'
            Write-Host '🚀 Uruchamianie instalatora...'
            Start-Process -FilePath $output -ArgumentList '/quiet', 'InstallAllUsers=1', 'PrependPath=1', 'Include_test=0' -Wait
            Write-Host '✅ Python zainstalowany'
        } catch {
            Write-Host '❌ Błąd pobierania: $_'
            Write-Host '🌐 Otwieranie strony Python.org...'
            Start-Process 'https://www.python.org/downloads/'
            Read-Host 'Zainstaluj Python ręcznie i naciśnij Enter...'
        }
    }"
    
    :: Odświeżenie PATH
    call refreshenv >nul 2>&1 || echo 🔄 PATH może wymagać odświeżenia
    
    :: Ponowne sprawdzenie
    python --version >nul 2>&1
    if %errorlevel% neq 0 (
        echo ❌ Python nadal niedostępny. Zainstaluj ręcznie i uruchom ponownie.
        pause
        goto MENU
    )
)

for /f "tokens=2 delims= " %%i in ('python --version 2^>^&1') do set PYTHON_VER=%%i
echo ✅ Python %PYTHON_VER% dostępny
echo.

:: Tworzenie katalogu
echo 📁 Tworzenie katalogu %SPEKTROMETR_DIR%...
if not exist "%SPEKTROMETR_DIR%" mkdir "%SPEKTROMETR_DIR%"
cd /d "%SPEKTROMETR_DIR%"

:: Kopiowanie plików
echo 📂 Kopiowanie plików aplikacji...
xcopy "%~dp0..\..\*" "%SPEKTROMETR_DIR%\" /E /I /Y /Q >nul 2>&1
if exist "%~dp0..\..\..\index.py" (
    xcopy "%~dp0..\..\..\*" "%SPEKTROMETR_DIR%\" /E /I /Y /Q >nul 2>&1
) else (
    echo ⚠️  Nie można znaleźć plików źródłowych
)

:: Instalacja pakietów Python
echo 📦 Instalacja pakietów Python...
echo contourpy==1.3.3 > requirements_temp.txt
echo cycler==0.12.1 >> requirements_temp.txt
echo fonttools==4.60.1 >> requirements_temp.txt
echo kiwisolver==1.4.9 >> requirements_temp.txt
echo matplotlib==3.10.7 >> requirements_temp.txt
echo numpy==2.2.6 >> requirements_temp.txt
echo opencv-python==4.12.0.88 >> requirements_temp.txt
echo packaging==25.0 >> requirements_temp.txt
echo pillow==12.0.0 >> requirements_temp.txt
echo pixelinkWrapper==1.4.1 >> requirements_temp.txt
echo pyparsing==3.2.5 >> requirements_temp.txt
echo pyserial==3.5 >> requirements_temp.txt
echo python-dateutil==2.9.0.post0 >> requirements_temp.txt
echo six==1.17.0 >> requirements_temp.txt

python -m pip install --upgrade pip
python -m pip install -r requirements_temp.txt
del requirements_temp.txt

:: PixeLink SDK
echo 🔗 Konfiguracja PixeLink SDK...
if exist "pixelinksdk-for-windows-pc_64-v35" (
    echo ✅ Znaleziono PixeLink SDK
    
    :: Dodanie do PATH systemowego
    set SDK_PATH=%SPEKTROMETR_DIR%\pixelinksdk-for-windows-pc_64-v35\bin
    
    powershell -Command "& {
        $path = [Environment]::GetEnvironmentVariable('PATH', 'Machine')
        if ($path -notlike '*%SDK_PATH%*') {
            $newPath = $path + ';%SDK_PATH%'
            [Environment]::SetEnvironmentVariable('PATH', $newPath, 'Machine')
            Write-Host '✅ PixeLink SDK dodane do PATH'
        } else {
            Write-Host '✅ PixeLink SDK już w PATH'
        }
    }"
) else (
    echo ⚠️  PixeLink SDK nie znalezione
)

:: Skrót na pulpicie
echo 🖥️ Tworzenie skrótu na pulpicie...
powershell -Command "& {
    $WshShell = New-Object -comObject WScript.Shell
    $Shortcut = $WshShell.CreateShortcut('%DESKTOP_SHORTCUT%')
    $Shortcut.TargetPath = 'python'
    $Shortcut.Arguments = '\"%SPEKTROMETR_DIR%\index.py\"'
    $Shortcut.WorkingDirectory = '%SPEKTROMETR_DIR%'
    $Shortcut.IconLocation = '%SPEKTROMETR_DIR%\index.py,0'
    $Shortcut.Description = 'Spektrometr Application'
    $Shortcut.Save()
    Write-Host '✅ Skrót utworzony'
}"

:: Skrypt uruchamiający
echo 📝 Tworzenie skryptu uruchamiającego...
echo @echo off > "%SPEKTROMETR_DIR%\run_spektrometr.bat"
echo cd /d "%%~dp0" >> "%SPEKTROMETR_DIR%\run_spektrometr.bat"
echo python index.py >> "%SPEKTROMETR_DIR%\run_spektrometr.bat"

:: Wpis w menu Start (opcjonalnie)
echo 📋 Dodawanie do menu Start...
set START_MENU_DIR="%PROGRAMDATA%\Microsoft\Windows\Start Menu\Programs\Spektrometr"
if not exist %START_MENU_DIR% mkdir %START_MENU_DIR%
copy "%DESKTOP_SHORTCUT%" "%START_MENU_DIR%\Spektrometr.lnk" >nul 2>&1

echo.
echo ✅ INSTALACJA ZAKOŃCZONA!
echo.
echo 🎉 Spektrometr jest gotowy do użycia!
echo 📍 Lokalizacja: %SPEKTROMETR_DIR%
echo 🖥️ Skrót na pulpicie: Spektrometr.lnk
echo.
pause
goto MENU

:UNINSTALL
cls
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                    ODINSTALOWANIE                           ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

set /p "confirm=❓ Czy na pewno chcesz odinstalować? (wpisz TAK): "
if not "%confirm%"=="TAK" (
    echo ❌ Anulowano
    pause
    goto MENU
)

echo 🗑️ Usuwanie plików...

:: Usunięcie katalogu głównego
if exist "%SPEKTROMETR_DIR%" (
    rmdir /s /q "%SPEKTROMETR_DIR%" >nul 2>&1
    echo ✅ Usunięto katalog aplikacji
)

:: Usunięcie skrótu z pulpitu
if exist %DESKTOP_SHORTCUT% (
    del %DESKTOP_SHORTCUT% >nul 2>&1
    echo ✅ Usunięto skrót z pulpitu
)

:: Usunięcie z menu Start
set START_MENU_DIR="%PROGRAMDATA%\Microsoft\Windows\Start Menu\Programs\Spektrometr"
if exist %START_MENU_DIR% (
    rmdir /s /q %START_MENU_DIR% >nul 2>&1
    echo ✅ Usunięto z menu Start
)

:: Czyszczenie PATH (opcjonalne)
powershell -Command "& {
    $path = [Environment]::GetEnvironmentVariable('PATH', 'Machine')
    $newPath = $path -replace ';[^;]*pixelinksdk[^;]*', ''
    if ($path -ne $newPath) {
        [Environment]::SetEnvironmentVariable('PATH', $newPath, 'Machine')
        Write-Host '✅ Wyczyszczono PATH z PixeLink SDK'
    }
}"

echo.
echo ✅ ODINSTALOWANIE ZAKOŃCZONE!
echo.
pause
goto MENU

:CONFIGURE_SDK
cls
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                KONFIGURACJA PIXELINK SDK                    ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

echo 🔧 Testowanie PixeLink SDK...
cd /d "%SPEKTROMETR_DIR%" 2>nul || cd /d "%~dp0..\.."

python -c "
try:
    from pixelinkWrapper import PxLApi
    print('✅ pixelinkWrapper import sukces')
    try:
        result = PxLApi.getNumberCameras()
        print(f'✅ PxLApi działa: {result}')
    except Exception as e:
        print(f'⚠️ PxLApi error: {e}')
except ImportError as e:
    print(f'❌ Import error: {e}')
"

echo.
echo 🔌 Sprawdzanie urządzeń USB...
powershell -Command "Get-WmiObject Win32_USBControllerDevice | ForEach-Object { [wmi]($_.Dependent) } | Where-Object { $_.Description -like '*PixeLink*' -or $_.Name -like '*PixeLink*' } | Select-Object Description, DeviceID"

echo.
pause
goto MENU

:TEST_INSTALL
cls
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                     TEST INSTALACJI                         ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

echo 🧪 Testowanie instalacji...
echo.

:: Test Pythona
echo 🐍 Python:
python --version 2>nul && echo ✅ Python OK || echo ❌ Python BŁĄD

:: Test pakietów
echo.
echo 📦 Pakiety Python:
for %%p in (numpy matplotlib opencv-python pyserial pillow pixelinkWrapper) do (
    python -c "import %%p; print('✅ %%p OK')" 2>nul || echo ❌ %%p BŁĄD
)

:: Test plików
echo.
echo 📁 Pliki:
if exist "%SPEKTROMETR_DIR%\index.py" (echo ✅ index.py OK) else (echo ❌ index.py BŁĄD)
if exist %DESKTOP_SHORTCUT% (echo ✅ Skrót pulpitu OK) else (echo ❌ Skrót pulpitu BŁĄD)

:: Test SDK
echo.
echo 🔗 PixeLink SDK:
if exist "%SPEKTROMETR_DIR%\pixelinksdk-for-windows-pc_64-v35" (
    echo ✅ SDK pliki OK
) else (
    echo ❌ SDK pliki BŁĄD
)

echo.
pause
goto MENU

:EXIT
echo 👋 Do widzenia!
exit /b 0