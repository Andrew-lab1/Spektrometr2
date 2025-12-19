# 🔬 Spektrometr - Instalacja

## 📋 Przegląd

Spektrometr to aplikacja do analizy spektroskopowej z obsługą kamer PixeLink i kontrolerów silników. Ta instrukcja opisuje proces instalacji na systemach Linux i Windows.

## 🐧 Instalacja na Linux

### Szybka instalacja
```bash
cd install/linux
chmod +x spektrometr-installer.sh
./spektrometr-installer.sh
```

### Opcje wiersza poleceń
```bash
./spektrometr-installer.sh --install     # Bezpośrednia instalacja
./spektrometr-installer.sh --uninstall   # Odinstalowanie
./spektrometr-installer.sh --configure   # Konfiguracja SDK
```

### Co instaluje skrypt:
- ✅ Pakiety systemowe (python3-dev, python3-tk, pip)
- ✅ PixeLink SDK (biblioteki, nagłówki, reguły udev, zmienne środowiskowe)
- ✅ Pakiety Python (numpy, matplotlib, opencv-python, itp.)
- ✅ Aplikacja w `/opt/spektrometr`
- ✅ Globalna komenda `spektrometr`
- ✅ Wpis w menu aplikacji
- ✅ Uprawnienia USB (grupa plugdev)

### Uruchamianie:
```bash
spektrometr
```

## 🪟 Instalacja na Windows

### Szybka instalacja
1. Uruchom jako Administrator: `install/windows/spektrometr-installer.bat`
2. Wybierz opcję `1` (Zainstaluj)
3. Postępuj zgodnie z instrukcjami

### Co instaluje skrypt:
- ✅ Python 3.13 (jeśli nie ma)
- ✅ Pakiety Python (pip install)
- ✅ PixeLink SDK (dodanie do PATH)
- ✅ Aplikacja w `C:\Spektrometr`
- ✅ Skrót na pulpicie
- ✅ Wpis w menu Start

### Uruchamianie:
- Kliknij skrót na pulpicie "Spektrometr"
- Lub uruchom `C:\Spektrometr\run_spektrometr.bat`

## 📦 Wymagania

### Linux (Ubuntu/Debian):
- Ubuntu 20.04+ lub Debian 11+
- Uprawnienia sudo
- Połączenie internetowe
- Porty USB dla kamer PixeLink

### Windows:
- Windows 10/11
- Uprawnienia Administratora
- Połączenie internetowe
- Sterowniki USB dla kamer PixeLink

## 🔌 PixeLink SDK

### Linux:
SDK dla Linuxa jest już dołączone w katalogu `install/linux` tego repozytorium
(`lib/`, `include/`, `PixeLINK.rules`).

Skrypt `install/linux/spektrometr-installer.sh`:
- kopiuje biblioteki `libPxLApi*.so*` do `/usr/local/lib`,
- kopiuje nagłówki do `/usr/local/include/pixelink`,
- instaluje reguły udev (`/etc/udev/rules.d/99-pixelink.rules`),
- tworzy `/etc/profile.d/pixelink-sdk.sh` z ustawieniami:
	- `PIXELINK_SDK_INC=/usr/local/include/pixelink`
	- `PIXELINK_SDK_LIB=/usr/local/lib`
	- aktualizuje `LD_LIBRARY_PATH`.

Po instalacji warto się wylogować i zalogować ponownie,
aby nowe zmienne środowiskowe były widoczne w powłokach.

### Windows:
SDK musi być w katalogu `pixelinksdk-for-windows-pc_64-v35` obok plików aplikacji.

## 🛠️ Rozwiązywanie problemów

### Linux:
```bash
# Test SDK
./spektrometr-installer.sh --configure

# Sprawdzenie uprawnień USB
groups | grep plugdev

# Test aplikacji
spektrometr
```

### Windows:
```batch
# Test instalacji (w instalatorze)
Wybierz opcję: 4 - Test instalacji

# Ręczne uruchomienie
cd C:\Spektrometr
python index.py
```

## 📁 Struktura po instalacji

### Linux:
```
/opt/spektrometr/              # Aplikacja
/usr/local/bin/spektrometr     # Skrypt uruchamiający
/usr/local/lib/libPxLApi.so*   # Biblioteki PixeLink
/etc/udev/rules.d/99-pixelink.rules  # Reguły USB
```

### Windows:
```
C:\Spektrometr\                # Aplikacja
Desktop\Spektrometr.lnk        # Skrót pulpitu
Start Menu\Spektrometr\        # Menu Start
```

## 🗑️ Odinstalowanie

### Linux:
```bash
./spektrometr-installer.sh --uninstall
```

### Windows:
1. Uruchom installer
2. Wybierz opcję `2` (Odinstaluj)
3. Potwierdź wpisując `TAK`

## ⚡ Szybki start

1. **Zainstaluj** używając odpowiedniego skryptu
2. **Uruchom** aplikację (`spektrometr` na Linux, skrót na Windows)
3. **Sprawdź** połączenie kamery w zakładce Camera
4. **Skonfiguruj** porty szeregowe dla silników
5. **Rozpocznij** pomiary!

## 🐛 Błędy i problemy

### Najczęstsze problemy:
- **Brak uprawnień USB** - Dodaj użytkownika do grupy plugdev (Linux)
- **Brak kamery** - Sprawdź połączenie USB i sterowniki
- **Błąd SDK** - Sprawdź czy PixeLink SDK jest poprawnie zainstalowane
- **Brak Pythona** - Installer automatycznie instaluje Python

### Logi i diagnostyka:
- Linux: `/var/log/` lub `journalctl`
- Windows: Event Viewer lub `%TEMP%\spektrometr.log`

## 📞 Wsparcie

W przypadku problemów:
1. Sprawdź wymagania systemowe
2. Użyj opcji testowych w instalatorze
3. Sprawdź czy wszystkie pakiety są zainstalowane
4. Zrestartuj system po instalacji SDK