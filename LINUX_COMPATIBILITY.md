# Kompatybilność z Linux

Aplikacja została przetestowana na:
- Ubuntu 20.04/22.04/24.04 (x86_64)

## Zalecana instalacja

Najprostszy sposób instalacji (z pełną obsługą kamery PixeLink) to użycie
skryptu instalacyjnego z dołączonym SDK:

```bash
cd install/linux
chmod +x spektrometr-installer.sh
./spektrometr-installer.sh --install
```

Skrypt:
- kopiuje aplikację do `/opt/spektrometr`,
- instaluje wymagane pakiety Python,
- instaluje PixeLink SDK z katalogu `install/linux` (biblioteki `.so`, nagłówki,
  reguły udev),
- ustawia zmienne środowiskowe SDK w `/etc/profile.d/pixelink-sdk.sh`,
- dodaje globalną komendę `spektrometr`.

Po instalacji zalecane jest wylogowanie i ponowne zalogowanie, aby nowe
zmienne środowiskowe (`PIXELINK_SDK_INC`, `PIXELINK_SDK_LIB`, `LD_LIBRARY_PATH`)
były widoczne we wszystkich terminalach.

## Ręczne uruchamianie (bez instalatora)

Jeśli chcesz tylko szybko uruchomić aplikację bez globalnej instalacji:

```bash
sudo apt-get update
sudo apt-get install python3 python3-pip python3-tk python3-venv

python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python index.py
```

W tym trybie pełna obsługa kamery PixeLink wymaga ręcznej instalacji SDK
wg instrukcji producenta (plik `install/linux/INSTALL.INSTRUCTIONS.txt`) lub
uruchomienia skryptu instalacyjnego opisane wyżej.