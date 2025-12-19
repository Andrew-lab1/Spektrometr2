#!/bin/bash

# ===================================================================
# SPEKTROMETR - INSTALLER FOR LINUX
# ===================================================================
# Kompletny skrypt instalacyjny dla systemów Linux
# Zawiera: instalację pakietów, SDK PixeLink, konfigurację, odinstalowanie
# ===================================================================

# Kolory
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m'

# Ścieżki
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PIXELINK_SDK_DIR="$SCRIPT_DIR"  # W tym katalogu znajdują się: lib/, include/, PixeLINK.rules

# Funkcje pomocnicze
print_header() {
    clear
    echo -e "${BLUE}
╔══════════════════════════════════════════════════════════════╗
║                    SPEKTROMETR LINUX                        ║
║                      Installer v1.0                         ║
╚══════════════════════════════════════════════════════════════╝
${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️ $1${NC}"
}

# Funkcja instalacji
install_spektrometr() {
    print_header
    echo -e "${GREEN}🚀 INSTALACJA SPEKTROMETRU${NC}"
    echo ""
    
    # Sprawdzenie uprawnień
    if ! sudo -n true 2>/dev/null; then
        print_info "Wymagane uprawnienia administratora..."
        sudo -v
    fi
    
    # Aktualizacja systemu
    print_info "Aktualizacja pakietów systemowych..."
    sudo apt update
    sudo apt install -y python3-dev python3-tk python3-setuptools python3-pip
    
    # PixeLink SDK (wbudowane w katalog install/linux)
    print_info "Konfiguracja PixeLink SDK..."
    if [ -d "$PIXELINK_SDK_DIR/lib" ] && ls "$PIXELINK_SDK_DIR"/lib/libPxLApi.so* >/dev/null 2>&1; then
        print_success "Znaleziono PixeLink SDK w $PIXELINK_SDK_DIR"

        # Biblioteki współdzielone
        sudo mkdir -p /usr/local/lib
        sudo cp "$PIXELINK_SDK_DIR"/lib/libPxLApi.so* /usr/local/lib/
        if ls "$PIXELINK_SDK_DIR"/lib/libPxLApiLite.so* >/dev/null 2>&1; then
            sudo cp "$PIXELINK_SDK_DIR"/lib/libPxLApiLite.so* /usr/local/lib/
        fi

        # Nagłówki
        if [ -d "$PIXELINK_SDK_DIR/include" ]; then
            sudo mkdir -p /usr/local/include/pixelink
            sudo cp -r "$PIXELINK_SDK_DIR"/include/* /usr/local/include/pixelink/
        fi

        # Reguły udev
        if [ -f "$PIXELINK_SDK_DIR/PixeLINK.rules" ]; then
            sudo cp "$PIXELINK_SDK_DIR/PixeLINK.rules" /etc/udev/rules.d/99-pixelink.rules
            sudo udevadm control --reload-rules
            sudo udevadm trigger
        fi

        # Zmienne środowiskowe SDK (widoczne po ponownym zalogowaniu)
        sudo tee /etc/profile.d/pixelink-sdk.sh > /dev/null << 'EOF'
# PixeLINK SDK environment for Spektrometr
export PIXELINK_SDK_INC="/usr/local/include/pixelink"
export PIXELINK_SDK_LIB="/usr/local/lib"
export LD_LIBRARY_PATH="${PIXELINK_SDK_LIB}:${LD_LIBRARY_PATH:-}"
EOF

        sudo ldconfig
        print_success "PixeLink SDK zainstalowane i skonfigurowane (biblioteki + zmienne środowiskowe)"
        print_warning "Aby zmienne środowiskowe PIXELINK_SDK_* były widoczne w terminalu, wyloguj i zaloguj się ponownie."
    else
        print_warning "PixeLink SDK nie znalezione w $PIXELINK_SDK_DIR/lib - niektóre funkcje kamery mogą nie działać"
    fi
    
    # Pakiety Python
    print_info "Instalacja pakietów Python..."
    cat > /tmp/requirements.txt << 'EOF'
contourpy==1.3.3
cycler==0.12.1
fonttools==4.60.1
kiwisolver==1.4.9
matplotlib==3.10.7
numpy==2.2.6
opencv-python==4.12.0.88
packaging==25.0
pillow==12.0.0
pixelinkWrapper==1.4.1
pyparsing==3.2.5
pyserial==3.5
python-dateutil==2.9.0.post0
six==1.17.0
EOF
    
    if ! pip3 install --user -r /tmp/requirements.txt; then
        print_info "Próba z --break-system-packages..."
        pip3 install --user --break-system-packages -r /tmp/requirements.txt
    fi
    
    # Kopiowanie aplikacji
    print_info "Instalacja aplikacji..."
    sudo mkdir -p /opt/spektrometr
    sudo cp -r ../* /opt/spektrometr/ 2>/dev/null || true
    sudo chown -R $USER:$USER /opt/spektrometr
    
    # Skrypt uruchamiający
    print_info "Tworzenie globalnej komendy..."
    sudo tee /usr/local/bin/spektrometr > /dev/null << 'EOF'
#!/bin/bash

# Środowisko PixeLink SDK dla uruchomienia aplikacji
export PIXELINK_SDK_INC="/usr/local/include/pixelink"
export PIXELINK_SDK_LIB="/usr/local/lib"
export LD_LIBRARY_PATH="${PIXELINK_SDK_LIB}:/usr/lib:${LD_LIBRARY_PATH:-}"

export DISPLAY="${DISPLAY:-:0}"
export C_INCLUDE_PATH="/usr/local/include/pixelink:${C_INCLUDE_PATH:-}"
export CPLUS_INCLUDE_PATH="/usr/local/include/pixelink:${CPLUS_INCLUDE_PATH:-}"

cd /opt/spektrometr
python3 index.py "$@"
EOF
    
    sudo chmod +x /usr/local/bin/spektrometr
    
    # Menu aplikacji
    print_info "Dodawanie do menu..."
    sudo tee /usr/share/applications/spektrometr.desktop > /dev/null << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=Spektrometr
Comment=Spektrometr Application
Exec=/usr/local/bin/spektrometr
Path=/opt/spektrometr
Terminal=false
StartupNotify=true
Categories=Science;Education;
EOF
    
    if command -v update-desktop-database &> /dev/null; then
        sudo update-desktop-database
    fi
    
    # Uprawnienia USB
    print_info "Konfiguracja uprawnień USB..."
    if ! groups | grep -q "plugdev"; then
        sudo usermod -a -G plugdev $USER
        print_warning "Wyloguj się i zaloguj ponownie dla pełnych uprawnień USB"
    fi
    
    print_success "INSTALACJA ZAKOŃCZONA!"
    echo ""
    echo -e "${GREEN}🎉 Spektrometr jest gotowy do użycia!${NC}"
    echo -e "${BLUE}Uruchom: spektrometr${NC}"
}

# Funkcja odinstalowania
uninstall_spektrometr() {
    print_header
    echo -e "${RED}🗑️ ODINSTALOWANIE SPEKTROMETRU${NC}"
    echo ""
    
    read -p "❓ Czy na pewno chcesz odinstalować? (wpisz TAK): " confirm
    if [ "$confirm" != "TAK" ]; then
        print_info "Anulowano"
        exit 0
    fi
    
    print_info "Usuwanie plików..."
    
    # Usunięcie aplikacji
    [ -d "/opt/spektrometr" ] && sudo rm -rf /opt/spektrometr
    [ -f "/usr/local/bin/spektrometr" ] && sudo rm -f /usr/local/bin/spektrometr
    [ -f "/usr/share/applications/spektrometr.desktop" ] && sudo rm -f /usr/share/applications/spektrometr.desktop
    
    # Aktualizacja menu
    if command -v update-desktop-database &> /dev/null; then
        sudo update-desktop-database
    fi
    
    print_success "ODINSTALOWANIE ZAKOŃCZONE!"
}

# Funkcja konfiguracji SDK
configure_sdk() {
    print_header
    echo -e "${YELLOW}🔧 KONFIGURACJA PIXELINK SDK${NC}"
    echo ""
    
    # Test SDK
    print_info "Testowanie SDK..."
    python3 -c "
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
    
    # Sprawdzenie uprawnień
    print_info "Sprawdzanie uprawnień USB..."
    if groups | grep -q "plugdev"; then
        print_success "Użytkownik w grupie plugdev"
    else
        print_warning "Dodaj do grupy plugdev: sudo usermod -a -G plugdev \$USER"
    fi
    
    # Sprawdzenie urządzeń
    print_info "Sprawdzanie urządzeń USB..."
    if lsusb | grep -i pixelink; then
        print_success "Kamera PixeLink wykryta"
    else
        print_warning "Kamera PixeLink nie wykryta"
    fi
}

# Menu główne
show_menu() {
    print_header
    echo -e "${BLUE}Wybierz opcję:${NC}"
    echo ""
    echo "1️⃣  Zainstaluj Spektrometr"
    echo "2️⃣  Odinstaluj Spektrometr"  
    echo "3️⃣  Konfiguruj PixeLink SDK"
    echo "4️⃣  Wyjście"
    echo ""
    read -p "Wybór (1-4): " choice
    
    case $choice in
        1) install_spektrometr ;;
        2) uninstall_spektrometr ;;
        3) configure_sdk ;;
        4) exit 0 ;;
        *) print_error "Nieprawidłowy wybór!" ; sleep 2 ; show_menu ;;
    esac
}

# Sprawdzenie argumentów
if [ "$1" = "--install" ]; then
    install_spektrometr
elif [ "$1" = "--uninstall" ]; then
    uninstall_spektrometr
elif [ "$1" = "--configure" ]; then
    configure_sdk
else
    show_menu
fi