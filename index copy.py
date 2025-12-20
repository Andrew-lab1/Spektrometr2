"""
Spektrometr Application - Main File
Refactored with proper threading and structure
"""

import os
import sys
import time
import threading
from concurrent.futures import ThreadPoolExecutor
from tkinter import *
from tkinter import ttk, filedialog, messagebox
import json
import csv
import glob

# Third-party imports
import cv2
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.gridspec import GridSpec
from PIL import Image, ImageTk
import serial
import serial.tools.list_ports
from pixelinkWrapper import PxLApi


# Load configuration
try:
    with open('options.json', 'r') as f:
        options = json.load(f)
except FileNotFoundError:
    options = {
        'step_x': 20, 'step_y': 20, 'offset': 10,  # Values in micrometers (1 pulse = 2 μm)
        'width': 200, 'height': 200, 'await': 0.01,  # Width/height in micrometers
        'sequence_sleep': 0.1,  # Sleep time during sequence measurements
        'starting_corner': 'top-left',  # Starting corner for scanning sequence
        'xmin': '0', 'xmax': '2048',
        'port_x': 'COM5', 'port_y': 'COM9',
        'camera_index': 0,  # Try camera 0 by default
        # Camera settings
        'exposure_time': 10.0,  # Exposure time in milliseconds
        'gain': 1.0  # Camera gain multiplier
    }

# Color constants
LGRAY = '#232323'
DGRAY = '#161616'
RGRAY = '#2c2c2c'
MGRAY = '#1D1c1c'


class StreamToFunction:
    """Redirect stdout to a function"""
    def __init__(self, func):
        self.func = func

    def write(self, message):
        if message.strip():
            self.func(message)

    def flush(self):
        pass


class SpectrometerManager:
    """Simplified Pixelink camera manager based on samples/getNextNumPyFrame.py"""
    
    def __init__(self):
        self.hCamera = None
        self.running = False
        self.thread = None
        
        # Create buffer with reasonable size for PixeLink cameras
        MAX_WIDTH = 2048   # in pixels - more reasonable for most PixeLink models  
        MAX_HEIGHT = 2048  # in pixels - sufficient for most applications
        self.frame_buffer = np.zeros([MAX_HEIGHT, MAX_WIDTH], dtype=np.uint8)
        
        # Check USB device availability
        self._check_usb_device()
        
    def _check_usb_device(self):
        """Check if PixeLink USB device is detected and accessible"""
        try:
            import subprocess
            result = subprocess.run(['lsusb'], capture_output=True, text=True)
            # Silent USB device check - detailed status shown during initialization
                
        except Exception as e:
            print(f"USB device check failed: {e}")
        
    def initialize(self):
        """Initialize camera exactly like sample"""
        try:
            # Initialize any camera - exactly like samples
            ret = PxLApi.initialize(0)
            if not PxLApi.apiSuccess(ret[0]):
                error_code = ret[0]
                print(f"PixeLink initialize failed with error code: {error_code}")
                return False

            self.hCamera = ret[1]
            return True
                
        except Exception as e:
            try:
                if self.hCamera:
                    PxLApi.loadSettings(self.hCamera, PxLApi.Settings.SETTINGS_FACTORY)
                    time.sleep(0.1)
                    return self.initialize()  # Recursive retry
            except:
                pass
                
            return False
    
    def start(self):
        """Start streaming exactly like sample"""
        if self.hCamera and not self.running:
            self.running = True
            self.thread = threading.Thread(target=self._capture_loop, daemon=True)
            self.thread.start()
    
    def stop(self):
        """Stop streaming exactly like sample"""
        self.running = False
        
        if self.thread:
            self.thread.join(timeout=2.0)
            
        if self.hCamera:
            try:
                PxLApi.setStreamState(self.hCamera, PxLApi.StreamState.STOP)
                PxLApi.uninitialize(self.hCamera)
                print("PixeLink camera stopped and uninitialized")
            except Exception as e:
                print(f"Error stopping PixeLink: {e}")
                
        self.hCamera = None

    def get_next_frame(self, maxTries=5):
        """Robust wrapper around getNextFrame exactly like sample"""
        ret = (PxLApi.ReturnCode.ApiUnknownError,)
        
        for _ in range(maxTries):
            ret = PxLApi.getNextNumPyFrame(self.hCamera, self.frame_buffer)
            if PxLApi.apiSuccess(ret[0]):
                return ret
            else:
                # If the streaming is turned off, or worse yet -- is gone?
                if PxLApi.ReturnCode.ApiStreamStopped == ret[0] or \
                   PxLApi.ReturnCode.ApiNoCameraAvailableError == ret[0]:
                    return ret
                else:
                    print(f"    Hmmm... getNextFrame returned {ret[0]}")
        
        # Ran out of tries
        return ret

    def _capture_loop(self):
        """Main capture loop - exactly like sample getNextNumPyFrame.py"""
        if not self.hCamera or not self.frame_buffer.size:
            return
            
        # Start the stream exactly like sample
        ret = PxLApi.setStreamState(self.hCamera, PxLApi.StreamState.START)
        if not PxLApi.apiSuccess(ret[0]):
            print(f"setStreamState with StreamState.START failed, rc = {ret[0]}")
            return

        while self.running:
            try:
                # Use robust wrapper exactly like sample
                ret = self.get_next_frame(1)
                
                if PxLApi.apiSuccess(ret[0]):
                    # ret[1] is frameDescriptor, frame_buffer already contains image data
                    frameDescriptor = ret[1]
                    # Could use frameDescriptor.uFrameNumber, frameDescriptor.fFrameTime etc if needed

                time.sleep(0.5)  # 500ms like sample
                
            except Exception as e:
                print(f"PixeLink capture error: {e}")
                time.sleep(0.1)
        
        # Stop streaming when loop ends
        try:
            ret = PxLApi.setStreamState(self.hCamera, PxLApi.StreamState.STOP)
        except Exception as e:
            print(f"Stop streaming error: {e}")

    def set_exposure(self, exposure_ms):
        """Set camera exposure time in milliseconds"""
        if not self.hCamera:
            print("Camera not initialized - cannot set exposure")
            return False
        
        try:
            # Convert milliseconds to seconds (PixeLink uses seconds)
            exposure_seconds = exposure_ms / 1000.0
            params = [exposure_seconds]
            
            ret = PxLApi.setFeature(self.hCamera, PxLApi.FeatureId.EXPOSURE, PxLApi.FeatureFlags.MANUAL, params)
            if PxLApi.apiSuccess(ret[0]):
                print(f"Exposure set to {exposure_ms} ms")
                return True
            else:
                print(f"Failed to set exposure: {ret[0]}")
                return False
        except Exception as e:
            print(f"Exposure setting error: {e}")
            return False

    def apply_exposure(self, exposure_ms):
        """Asynchronicznie ustaw ekspozycję (wykorzystywane przez GUI).

        Logika ustawiania ekspozycji na kamerze jest trzymana w
        SpectrometerManager, a GUI tylko woła tę funkcję z żądaną
        wartością w ms.
        """
        if not self.hCamera:
            print("Camera not initialized - cannot apply exposure")
            return

        def _worker():
            try:
                self.set_exposure(exposure_ms)
            except Exception as e:
                print(f"Apply exposure worker error: {e}")

        threading.Thread(target=_worker, daemon=True).start()
    
    def set_gain(self, gain_value):
        """Set camera gain value"""
        if not self.hCamera:
            print("Camera not initialized - cannot set gain")
            return False
        
        try:
            params = [float(gain_value)]
            
            ret = PxLApi.setFeature(self.hCamera, PxLApi.FeatureId.GAIN, PxLApi.FeatureFlags.MANUAL, params)
            if PxLApi.apiSuccess(ret[0]):
                print(f"Gain set to {gain_value}")
                return True
            else:
                print(f"Failed to set gain: {ret[0]}")
                return False
        except Exception as e:
            print(f"Gain setting error: {e}")
            return False

    def apply_gain(self, gain_value):
        """Asynchronicznie ustaw gain (wykorzystywane przez GUI).

        Cała logika komunikacji z kamerą dla gain jest w jednym
        miejscu (SpectrometerManager), a GUI tylko przekazuje wartość.
        """
        if not self.hCamera:
            print("Camera not initialized - cannot apply gain")
            return

        def _worker():
            try:
                self.set_gain(gain_value)
            except Exception as e:
                print(f"Apply gain worker error: {e}")

        threading.Thread(target=_worker, daemon=True).start()

    def get_exposure(self):
        """Get current camera exposure time"""
        if not self.hCamera:
            return None
        
        try:
            ret = PxLApi.getFeature(self.hCamera, PxLApi.FeatureId.EXPOSURE)
            if PxLApi.apiSuccess(ret[0]):
                # Convert seconds to milliseconds
                exposure_ms = ret[2][0] * 1000.0
                return exposure_ms
            else:
                return None
        except Exception as e:
            print(f"Get exposure error: {e}")
            return None

    def get_gain(self):
        """Get current camera gain"""
        if not self.hCamera:
            return None
        
        try:
            ret = PxLApi.getFeature(self.hCamera, PxLApi.FeatureId.GAIN)
            if PxLApi.apiSuccess(ret[0]):
                return ret[2][0]
            else:
                return None
        except Exception as e:
            print(f"Get gain error: {e}")
            return None


class MotorController:
    """Controls stepper motors in µm (1 pulse = 2 µm)."""

    MICROMETERS_PER_PULSE = 2.0  # 1 pulse = 2 µm

    def __init__(self, port_x='COM5', port_y='COM9'):
        self.ports = []
        self.connected = False
        self.executor = ThreadPoolExecutor(max_workers=2)

        try:
            if self._check_ports(port_x, port_y):
                self.ports = [serial.Serial(port_x), serial.Serial(port_y)]
                self.connected = True
                self.port_x = port_x
                self.port_y = port_y
                print("Motors connected")
        except Exception as e:
            print(f"Motor connection error: {e}")

    def _check_ports(self, port_x, port_y):
        """Check if ports are available"""
        available_ports = [p.device for p in serial.tools.list_ports.comports()]
        return port_x in available_ports and port_y in available_ports

    def move(self, dx_um: float = 0.0, dy_um: float = 0.0, home: bool = False):
        """Move both axes asynchronously by distance in µm or go home.

        dx_um, dy_um – przesunięcie w µm w osi X i Y.
        home=True – wywołuje komendę "home" na obu osiach.
        """
        if not self.connected:
            return

        # Konwersja: 1 impuls = 2 µm, więc pulses = step/2.
        step_x_pulses = int(abs(dx_um) / 2) if dx_um != 0 else 0
        step_y_pulses = int(abs(dy_um) / 2) if dy_um != 0 else 0

        def _move():
            try:
                if home:
                    # Proste "home" obu osi
                    self.ports[0].write("H:1\r\n".encode())
                    self.ports[1].write("H:1\r\n".encode())
                    return

                if step_x_pulses > 0:
                    if dx_um > 0:
                        self.ports[0].write(f"M:1+P{step_x_pulses}\r\n".encode())
                    else:
                        self.ports[0].write(f"M:1-P{step_x_pulses}\r\n".encode())
                    self.ports[0].write('G:\r\n'.encode())

                if step_y_pulses > 0:
                    if dy_um > 0:
                        self.ports[1].write(f"M:1+P{step_y_pulses}\r\n".encode())
                    else:
                        self.ports[1].write(f"M:1-P{step_y_pulses}\r\n".encode())
                    self.ports[1].write('G:\r\n'.encode())
            except Exception as e:
                print(f"Motor move error: {e}")

        self.executor.submit(_move)
    
    def close(self):
        """Close motor connections"""
        try:
            self.executor.shutdown(wait=True)
        except Exception:
            pass
        for port in self.ports:
            try:
                port.close()
            except Exception:
                pass


class CustomWindow:
    """Custom window base class"""
    
    def __init__(self, *args, **kwargs):
        self.tk_title = "Arcy puszka"
        self.LGRAY = LGRAY
        self.DGRAY = DGRAY
        self.RGRAY = RGRAY
        self.MGRAY = MGRAY
        self._setup_window()
    
    def _setup_window(self):
        """Setup custom window elements"""
        self.title_bar = Frame(self, bg=self.RGRAY, relief='raised', bd=0, 
                              highlightthickness=1, highlightbackground=self.MGRAY)
        
        self.close_button = Button(self.title_bar, text='  ×  ', command=self.close_application, 
                                  bg=self.RGRAY, padx=2, pady=2, font=("calibri", 13), 
                                  bd=0, fg='lightgray', highlightthickness=0)
        
        self.minimize_button = Button(self.title_bar, text=' 🗕 ', command=self.minimize_me, 
                                     bg=self.RGRAY, padx=2, pady=2, bd=0, fg='lightgray', 
                                     font=("calibri", 13), highlightthickness=0)
        
        self.title_bar_title = Label(self.title_bar, text=self.tk_title, bg=self.RGRAY, 
                                    bd=0, fg='lightgray', font=("helvetica", 10))
        
        self.window = Frame(self, bg=self.DGRAY, highlightthickness=1, 
                           highlightbackground=self.MGRAY)
        
        # Pack elements
        self.title_bar.pack(fill=X)
        self.title_bar_title.pack(side=LEFT, padx=10)
        self.close_button.pack(side=RIGHT, ipadx=7, ipady=1)
        self.minimize_button.pack(side=RIGHT, ipadx=7, ipady=1)
        self.window.pack(expand=1, fill=BOTH)
        self.window.pack_propagate(1)

        # Bind events
        self.title_bar.bind('<Button-1>', self.get_pos)
        self.title_bar_title.bind('<Button-1>', self.get_pos)
        self.close_button.bind('<Enter>', lambda e: self.changex_on_hovering())
        self.close_button.bind('<Leave>', lambda e: self.returnx_to_normalstate())
        
        if hasattr(self, 'winfo_class') and self.winfo_class() == 'Tk':
            self.bind("<Expose>", lambda e: self.deminimize())
        self.after(10, lambda: self.set_appwindow())
    
    def get_pos(self, event):
        """Handle window dragging"""
        xwin = self.winfo_x()
        ywin = self.winfo_y()
        startx = event.x_root
        starty = event.y_root
        ywin = ywin - starty
        xwin = xwin - startx
        
        def move_window(event):
            self.config(cursor="fleur")
            self.geometry(f'+{event.x_root + xwin}+{event.y_root + ywin}')

        def release_window(event):
            self.config(cursor="arrow")

        self.title_bar.bind('<B1-Motion>', move_window)
        self.title_bar.bind('<ButtonRelease-1>', release_window)
    
    def set_appwindow(self):
        """Set window to appear in taskbar"""
        try:
            # Windows-specific code - skip on Linux
            import sys
            if sys.platform == 'win32':
                from ctypes import windll
                GWL_EXSTYLE = -20
                WS_EX_APPWINDOW = 0x00040000
                WS_EX_TOOLWINDOW = 0x00000080
                hwnd = windll.user32.GetParent(self.winfo_id())
                stylew = windll.user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
                stylew = stylew & ~WS_EX_TOOLWINDOW
                stylew = stylew | WS_EX_APPWINDOW
                windll.user32.SetWindowLongW(hwnd, GWL_EXSTYLE, stylew)
                self.wm_withdraw()
                self.after(10, lambda: self.wm_deiconify())
        except:
            pass
    
    def minimize_me(self):
        """Minimize window"""
        import sys
        if sys.platform == 'win32':
            self.overrideredirect(False)
            self.attributes('-alpha', 0)
            self.wm_state('iconic')
        else:
            try:
                self.iconify()
            except Exception:
                self.wm_state('iconic')
    
    def deminimize(self):
        """Restore window"""
        import sys
        # Przywracanie po minimalizacji: na Windows wracamy do starej
        # logiki, na Linuksie po prostu deiconify + normalny stan.
        if sys.platform == 'win32':
            self.overrideredirect(True)
            self.attributes('-alpha', 1)
            try:
                self.wm_state('zoomed')
            except Exception:
                self.wm_state('normal')
        else:
            try:
                self.deiconify()
            except Exception:
                self.wm_state('normal')
    
    def changex_on_hovering(self):
        """Close button hover effect"""
        self.close_button['bg'] = 'red'
    
    def returnx_to_normalstate(self):
        """Close button normal state"""
        self.close_button['bg'] = self.RGRAY
    
    def close_application(self):
        """Close application properly"""
        try:
            # For main window, call on_closing
            if hasattr(self, 'on_closing'):
                self.on_closing()
            else:
                # For other windows, just destroy
                self.destroy()
        except:
            import sys
            sys.exit(0)

class CButton(Button):
    """Custom button with hover effects"""
    
    def __init__(self, *args, **kwargs):
        Button.__init__(self, *args, **kwargs)
        self.config(
            bg=RGRAY, padx=2, pady=2, bd=0, fg='lightgray',
            highlightthickness=0, relief='flat'
        )
        self.bind('<Enter>', self.on_enter)
        self.bind('<Leave>', self.on_leave)
        self.bind('<ButtonPress-1>', self.on_press)
        self.bind('<ButtonRelease-1>', self.on_release)

    def on_enter(self, event, color='gray'):
        self.config(bg=color)

    def on_leave(self, event):
        self.config(bg=RGRAY)

    def on_press(self, event):
        self.config(relief='sunken')

    def on_release(self, event):
        self.config(relief='flat')

class CustomTk(Tk, CustomWindow):
    """Custom main window"""
    
    def __init__(self, *args, **kwargs):
        Tk.__init__(self, *args, **kwargs)
        CustomWindow.__init__(self, *args, **kwargs)
        self.tk_title = "Spektrometr"
        
        # Get screen dimensions first
        self.update_idletasks()
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        
        # Linux-compatible fullscreen setup
        self.geometry(f'{screen_width}x{screen_height}+0+0')
        self.attributes('-fullscreen', True)  # True fullscreen
        self.overrideredirect(False)  # Keep window manager for better compatibility
        self.config(bg=self.DGRAY, highlightthickness=0)
                
        # Also bind Ctrl+Q for quick quit
        self.bind('<Control-q>', lambda e: self.on_closing())

class CustomToplevel(Toplevel, CustomWindow):
    """Custom dialog window z własnym oknem potwierdzenia (confirm)."""
    
    def __init__(self, *args, **kwargs):
        Toplevel.__init__(self, *args, **kwargs)
        CustomWindow.__init__(self, *args, **kwargs)
        self.overrideredirect(True)
        self.config(bg=self.DGRAY, highlightthickness=0)

    @staticmethod
    def confirm(parent, message: str, title: str = "Potwierdź") -> bool:

        result = {'value': False}

        def _build_dialog(dlg: "CustomToplevel", done_callback=None):
            """Wspólne budowanie zawartości dialogu."""
            sw = dlg.winfo_screenwidth()
            sh = dlg.winfo_screenheight()
            ww, wh = 420, 160
            x = (sw - ww) // 2
            y = (sh - wh) // 2
            dlg.geometry(f"{ww}x{wh}+{x}+{y}")

            frame = Frame(dlg.window, bg=dlg.DGRAY)
            frame.pack(fill=BOTH, expand=True, padx=15, pady=15)

            Label(frame, text=message, bg=dlg.DGRAY,
                  fg='white', font=("Arial", 11, "bold")).pack(pady=(0, 10))

            btns = Frame(frame, bg=dlg.DGRAY)
            btns.pack(pady=(10, 0))

            def _set(val: bool):
                result['value'] = val
                try:
                    dlg.destroy()
                except Exception:
                    pass
                if done_callback is not None:
                    done_callback()

            CButton(btns, text="Tak", width=10,
                    command=lambda: _set(True)).pack(side=LEFT, padx=10)
            CButton(btns, text="Nie", width=10,
                    command=lambda: _set(False)).pack(side=LEFT, padx=10)

        # Główny wątek: klasyczne modalne okno z wait_window
        if threading.current_thread() is threading.main_thread():
            try:
                dlg = CustomToplevel(parent)
                dlg.tk_title = title
                _build_dialog(dlg)
                try:
                    dlg.grab_set()
                except Exception:
                    pass
                try:
                    parent.wait_window(dlg)
                except Exception:
                    # Jeśli coś pójdzie nie tak, traktuj jak anulowanie
                    return False
                return bool(result['value'])
            except Exception:
                return False

        # Wątek roboczy: użyj after + Event, aby nie blokować pętli Tk
        event = threading.Event()

        def _show_dialog_from_worker():
            try:
                dlg = CustomToplevel(parent)
                dlg.tk_title = title

                def _done():
                    event.set()

                _build_dialog(dlg, done_callback=_done)
            except Exception:
                event.set()

        try:
            parent.after(0, _show_dialog_from_worker)
            event.wait()
        except Exception:
            return False

        return bool(result['value'])

    @staticmethod
    def alert(parent, message: str, title: str = "Info") -> None:
        """Proste okno powiadomienia z przyciskiem OK (customowe).

        Analogicznie do confirm:
        - w głównym wątku używa wait_window,
        - w wątku roboczym używa after + Event.
        """

        def _build_dialog(dlg: "CustomToplevel", done_callback=None):
            sw = dlg.winfo_screenwidth()
            sh = dlg.winfo_screenheight()
            ww, wh = 420, 160
            x = (sw - ww) // 2
            y = (sh - wh) // 2
            dlg.geometry(f"{ww}x{wh}+{x}+{y}")

            frame = Frame(dlg.window, bg=dlg.DGRAY)
            frame.pack(fill=BOTH, expand=True, padx=15, pady=15)

            Label(frame, text=message, bg=dlg.DGRAY,
                  fg='white', font=("Arial", 11)).pack(pady=(0, 10))

            btns = Frame(frame, bg=dlg.DGRAY)
            btns.pack(pady=(10, 0))

            def _close():
                try:
                    dlg.destroy()
                except Exception:
                    pass
                if done_callback is not None:
                    done_callback()

            CButton(btns, text="OK", width=10, command=_close).pack(side=LEFT, padx=10)

        # Główny wątek: synchroniczny alert z wait_window
        if threading.current_thread() is threading.main_thread():
            try:
                dlg = CustomToplevel(parent)
                dlg.tk_title = title
                _build_dialog(dlg)
                try:
                    dlg.grab_set()
                except Exception:
                    pass
                try:
                    parent.wait_window(dlg)
                except Exception:
                    return
                return
            except Exception:
                return

        # Wątek roboczy: after + Event
        event = threading.Event()

        def _show_dialog_from_worker():
            try:
                dlg = CustomToplevel(parent)
                dlg.tk_title = title

                def _done():
                    event.set()

                _build_dialog(dlg, done_callback=_done)
            except Exception:
                event.set()

        try:
            parent.after(0, _show_dialog_from_worker)
            event.wait()
        except Exception:
            return


class HeatMapWindow(CustomToplevel):
    """Viewer wyników: prosty slider długości fali + mapa 2D + widmo.

    Klasa sama przechowuje nazwę pliku z pomiarem i sama go
    wczytuje przy otwarciu.
    """

    def __init__(self, parent, filename):
        CustomToplevel.__init__(self, parent)

        # Zapamiętaj pełną ścieżkę i nazwę pliku pomiaru
        self.filename = filename
        self.basename = os.path.basename(filename)
        self.title(f"Measurement: {self.basename}")

        # Rozmiar okna ~80% ekranu, wyśrodkowany
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        window_width = int(screen_width * 0.8)
        window_height = int(screen_height * 0.8)
        x = (screen_width - window_width) // 2
        y = (screen_height - window_height) // 2
        self.geometry(f"{window_width}x{window_height}+{x}+{y}")

        # Wczytaj dane z pliku CSV: x, y, spectrum... (na podstawie self.filename)
        data = []
        try:
            with open(self.filename, 'r') as f:
                reader = csv.reader(f)
                for row in reader:
                    if len(row) > 2:
                        try:
                            x_val = float(row[0])
                            y_val = float(row[1])
                            spectrum = [float(v) for v in row[2:]]
                            data.append([x_val, y_val, spectrum])
                        except Exception:
                            continue
        except Exception as e:
            Label(self.window, text=f"Error loading data: {e}",
                  bg=self.DGRAY, fg='red').pack(padx=10, pady=10)
            return

        if not data:
            Label(self.window, text="No spectrum data in file",
                  bg=self.DGRAY, fg='lightgray').pack(padx=10, pady=10)
            return

        # Prosta siatka X/Y
        xs = sorted({row[0] for row in data})
        ys = sorted({row[1] for row in data})
        nx, ny = len(xs), len(ys)

        # Zbuduj pełną kostkę widmową: (ny, nx, L)
        min_len = min(len(row[2]) for row in data)
        self.cube = np.zeros((ny, nx, min_len), dtype=float)

        for x_val, y_val, spec in data:
            try:
                x_idx = xs.index(x_val)
                y_idx = ys.index(y_val)
            except ValueError:
                continue

            spec_arr = np.array(spec[:min_len], dtype=float)
            self.cube[y_idx, x_idx, :] = spec_arr

        # Uśrednione widmo po całym obszarze
        self.mean_spec = np.mean(self.cube, axis=(0, 1))

        # Oś długości fali: liniowe odwzorowanie indeksów -> λ
        try:
            wl_min = float(options.get('wavelength_min', 0.0))
            wl_max = float(options.get('wavelength_max', float(min_len - 1)))
        except Exception:
            wl_min, wl_max = 0.0, float(min_len - 1)

        self.x_axis = np.linspace(wl_min, wl_max, min_len)
        self.current_idx = 0
        self.current_wavelength = float(self.x_axis[0]) if len(self.x_axis) > 0 else 0.0

        # Górny panel: opis + slider po długości fali
        control_frame = Frame(self.window, bg=self.DGRAY)
        control_frame.pack(fill=X, padx=10, pady=5)

        Label(control_frame, text="Wavelength (λ):", bg=self.DGRAY,
              fg='white', font=("Arial", 10, "bold")).pack(side=LEFT)
        self.lambda_label = Label(control_frame, text=f"{self.current_wavelength:.1f}", bg=self.DGRAY,
                                   fg='lightgreen', font=("Arial", 10))
        self.lambda_label.pack(side=LEFT, padx=(5, 15))

        self.slider = Scale(
            control_frame,
            from_=0,
            to=min_len - 1,
            orient=HORIZONTAL,
            command=self._on_slider,
            bg=self.DGRAY,
            fg='white',
            highlightthickness=0,
            troughcolor=self.RGRAY,
            showvalue=False,
            length=400
        )
        self.slider.pack(side=LEFT, fill=X, expand=True)

        # Layout: u góry 2D mapa, na dole widmo (stały GridSpec)
        self.fig = plt.figure(figsize=(10, 7), facecolor=self.DGRAY)
        gs = GridSpec(2, 1, height_ratios=[2, 1])
        self.ax2d = self.fig.add_subplot(gs[0, 0])
        self.ax_spec = self.fig.add_subplot(gs[1, 0])
        self.colorbar = None
        
        try:
            # Większy odstęp między górnym a dolnym wykresem
            self.fig.subplots_adjust(top=0.93, bottom=0.12, left=0.08, right=0.98, hspace=0.6)
        except Exception:
            pass

        # Inicjalne rysowanie
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.window)
        self.canvas.get_tk_widget().pack(fill=BOTH, expand=True, padx=5, pady=5)

        try:
            self.current_idx = 0
            self._update_plots()
        except Exception:
            pass
        try:
            self.slider.set(0)
        except Exception:
            pass

    def _on_slider(self, value):
        """Zmiana indeksu długości fali (piksela)."""
        try:
            idx = int(float(value))
            idx = max(0, min(idx, self.cube.shape[2] - 1))
            self.current_idx = idx
            try:
                self.current_wavelength = float(self.x_axis[idx])
            except Exception:
                self.current_wavelength = float(idx)
            self.lambda_label.configure(text=f"{self.current_wavelength:.1f}")
            self._update_plots()
        except Exception:
            pass

    def _update_plots(self):
        """Aktualizacja mapy 2D dla aktualnego indeksu i średniego widma."""
        try:
            self.ax2d.clear()
            self.ax_spec.clear()

            # Mapa 2D dla wybranego indeksu
            slice_2d = self.cube[:, :, self.current_idx]
            ny, nx = slice_2d.shape
            im = self.ax2d.imshow(
                slice_2d,
                origin='lower',
                cmap='hot',
                # Równe proporcje pikseli – mapa nie zmienia kształtu
                aspect='equal'
            )
            # Ustal stałe granice osi, żeby rozmiar mapy był stabilny
            try:
                self.ax2d.set_xlim(-0.5, nx - 0.5)
                self.ax2d.set_ylim(-0.5, ny - 0.5)
            except Exception:
                pass
            # Wycentruj mapę w osi przy equal aspect
            try:
                self.ax2d.set_anchor('C')
            except Exception:
                pass
            # Czytelniejsze opisy i osie dla mapy 2D (wavelength zamiast pixeli)
            wl_val = getattr(self, 'current_wavelength', float(self.current_idx))
            self.ax2d.set_title(
                f"Intensity map @ λ = {wl_val:.1f}",
                color='white', fontsize=16, fontweight='bold'
            )
            self.ax2d.set_xlabel("X index", color='white', fontsize=14, fontweight='bold')
            self.ax2d.set_ylabel("Y index", color='white', fontsize=14, fontweight='bold')
            self.ax2d.set_facecolor(self.DGRAY)
            self.ax2d.tick_params(colors='white', labelsize=10)
            for spine in self.ax2d.spines.values():
                spine.set_color('white')

            # Jedna stała colorbar – tylko aktualizujemy mapę, żeby oś nie zmieniała rozmiaru
            if self.colorbar is None:
                self.colorbar = self.fig.colorbar(im, ax=self.ax2d, fraction=0.046, pad=0.04)
                self.colorbar.ax.tick_params(colors='white', labelsize=10)
                try:
                    self.colorbar.outline.set_edgecolor('white')
                except Exception:
                    pass
            else:
                try:
                    self.colorbar.update_normal(im)
                except Exception:
                    pass

            # Średnie widmo + pionowa linia aktualnej długości fali
            self.ax_spec.plot(self.x_axis, self.mean_spec, color='orange')
            try:
                wl_val = getattr(self, 'current_wavelength', float(self.current_idx))
                self.ax_spec.axvline(wl_val, color='red', linestyle='--', linewidth=1.5)
            except Exception:
                self.ax_spec.axvline(self.current_idx, color='red', linestyle='--', linewidth=1.5)
            # Czytelniejsze opisy i osie dla widma
            self.ax_spec.set_title("Average spectrum vs. wavelength", color='white', fontsize=16, fontweight='bold')
            self.ax_spec.set_xlabel("Wavelength (λ)", color='white', fontsize=14, fontweight='bold')
            self.ax_spec.set_ylabel("Intensity", color='white', fontsize=14, fontweight='bold')
            self.ax_spec.grid(True, alpha=0.3, color='gray')
            self.ax_spec.set_facecolor(self.DGRAY)
            self.ax_spec.tick_params(colors='white', labelsize=10)
            for spine in self.ax_spec.spines.values():
                spine.set_color('white')

            self.canvas.draw_idle()
        except Exception as e:
            print(f"HeatMap redraw error: {e}")


class SpektrometerApp(CustomTk):
    """Main application class"""
    
    def __init__(self):
        super().__init__()
        self.title("Spektrometr")

        # Set to maximum screen size
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        self.geometry(f'{screen_width}x{screen_height}+0+0')
        
        # Konfiguracja aplikacji i sprzetu
        self.spectrometer_manager = SpectrometerManager()
        self.motor_controller = MotorController(
            options.get('port_x', 'COM10'),
            options.get('port_y', 'COM11')
        )

        self.motors_ready = False
        self.pixelink_ready = False
        
        # Variables
        self.measurement_files = []  # Store filenames only, not data
        self.current_image = None
        self.spectrum_data = np.zeros(2048)
        
        self.pixelink_image_data = None  # Store current PixeLink frame
        
        # Sequence control flags
        self._sequence_running = False
        self._sequence_stop_requested = False
        self._shutting_down = False
        
        self._create_widgets()
        self._setup_styles()
        
        # NOW redirect stdout after console is created
        sys.stdout = StreamToFunction(self.console_output)
        # Ensure cleanup runs on window close to avoid PhotoImage/Tcl teardown races
        try:
            self.protocol("WM_DELETE_WINDOW", self.on_closing)
        except Exception:
            pass
        
        # Initialize systems and start update loop
        self.after(100, self._delayed_init)
        
        # Keep track of after() calls to cancel them during cleanup
        self._after_ids = []

    def _create_widgets(self):
        """Create main GUI widgets"""
        # Notebook for tabs
        self.notebook = ttk.Notebook(self.window)
        self.notebook.pack(fill=BOTH, expand=True, padx=2, pady=2)
        
        # Create tabs - COMBINED TABS
        self.tab_camera_controls = Frame(self.notebook, bg=self.DGRAY)  # Camera + Controls
        self.tab_spectrum_pixelink = Frame(self.notebook, bg=self.DGRAY)  # Spectrum + Pixelink
        self.tab_results = Frame(self.notebook, bg=self.DGRAY)
        self.tab_settings = Frame(self.notebook, bg=self.DGRAY)
        
        self.notebook.add(self.tab_camera_controls, text="Camera & Controls")
        self.notebook.add(self.tab_spectrum_pixelink, text="Spectrum")
        self.notebook.add(self.tab_results, text="Results")
        self.notebook.add(self.tab_settings, text="Settings")
        
        # Setup individual tabs
        self._setup_camera_controls_tab()
        self._setup_spectrum_pixelink_tab()
        self._setup_results_tab()
        self._setup_settings_tab()

    def _setup_camera_controls_tab(self):
        """Setup simplified camera tab with centered view and calibrate button only"""
        # Create main container 
        main_container = Frame(self.tab_camera_controls, bg=self.DGRAY)
        main_container.pack(fill=BOTH, expand=True, padx=5, pady=5)
        
        # Camera title
        Label(main_container, text="Camera", font=("Arial", 16, "bold"), 
              bg=self.DGRAY, fg='white').pack(pady=(0, 10))
        
        # Centered camera display (Canvas with drag select) - moderate size
        camera_container = Frame(main_container, bg=self.DGRAY)
        camera_container.pack(expand=False, fill=X)
        
        self.camera_canvas = Canvas(camera_container, bg=self.DGRAY, highlightthickness=0)
        self.camera_canvas.pack()
        self._camera_canvas_img = None  # Initialize as None instead of reading from camera
        # Use 3/5 of screen dimensions for camera canvas
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        self._camera_frame_size = (int(screen_width * 3/5), int(screen_height * 3/5))  # 0.4 to leave space for controls
        self.camera_canvas.config(width=self._camera_frame_size[0], height=self._camera_frame_size[1])
        
        # Status under canvas
        self.cam_status = Label(main_container, bg=self.DGRAY, fg='lightgray', 
                               text="Camera Status: Not Started", font=("Arial", 10))
        self.cam_status.pack(pady=5)
        
        # Control buttons frame - centered
        control_frame = Frame(main_container, bg=self.DGRAY)
        control_frame.pack(pady=10)
        
        # Essential controls only
        CButton(control_frame, text="Start Camera", command=lambda: self.start_camera()).pack(side=LEFT, padx=5)
        CButton(control_frame, text="Stop Camera", command=lambda: self.stop_camera()).pack(side=LEFT, padx=5)
        
        # Add Start Sequence button (needed for state management)
        self.start_seq_btn = CButton(control_frame, text="Start Sequence", command=self.start_measurement_sequence)
        self.start_seq_btn.pack(side=LEFT, padx=5)
        
        # Add Stop Sequence button
        self.stop_seq_btn = CButton(control_frame, text="Stop Sequence", command=self.stop_measurement_sequence)
        self.stop_seq_btn.pack(side=LEFT, padx=5)
        self.stop_seq_btn.config(state=DISABLED)  # Initially disabled
        
        # Initial state based on calibration
        self._update_start_seq_state()
        
        # Motor control section - na dole, przylegające do krawędzi
        motor_frame = LabelFrame(main_container, text="Manual Motor Control", bg=self.DGRAY, fg='white')
        motor_frame.pack(fill=BOTH, expand=True, pady=10, side=BOTTOM)  # Dodano side=BOTTOM
        
        # Horizontal layout: Console on left, buttons on right
        horizontal_frame = Frame(motor_frame, bg=self.DGRAY)
        horizontal_frame.pack(fill=BOTH, expand=True, padx=10, pady=10)
        
        # Left side - Console (larger)
        console_frame = LabelFrame(horizontal_frame, text="Status Console", bg=self.DGRAY, fg='white')
        console_frame.pack(side=LEFT, fill=BOTH, expand=True, padx=(0, 10))
        
        self.console = Text(console_frame, bg=self.DGRAY, fg='lightgreen', height=10, wrap=WORD)
        console_scrollbar = Scrollbar(console_frame, orient=VERTICAL, command=self.console.yview)
        self.console.configure(yscrollcommand=console_scrollbar.set)
        # Color tags for log levels
        self.console.tag_configure("error", foreground="red")
        self.console.tag_configure("warning", foreground="yellow")
        self.console.tag_configure("normal", foreground="lightgreen")

        self.console.pack(side=LEFT, fill=BOTH, expand=True)
        console_scrollbar.pack(side=RIGHT, fill=Y)
        
        # Right side - Motor controls (smaller)
        motor_controls_frame = Frame(horizontal_frame, bg=self.DGRAY)
        motor_controls_frame.pack(side=RIGHT, fill=Y, padx=(10, 0))
        
        # Step size controls
        Label(motor_controls_frame, text="Step Size:", bg=self.DGRAY, fg='white', font=("Arial", 10)).pack(pady=(0, 5))
        step_control_frame = Frame(motor_controls_frame, bg=self.DGRAY)
        step_control_frame.pack(pady=(0, 10))
        
        self.motor_step_var = IntVar(value=1)
        step_sizes = [1, 5, 10, 25, 50]
        for i, step in enumerate(step_sizes):
            if i < 3:  # First row
                row, col = 0, i
            else:  # Second row
                row, col = 1, i-3
            Radiobutton(step_control_frame, text=str(step), variable=self.motor_step_var, value=step,
                       bg=self.DGRAY, fg='white', selectcolor=self.RGRAY, font=("Arial", 9),
                       activebackground=self.RGRAY).grid(row=row, column=col, padx=1, pady=1)
        
        # Directional buttons in cross pattern (smaller)
        Label(motor_controls_frame, text="Movement:", bg=self.DGRAY, fg='white', font=("Arial", 10)).pack(pady=(10, 5))
        direction_frame = Frame(motor_controls_frame, bg=self.DGRAY)
        direction_frame.pack()
        
        # Row 1: Up button
        Button(direction_frame, text="↑", command=lambda: self.move_motor('u'), 
               width=4, bg=RGRAY, fg='white', font=("Arial", 10)).grid(row=0, column=1, padx=2, pady=2)
        
        # Row 2: Left, Origin, Right buttons  
        Button(direction_frame, text="←", command=lambda: self.move_motor('l'), 
               width=4, bg=RGRAY, fg='white', font=("Arial", 10)).grid(row=1, column=0, padx=2, pady=2)
        Button(direction_frame, text="⌂", command=lambda: self.move_motor('o'), 
               width=4, bg=RGRAY, fg='white', font=("Arial", 10)).grid(row=1, column=1, padx=2, pady=2)
        Button(direction_frame, text="→", command=lambda: self.move_motor('r'), 
               width=4, bg=RGRAY, fg='white', font=("Arial", 10)).grid(row=1, column=2, padx=2, pady=2)
        
        # Row 3: Down button
        Button(direction_frame, text="↓", command=lambda: self.move_motor('d'), 
               width=4, bg=RGRAY, fg='white', font=("Arial", 10)).grid(row=2, column=1, padx=2, pady=2)
        
        # Motor status
        self.motor_status = Label(motor_controls_frame, bg=self.DGRAY, fg='lightgray', 
                                 text="Motor Status: Checking...", font=("Arial", 9), wraplength=150)
        self.motor_status.pack(pady=(10, 0))

    def move_motor(self, direction: str):
        """Obsługa przycisków strzałek: ruch w µm lub home."""
        try:
            if not self.motor_controller or not self.motor_controller.connected:
                print("Motor controller not connected")
                return

            # Mnożnik z przycisków (1,5,10,25,50)
            step_mult = self.motor_step_var.get() if hasattr(self, 'motor_step_var') else 1

            base_step_x = self.step_x.get() if hasattr(self, 'step_x') else options.get('step_x', 20)
            base_step_y = self.step_y.get() if hasattr(self, 'step_y') else options.get('step_y', 20)

            step_x_um = step_mult * base_step_x
            step_y_um = step_mult * base_step_y

            if direction == 'o':
                self.motor_controller.move(home=True)
                return

            dx = 0.0
            dy = 0.0
            if direction == 'r':
                dx = step_x_um
            elif direction == 'l':
                dx = -step_x_um
            elif direction == 'u':
                dy = step_y_um
            elif direction == 'd':
                dy = -step_y_um

            if dx != 0.0 or dy != 0.0:
                self.motor_controller.move(dx_um=dx, dy_um=dy)
        except Exception as e:
            print(f"Manual motor move error: {e}")

    def _setup_spectrum_pixelink_tab(self):
        """Setup spectrum tab with image preview and spectrum plot"""
        # Create main container
        main_container = Frame(self.tab_spectrum_pixelink, bg=self.DGRAY)
        main_container.pack(fill=BOTH, expand=True, padx=5, pady=5)
        
        # Top section - Image Preview (fixed size to leave room for controls)
        image_frame = Frame(main_container, bg=self.DGRAY)
        image_frame.pack(fill=BOTH, pady=(0, 10))
        image_frame.pack_propagate(False)
        image_frame.configure(height=self.winfo_screenheight()*3//5)
        # Calculate 3/5 of screen height minus space for larger spectrum (350px) and controls # Minimum 400px
        
        # Image title and status
        image_header = Frame(image_frame, bg=self.DGRAY)
        image_header.pack(fill=X, pady=(0, 5))
        
        Label(image_header, text="PixeLink Camera", font=("Arial", 14, "bold"), 
              bg=self.DGRAY, fg='white').pack(side=LEFT)
        
        # Default Pixelink status: assume offline until initialization succeeds
        self.pixelink_status = Label(
            image_header,
            text="Offline",
            bg=self.DGRAY, fg='red', font=("Arial", 10)
        )
        self.pixelink_status.pack(side=RIGHT)
        
        # Canvas for image display - controlled size
        canvas_container = Frame(image_frame, bg=self.DGRAY)
        canvas_container.pack(fill=BOTH, pady=5)
        
        self.spectrum_image_canvas = Canvas(
            canvas_container,
            bg='black',
            highlightthickness=0
        )
        self.spectrum_image_canvas.pack()
        
        # Use 3/5 of screen width and adapt height to available space
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        canvas_width = int(screen_width * 3/5)
        canvas_height = int(screen_height * 3/5)  # Reserve space for larger spectrum (350px) and controls
        self._spectrum_image_size = (canvas_width,canvas_height)
        self.spectrum_image_canvas.config(width=self._spectrum_image_size[0], height=self._spectrum_image_size[1])
        self.spectrum_image_canvas_image = None  # Reference for image
        
        # Centered placeholder text
        self.spectrum_image_canvas.create_text(
            self._spectrum_image_size[0]//2,
            self._spectrum_image_size[1]//2,
            text="Camera Preview/Initializing...",
            fill='white',
            font=("Arial", 12),
            tags="placeholder"
        )
        
        # Camera Controls section (Compact between image and spectrum)
        controls_frame = Frame(main_container, bg=self.DGRAY)
        controls_frame.pack(fill=X, pady=(5, 5))
        
        # Title for controls
        Label(controls_frame, text="Camera Controls", font=("Arial", 11, "bold"), 
              bg=self.DGRAY, fg='white').pack(pady=(0, 5))  # Reduced font and padding
        
        # Add PixeLink reconnect button
        reconnect_frame = Frame(controls_frame, bg=self.DGRAY)
        reconnect_frame.pack(fill=X, pady=(0, 5))
        
        CButton(reconnect_frame, text="Reconnect PixeLink", command=self._force_pixelink_reconnect, 
               bg='#ff6b6b', fg='white').pack(side=LEFT, padx=(0, 10))
        
        self.pixelink_reconnect_status = Label(reconnect_frame, text="Ready to connect", 
                                              bg=self.DGRAY, fg='lightgray', font=("Arial", 9))
        self.pixelink_reconnect_status.pack(side=LEFT)
        
        # Controls container with two columns
        controls_container = Frame(controls_frame, bg=self.DGRAY)
        controls_container.pack(fill=X, padx=15)  # Reduced padding
        
        # Exposure control
        exposure_frame = Frame(controls_container, bg=self.DGRAY)
        exposure_frame.pack(side=LEFT, fill=X, expand=True, padx=(0, 10))  # Reduced padding
        
        Label(exposure_frame, text="Exposure Time (ms)", 
              bg=self.DGRAY, fg='white', font=("Arial", 9)).pack(anchor=W)  # Smaller font
        
        # Logiczna wartość ekspozycji (ms) – używana w sekwencji i zapisach
        self.exposure_var = DoubleVar(value=options.get('exposure_time', 10.0))
        # Suwak pracuje w zakresie 0-1 (ułamek), przeliczany ręcznie na ms
        # Zainicjalizujemy go później z faktycznej wartości ekspozycji
        self._exposure_slider_var = DoubleVar(value=0.0)
        self.exposure_scale = Scale(
            exposure_frame,
            from_=0.0,
            to=1.0,
            orient=HORIZONTAL,
            variable=self._exposure_slider_var,
            resolution=0.001,
            bg=self.DGRAY,
            fg='white',
            highlightbackground=self.DGRAY,
            troughcolor='gray',
            command=self._on_exposure_change
        )
        # Ensure slider is interactive (some Linux themes may interfere)
        try:
            self.exposure_scale.configure(state=NORMAL, takefocus=1, sliderlength=20)
        except Exception:
            pass
        self.exposure_scale.pack(fill=X, pady=(2, 0))  # Reduced padding

        # Fallback controls: +/- buttons for exposure (in case dragging is problematic)
        exp_btn_frame = Frame(exposure_frame, bg=self.DGRAY)
        exp_btn_frame.pack(fill=X, pady=(2, 0))

        def _exp_step(delta):
            """Step exposure in ms i przestaw też suwak 0-1."""
            try:
                min_ms, max_ms = 0.1, 1000.0
                cur_ms = float(self.exposure_var.get())
                new_ms = min(max_ms, max(min_ms, cur_ms + delta))
                self.exposure_var.set(new_ms)

                # Oblicz odpowiadającą pozycję suwaka (0-1) i ustaw Scale
                frac = (new_ms - min_ms) / (max_ms - min_ms)
                frac = max(0.0, min(1.0, frac))
                if hasattr(self, 'exposure_scale'):
                    self.exposure_scale.set(frac)
                if hasattr(self, '_exposure_slider_var'):
                    self._exposure_slider_var.set(frac)

                # Wywołaj tę samą logikę co przy ruchu suwaka
                self._apply_exposure_ms(new_ms)
            except Exception:
                pass

        CButton(exp_btn_frame, text="-", command=lambda: _exp_step(-1.0), width=2).pack(side=LEFT, padx=(0, 4))
        CButton(exp_btn_frame, text="+", command=lambda: _exp_step(1.0), width=2).pack(side=LEFT)
        
        # Exposure value label
        self.exposure_value_label = Label(
            exposure_frame, 
            text=f"{self.exposure_var.get():.1f} ms",
            bg=self.DGRAY, fg='lightgray', font=("Arial", 8)  # Smaller font
        )
        self.exposure_value_label.pack(anchor=W)
        
        # Gain control
        gain_frame = Frame(controls_container, bg=self.DGRAY)
        gain_frame.pack(side=LEFT, fill=X, expand=True, padx=(10, 0))  # Reduced padding
        
        Label(gain_frame, text="Gain", 
              bg=self.DGRAY, fg='white', font=("Arial", 9)).pack(anchor=W)  # Smaller font
        
        # Logiczny gain (1-10)
        self.gain_var = DoubleVar(value=options.get('gain', 1.0))
        # Suwak w zakresie 0-1, przeliczany ręcznie na 1-10
        # Zainicjalizujemy go później z faktycznej wartości gain
        self._gain_slider_var = DoubleVar(value=0.0)
        self.gain_scale = Scale(
            gain_frame,
            from_=0.0,
            to=1.0,
            orient=HORIZONTAL,
            variable=self._gain_slider_var,
            resolution=0.001,
            bg=self.DGRAY,
            fg='white',
            highlightbackground=self.DGRAY,
            troughcolor='gray',
            command=self._on_gain_change
        )
        try:
            self.gain_scale.configure(state=NORMAL, takefocus=1, sliderlength=20)
        except Exception:
            pass
        self.gain_scale.pack(fill=X, pady=(2, 0))  # Reduced padding

        # Fallback controls: +/- buttons for gain
        gain_btn_frame = Frame(gain_frame, bg=self.DGRAY)
        gain_btn_frame.pack(fill=X, pady=(2, 0))

        def _gain_step(delta):
            """Step gain 1-10 i przestaw też suwak 0-1."""
            try:
                min_gain, max_gain = 1.0, 10.0
                cur = float(self.gain_var.get())
                new = min(max_gain, max(min_gain, cur + delta))
                self.gain_var.set(new)

                frac = (new - min_gain) / (max_gain - min_gain)
                frac = max(0.0, min(1.0, frac))
                if hasattr(self, 'gain_scale'):
                    self.gain_scale.set(frac)
                if hasattr(self, '_gain_slider_var'):
                    self._gain_slider_var.set(frac)

                self._apply_gain_value(new)
            except Exception:
                pass

        CButton(gain_btn_frame, text="-", command=lambda: _gain_step(-0.1), width=2).pack(side=LEFT, padx=(0, 4))
        CButton(gain_btn_frame, text="+", command=lambda: _gain_step(0.1), width=2).pack(side=LEFT)
        
        # Gain value label
        self.gain_value_label = Label(
                gain_frame, 
                text=f"{self.gain_var.get():.1f}",
                bg=self.DGRAY, fg='lightgray', font=("Arial", 8)  # Smaller font
        )
        self.gain_value_label.pack(anchor=W)

        # Zsynchronizuj pozycje suwaków z aktualnymi wartościami z opcji
        try:
            self._apply_exposure_ms(float(self.exposure_var.get()))
        except Exception:
            pass
        try:
            self._apply_gain_value(float(self.gain_var.get()))
        except Exception:
            pass

        # ---- Spectrum ROI + Auto spectrum (moved from Settings tab) ----
        spectrum_ctrl_frame = Frame(controls_frame, bg=self.DGRAY)
        spectrum_ctrl_frame.pack(fill=X, padx=15, pady=(5, 0))
        # Ustal bazowy zakres osi (długość fali 0-2048 w umownych jednostkach)
        base_min = 0.0
        base_max = 2048.0
        units_label = "λ"

        self.spectrum_range_min_var = DoubleVar(value=options.get('spectrum_range_min', base_min))
        self.spectrum_range_max_var = DoubleVar(value=options.get('spectrum_range_max', base_max))

        Label(spectrum_ctrl_frame, text="Spectrum ROI:", bg=self.DGRAY, fg='white',
            font=("Arial", 9, "bold")).grid(row=0, column=0, sticky=W, pady=2)
        Label(spectrum_ctrl_frame, text="Min:", bg=self.DGRAY, fg='white',
            font=("Arial", 9)).grid(row=0, column=1, sticky=W, padx=(10, 2))
        self.spectrum_range_min_entry = Entry(
            spectrum_ctrl_frame,
            textvariable=self.spectrum_range_min_var,
            bg=self.RGRAY,
            fg='white',
            width=8,
            insertbackground='white',  # widoczny kursor
            relief='flat',
            highlightthickness=1,
            highlightbackground=self.RGRAY,
            highlightcolor='white'
        )
        self.spectrum_range_min_entry.grid(row=0, column=2, sticky=W)
        Label(spectrum_ctrl_frame, text="Max:", bg=self.DGRAY, fg='white',
            font=("Arial", 9)).grid(row=0, column=3, sticky=W, padx=(10, 2))
        self.spectrum_range_max_entry = Entry(
            spectrum_ctrl_frame,
            textvariable=self.spectrum_range_max_var,
            bg=self.RGRAY,
            fg='white',
            width=8,
            insertbackground='white',
            relief='flat',
            highlightthickness=1,
            highlightbackground=self.RGRAY,
            highlightcolor='white'
        )
        self.spectrum_range_max_entry.grid(row=0, column=4, sticky=W)
        Label(spectrum_ctrl_frame, text=units_label,
            bg=self.DGRAY, fg='lightgray', font=("Arial", 8)).grid(row=0, column=5, sticky=W, padx=(5, 0))

        # ROI control buttons
        self.spectrum_reset_btn = CButton(
            spectrum_ctrl_frame,
            text="RESET RANGE",
            command=self._reset_spectrum_roi_settings,
            font=("Arial", 9),
            fg='white'
        )
        self.spectrum_reset_btn.grid(row=1, column=0, columnspan=2, sticky=W, pady=2)

        self.spectrum_apply_btn = CButton(
            spectrum_ctrl_frame,
            text="APPLY ROI",
            command=self._apply_spectrum_roi_settings,
            font=("Arial", 9),
            fg='yellow'
        )
        self.spectrum_apply_btn.grid(row=1, column=3, columnspan=3, sticky=E, padx=(10, 0), pady=2)
    
        # Bottom section - Spectrum Plot (larger, reaching bottom)
        spectrum_frame = Frame(main_container, bg=self.DGRAY)
        spectrum_frame.pack(fill=BOTH, expand=True, pady=(5, 0))  # Changed to expand=True to reach bottom
        spectrum_frame.pack_propagate(False)
        spectrum_frame.configure(height=350)  # Increased from 200 to 350 pixels
        
        # Create larger spectrum plot - more visible
        self.spectrum_fig, self.spectrum_ax = plt.subplots(figsize=(18, 4.5), facecolor=self.DGRAY)  # Increased from (15, 2.5)
        self.spectrum_ax.set_facecolor(self.DGRAY)
        
        # Initialize spectrum data and axes
        self._update_spectrum_axes()  # Dynamic axis setup
        # Dopasuj długość danych do aktualnej osi (może być < 2048 po ROI)
        try:
            axis_len = len(self.x_axis)
        except Exception:
            axis_len = 2048
        self.spectrum_data = np.zeros(axis_len)
        self.spectrum_line, = self.spectrum_ax.plot(self.x_axis, self.spectrum_data, color='green', linewidth=1)
        
        # Style - larger fonts for better readability (wavelength on X)
        self.spectrum_ax.set_xlabel("Wavelength (λ)", color='white', fontsize=14)
        self.spectrum_ax.set_ylabel("Intensity", color='white', fontsize=14)
        self.spectrum_ax.set_title("Spectrum vs. wavelength", color='white', fontsize=16)
        self.spectrum_ax.tick_params(colors='white', labelsize=12)
        self.spectrum_ax.grid(True, alpha=0.3, color='gray')

        # Marginesy tak, aby tytuły i opisy nie były przycięte
        try:
            self.spectrum_fig.tight_layout(rect=[0.03, 0.08, 0.98, 0.97])
        except Exception:
            try:
                self.spectrum_fig.subplots_adjust(top=0.9, bottom=0.18, left=0.08, right=0.98)
            except Exception:
                pass
        
        # Canvas
        self.spectrum_canvas = FigureCanvasTkAgg(self.spectrum_fig, master=spectrum_frame)
        self.spectrum_canvas.draw()
        self.spectrum_canvas.get_tk_widget().pack(fill=BOTH, expand=True)
        
        # Start unified update thread for both camera and spectrometer
        def update_image():
            def update_camera_display(frame):
                """Update camera display - inline function"""
                try:
                    if frame is None or frame.size == 0:
                        return
                        
                    canvas_w, canvas_h = self._camera_frame_size
                    new_w, new_h = int(self.frame.winfo_width()), int(self.frame.winfo_height())
                    
                    if new_w > 0 and new_h > 0:
                        frame_resized = cv2.resize(frame, (new_w, new_h))
                        frame_rgb = cv2.cvtColor(frame_resized, cv2.COLOR_BGR2RGB)
                        
                        # Create PIL image and PhotoImage (attach to canvas as master)
                        pil_image = Image.fromarray(frame_rgb)
                        # Attach PhotoImage to the main app (`self`) so the Tcl interpreter
                        # still owns the image even if canvases are destroyed during shutdown.
                        photo = ImageTk.PhotoImage(pil_image, master=self)
                        
                        # Update camera canvas
                        if hasattr(self, 'camera_canvas'):
                            self.camera_canvas.delete("all")
                            x_offset = (canvas_w - new_w) // 2
                            y_offset = (canvas_h - new_h) // 2
                            self.camera_canvas.create_image(x_offset, y_offset, anchor='nw', image=photo)
                            self.camera_canvas.image = photo  # Keep reference
                            # Also keep a reference on the app object to ensure long-lived ownership
                            self._camera_canvas_img = photo
                            
                        # Update status
                        if hasattr(self, 'cam_status'):
                            self.cam_status.config(text="Camera: Live feed active", fg='lightgreen')
                    
                except Exception as e:
                    print(f"Camera display error: {e}")
            
            def update_spectrum_display(frame):
                try:
                    if frame is None or frame.size == 0:
                        return
                    # Skaluj obraz tak, aby CAŁY mieścił się w canvasie (letterbox/pillarbox),
                    # z zachowaniem proporcji, bez obcinania.

                    # Rozmiar klatki z kamery
                    h, w = frame.shape[:2]
                    if h <= 0 or w <= 0:
                        return

                    # Aktualny rozmiar canvasa (fallback do zapamiętanego)
                    canvas_w = self.spectrum_image_canvas.winfo_width()
                    canvas_h = self.spectrum_image_canvas.winfo_height()
                    if canvas_w <= 1 or canvas_h <= 1:
                        canvas_w, canvas_h = self._spectrum_image_size

                    # Współczynnik skalowania – tak, żeby cały obraz się zmieścił
                    scale = 1
                    new_w = max(1, int(w * scale))
                    new_h = max(1, int(h * scale))

                    pil_image = Image.fromarray(frame.copy())
                    pil_image = pil_image.resize((new_w, new_h), Image.Resampling.LANCZOS)
                    photo = ImageTk.PhotoImage(pil_image, master=self.spectrum_image_canvas)

                    self.spectrum_image_canvas.delete("all")

                    # Wycentruj obraz w canvasie
                    x_offset = 0
                    y_offset = 0

                    self.spectrum_image_canvas.create_image(
                        x_offset, y_offset, anchor='nw', image=photo
                    )

                    self.spectrum_image_canvas_image = photo
                    self.pixelink_image_data = frame.copy()
                    self.pixelink_ready = True
                    self._set_pixelink_status("Online", 'lightgreen')

                    # Zawsze aktualizuj widmo na podstawie klatki z kamery
                    self._calculate_spectrum_from_frame(frame)
                    
                except Exception:
                    pass
            
            while not getattr(self, '_stop_threads', False):
                try:
                    # Update camera display using direct method
                    if (hasattr(self, 'camera_manager') and 
                        self.camera_manager and 
                        self.camera_manager.running):
                        
                        camera_frame = self.camera_manager.get_current_frame()
                        if camera_frame is not None:
                            # Update camera display in main thread
                            self.after_idle(lambda f=camera_frame: update_camera_display(f))
                    
                    # Update spectrometer display using direct buffer access
                    if (hasattr(self, 'spectrometer_manager') and 
                        self.spectrometer_manager and 
                        self.spectrometer_manager.running and
                        hasattr(self.spectrometer_manager, 'frame_buffer')):
                        
                        frame_buffer = self.spectrometer_manager.frame_buffer
                        if frame_buffer is not None and frame_buffer.size > 0:
                            # Update spectrum display in main thread
                            self.after_idle(lambda f=frame_buffer: update_spectrum_display(f))
                    
                    time.sleep(0.1)  # 10 FPS max for better performance
                except Exception as e:
                    print(f"Update thread error: {e}")
                    time.sleep(0.1)
        
        # Start unified thread
        threading.Thread(target=update_image, daemon=True).start()

    def _on_exposure_change(self, value):
        """Callback z suwaka ekspozycji (0-1 -> 0.1-1000 ms)."""
        try:
            raw = float(str(value).replace(',', '.'))
            raw = max(0.0, min(1.0, raw))
            min_ms, max_ms = 0.1, 1000.0
            exposure_ms = min_ms + raw * (max_ms - min_ms)
            self._apply_exposure_ms(exposure_ms)
        except Exception as e:
            print(f"Exposure slider error: {e}")

    def _on_gain_change(self, value):
        """Callback z suwaka gain (0-1 -> 1-10)."""
        try:
            raw = float(str(value).replace(',', '.'))
            raw = max(0.0, min(1.0, raw))
            min_gain, max_gain = 1.0, 10.0
            gain_value = min_gain + raw * (max_gain - min_gain)
            self._apply_gain_value(gain_value)
        except Exception as e:
            print(f"Gain slider error: {e}")

    def _apply_exposure_ms(self, exposure_ms: float):
        """Ustaw ekspozycję (GUI + options + kamera) w jednym miejscu."""
        try:
            self.exposure_var.set(exposure_ms)
            self.exposure_value_label.configure(text=f"{exposure_ms:.1f} ms")
            options['exposure_time'] = float(exposure_ms)

            # Zsynchronizuj położenie suwaka 0-1 z aktualną wartością ms
            try:
                min_ms, max_ms = 0.1, 1000.0
                frac = (float(exposure_ms) - min_ms) / (max_ms - min_ms)
                frac = max(0.0, min(1.0, frac))
                if hasattr(self, '_exposure_slider_var'):
                    self._exposure_slider_var.set(frac)
                if hasattr(self, 'exposure_scale'):
                    self.exposure_scale.set(frac)
            except Exception:
                pass

            # Zaktualizuj await tak, żeby czekać co najmniej tyle, co ekspozycja
            exposure_s = max(0.001, float(exposure_ms) / 1000.0)
            options['await'] = max(0.01, min(2.0, exposure_s + 0.1))
            self.save_options()
            # Ustaw wartość w kamerze przez SpectrometerManager
            try:
                self.spectrometer_manager.apply_exposure(exposure_ms)
            except Exception as e:
                print(f"Exposure set error: {e}")
        except Exception as e:
            print(f"Apply exposure error: {e}")

    def _apply_gain_value(self, gain_value: float):
        """Ustaw gain (GUI + options + kamera) w jednym miejscu."""
        try:
            self.gain_var.set(gain_value)
            self.gain_value_label.configure(text=f"{gain_value:.1f}")
            options['gain'] = float(gain_value)

            # Zsynchronizuj położenie suwaka 0-1 z aktualną wartością gain (1-10)
            try:
                min_gain, max_gain = 1.0, 10.0
                frac = (float(gain_value) - min_gain) / (max_gain - min_gain)
                frac = max(0.0, min(1.0, frac))
                if hasattr(self, '_gain_slider_var'):
                    self._gain_slider_var.set(frac)
                if hasattr(self, 'gain_scale'):
                    self.gain_scale.set(frac)
            except Exception:
                pass

            # Przy zmianie gain też odśwież await na podstawie aktualnej ekspozycji
            try:
                cur_exp_ms = float(self.exposure_var.get())
            except Exception:
                cur_exp_ms = float(options.get('exposure_time', 10.0))
            exposure_s = max(0.001, cur_exp_ms / 1000.0)
            options['await'] = max(0.01, min(2.0, exposure_s + 0.1))
            self.save_options()
            # Ustaw wartość w kamerze przez SpectrometerManager
            try:
                self.spectrometer_manager.apply_gain(gain_value)
            except Exception as e:
                print(f"Gain set error: {e}")
        except Exception as e:
            print(f"Apply gain error: {e}")

    def _update_start_seq_state(self):
        try:
            if self.motors_ready and self.pixelink_ready:
                self.start_seq_btn.config(state=NORMAL)
            else:
                self.start_seq_btn.config(state=DISABLED)
        except Exception:
            pass


    def _setup_results_tab(self):
        """Setup results tab"""
        # Control buttons at top
        control_frame = Frame(self.tab_results, bg=self.DGRAY)
        control_frame.pack(fill=X, padx=5, pady=5)
        
        CButton(control_frame, text="Refresh", command=self.load_measurements).pack(side=LEFT, padx=5)
        CButton(control_frame, text="Delete All", command=self.delete_all_measurements).pack(side=LEFT, padx=5)
        
        # Info label
        self.results_info = Label(
            control_frame, 
            text="Measurements: 0", 
            bg=self.DGRAY, fg='lightgray'
        )
        self.results_info.pack(side=RIGHT, padx=10)
        
        # Main frame with canvas for scrolling
        main_frame = Frame(self.tab_results, bg=self.DGRAY)
        main_frame.pack(fill=BOTH, expand=True, padx=5, pady=5)
        
        # Canvas and scrollbars
        self.results_canvas = Canvas(main_frame, bg=self.DGRAY, highlightthickness=0)
        v_scrollbar = Scrollbar(main_frame, orient=VERTICAL, command=self.results_canvas.yview)
        h_scrollbar = Scrollbar(main_frame, orient=HORIZONTAL, command=self.results_canvas.xview)
        
        # Scrollable frame inside canvas
        self.results_frame = Frame(self.results_canvas, bg=self.DGRAY)
        
        # Configure scrolling
        self.results_canvas.configure(
            yscrollcommand=v_scrollbar.set,
            xscrollcommand=h_scrollbar.set
        )
        
        # Create window in canvas
        self.canvas_frame = self.results_canvas.create_window(
            (0, 0), window=self.results_frame, anchor="nw"
        )
        
        # Pack scrollbars and canvas
        v_scrollbar.pack(side=RIGHT, fill=Y)
        h_scrollbar.pack(side=BOTTOM, fill=X)
        self.results_canvas.pack(side=LEFT, fill=BOTH, expand=True)
        
        # Bind canvas resize
        self.results_canvas.bind('<Configure>', self._on_canvas_configure)
        self.results_frame.bind('<Configure>', self._on_frame_configure)
        
        # Bind mousewheel to canvas
        self.results_canvas.bind("<MouseWheel>", self._on_mousewheel)

    def _setup_settings_tab(self):
        """Setup settings tab"""
        settings_frame = Frame(self.tab_settings, bg=self.DGRAY)
        settings_frame.pack(fill=BOTH, expand=True, padx=20, pady=20)
        
        # Movement settings
        Label(settings_frame, text="Movement Settings", font=("Arial", 14, "bold"), 
              bg=self.DGRAY, fg='white').grid(row=0, column=0, columnspan=2, pady=10)
        
        # Variables for settings (values in micrometers)
        self.step_x = IntVar(value=options.get('step_x', 20))
        self.step_y = IntVar(value=options.get('step_y', 20))
        self.scan_width = IntVar(value=options.get('width', 200))
        self.scan_height = IntVar(value=options.get('height', 200))
        self.starting_corner = StringVar(value=options.get('starting_corner', 'top-left'))
        # Lens magnification (factor by which scan width/height are scaled)
        self.lens_magnification_var = DoubleVar(value=options.get('lens_magnification', 1.0))
        
        # Settings entries
        settings_data = [
            ("Step X (μm):", self.step_x),
            ("Step Y (μm):", self.step_y),
            ("Scan Width (μm, sample plane):", self.scan_width),
            ("Scan Height (μm, sample plane):", self.scan_height),
            ("Lens Magnification (×):", self.lens_magnification_var),
        ]
        
        for i, (label, var) in enumerate(settings_data, 1):
            Label(settings_frame, text=label, bg=self.DGRAY, fg='white').grid(row=i, column=0, sticky=W, pady=5)
            e = Entry(
                settings_frame,
                textvariable=var,
                bg=self.RGRAY,
                fg='white',
                insertbackground='white',  # widoczny kursor na ciemnym tle
                relief='flat',
                highlightthickness=1,
                highlightbackground=self.RGRAY,
                highlightcolor='white'
            )
            e.grid(row=i, column=1, sticky=EW, pady=5)

        # Starting corner selection
        corner_row = len(settings_data) + 1
        Label(settings_frame, text="Starting Corner:", bg=self.DGRAY, fg='white').grid(row=corner_row, column=0, sticky=W, pady=5)
        corner_options = ['top-left', 'top-right', 'bottom-left', 'bottom-right']
        self.corner_combo = ttk.Combobox(settings_frame, textvariable=self.starting_corner, values=corner_options, state='readonly')
        self.corner_combo.grid(row=corner_row, column=1, sticky=EW, pady=5)

        # Port settings only (moved camera + calibration to Camera & Controls)
        row_base = len(settings_data) + 3
        Label(settings_frame, text="Port Settings", font=("Arial", 14, "bold"), 
              bg=self.DGRAY, fg='white').grid(row=row_base, column=0, columnspan=3, pady=10, sticky=W)

        ports = [p.device for p in serial.tools.list_ports.comports()]

        Label(settings_frame, text="Port X:", bg=self.DGRAY, fg='white').grid(row=row_base+1, column=0, sticky=W, pady=5)
        self.port_x_var = StringVar(value=options.get('port_x', 'COM5'))
        self.port_x_combo = ttk.Combobox(settings_frame, textvariable=self.port_x_var, values=ports)
        self.port_x_combo.grid(row=row_base+1, column=1, sticky=EW, pady=5)

        Label(settings_frame, text="Port Y:", bg=self.DGRAY, fg='white').grid(row=row_base+2, column=0, sticky=W, pady=5)
        self.port_y_var = StringVar(value=options.get('port_y', 'COM9'))
        self.port_y_combo = ttk.Combobox(settings_frame, textvariable=self.port_y_var, values=ports)
        self.port_y_combo.grid(row=row_base+2, column=1, sticky=EW, pady=5)
        
        CButton(settings_frame, text="Refresh Ports", command=self.refresh_ports).grid(row=row_base+1, column=2, rowspan=3, padx=10, sticky=N)
        
        # Apply button - make it more prominent (below port settings)
        apply_frame = Frame(settings_frame, bg=self.DGRAY)
        apply_frame.grid(row=row_base+4, column=0, columnspan=3, pady=20)
        
        CButton(apply_frame, text="SAVE SETTINGS", command=self.apply_settings, 
               font=("Arial", 12, "bold"), fg='yellow').pack(pady=5)
        
        settings_frame.columnconfigure(1, weight=1)

    def refresh_ports(self):
        try:
            ports = [p.device for p in serial.tools.list_ports.comports()]
            for combo in [self.port_x_combo, self.port_y_combo]:
                combo.configure(values=ports)
        except Exception:
            pass

    def _apply_spectrum_roi_settings(self):
        """Zastosuj aktualne ustawienia ROI do osi widma i zapisz w opcjach."""
        try:
            # Aktualizacja opcji
            if hasattr(self, 'spectrum_range_min_var'):
                options['spectrum_range_min'] = float(self.spectrum_range_min_var.get())
            if hasattr(self, 'spectrum_range_max_var'):
                options['spectrum_range_max'] = float(self.spectrum_range_max_var.get())

            # Przeliczenie osi i odświeżenie wykresu
            self._update_spectrum_axes()
            self.spectrum_data = np.zeros(len(self.x_axis))
            self._update_spectrum_plot()

            # Zapis do options.json
            self.save_options()
        except Exception:
            pass

    def _reset_spectrum_roi_settings(self):
        """Przywróć pełny zakres spektrum (bez dodatkowego przycinania ROI)."""
        try:
            
            self._apply_spectrum_roi_settings()
        except Exception:
            pass

    def _update_spectrum_axes(self):
        """Ustaw oś X dla widma z uwzględnieniem prostego ROI (długość fali)."""
        try:
            # Zakres długości fali (konfigurowalny w opcjach)
            wl_min = float(options.get('wavelength_min', 0.0))
            wl_max = float(options.get('wavelength_max', 2048.0))
            base_axis = np.linspace(wl_min, wl_max, 2048)

            roi_min = float(options.get('spectrum_range_min', wl_min))
            roi_max = float(options.get('spectrum_range_max', wl_max))
            if roi_min >= roi_max:
                roi_min, roi_max = base_min, base_max

            mask = (base_axis >= roi_min) & (base_axis <= roi_max)
            if not np.any(mask):
                mask = np.ones_like(base_axis, dtype=bool)

            self.spectrum_roi_indices = np.where(mask)[0]
            self.x_axis = base_axis[self.spectrum_roi_indices]

            if hasattr(self, 'spectrum_ax'):
                self.spectrum_ax.set_xlabel("Wavelength", color='white', fontsize=10)
                self.spectrum_ax.set_title(f"Spectrum (λ {roi_min:.0f}-{roi_max:.0f})",
                                           color='white', fontsize=12)
        except Exception:
            # Fallback: prosta oś w umownych jednostkach długości fali
            self.x_axis = np.linspace(0, 2048, 2048)
            self.spectrum_roi_indices = None

    def _apply_spectrum_roi(self, spectrum_array):
        """Zastosuj bieżący ROI do jednowymiarowego widma."""
        try:
            if spectrum_array is None:
                return spectrum_array
            arr = np.asarray(spectrum_array)
            if self.spectrum_roi_indices is None:
                return arr
            valid_idx = [i for i in self.spectrum_roi_indices if i < len(arr)]
            if not valid_idx:
                return arr
            return arr[valid_idx]
        except Exception:
            return spectrum_array

    def _calculate_spectrum_from_frame(self, frame):
        """Wylicz widmo 1D z klatki (średnia po osi pionowej + ROI)."""
        try:
            if frame is None or frame.size == 0:
                return

            if len(frame.shape) == 3:
                if frame.shape[2] == 3:
                    frame_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                else:
                    frame_gray = frame[:, :, 0]
            else:
                frame_gray = frame

            spectrum_profile = np.mean(frame_gray, axis=0)

            # Przeskaluj do 2048 punktów jeśli potrzeba
            if len(spectrum_profile) != 2048:
                x_old = np.linspace(0, 1, len(spectrum_profile))
                x_new = np.linspace(0, 1, 2048)
                spectrum_profile = np.interp(x_new, x_old, spectrum_profile)

            spectrum_roi = self._apply_spectrum_roi(spectrum_profile)
            self.spectrum_data = spectrum_roi
            self.after_idle(self._update_spectrum_plot)
        except Exception as e:
            print(f"Spectrum calculation error: {e}")

    def save_options(self):
        try:
            if hasattr(self, 'step_x'):
                options['step_x'] = self.step_x.get()
            if hasattr(self, 'step_y'):
                options['step_y'] = self.step_y.get()
            if hasattr(self, 'scan_width'):
                options['width'] = self.scan_width.get()
            if hasattr(self, 'scan_height'):
                options['height'] = self.scan_height.get()
            if hasattr(self, 'starting_corner'):
                options['starting_corner'] = self.starting_corner.get()
            if hasattr(self, 'lens_magnification_var'):
                options['lens_magnification'] = float(self.lens_magnification_var.get())
            
            if hasattr(self, 'port_x_var'):
                options['port_x'] = self.port_x_var.get()
            if hasattr(self, 'port_y_var'):
                options['port_y'] = self.port_y_var.get()
                
            if hasattr(self, 'sequence_sleep_var'):
                options['sequence_sleep'] = self.sequence_sleep_var.get()

            if hasattr(self, 'spectrum_range_min_var'):
                options['spectrum_range_min'] = float(self.spectrum_range_min_var.get())
            if hasattr(self, 'spectrum_range_max_var'):
                options['spectrum_range_max'] = float(self.spectrum_range_max_var.get())
            
            with open('options.json', 'w') as f:
                json.dump(options, f, indent=4)
        except Exception:
            pass

    def _setup_styles(self):
        """Setup ttk styles"""
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('TNotebook', background=self.DGRAY, borderwidth=0)
        style.configure('TNotebook.Tab', background=self.DGRAY, foreground='white')
        style.map('TNotebook.Tab', background=[('selected', self.RGRAY)])

    def _on_canvas_configure(self, event):
        """Handle canvas resize"""
        canvas_width = event.width
        self.results_canvas.itemconfig(self.canvas_frame, width=canvas_width)

    def _on_frame_configure(self, event):
        """Handle frame resize"""
        self.results_canvas.configure(scrollregion=self.results_canvas.bbox("all"))

    def _on_mousewheel(self, event):
        """Handle mouse wheel scrolling"""
        self.results_canvas.yview_scroll(int(-1*(event.delta/120)), "units")

    def console_output(self, message):
        try:
            if not hasattr(self, 'console') or not self.console.winfo_exists():
                return
            readable_time = time.strftime('%H:%M:%S', time.localtime(time.time()))
            full_message = f'{readable_time}: {message}\n'
            
            is_error = any(keyword in message.lower() for keyword in [
                'error', 'exception', 'failed', 'not connected', 'traceback', 'not detected'
            ])
            
            is_warning = any(keyword in message.lower() for keyword in [
                'warning', 'attention', 'simulation', 'approximate'
            ])
            
            tag = "error" if is_error else "warning" if is_warning else "normal"
            
            self.console.insert(END, full_message, tag)
            self.console.see(END)
        except Exception:
            pass

    def _delayed_init(self):
        init_thread = threading.Thread(target=self._background_initialization, daemon=True)
        init_thread.start()
        
        self._update_motor_status()
        
        # Initial sequence button state update
        self.after(2000, self._update_start_seq_state)
        
    def _update_motor_status(self):
        """Update motor status periodically"""
        try:
            # Check if app is being destroyed
            if not hasattr(self, 'winfo_exists'):
                return
            
            try:
                # Test if window still exists
                self.winfo_exists()
            except:
                # Window destroyed, stop updates
                return
                
            # Check if widget still exists before updating
            if not hasattr(self, 'motor_status'):
                return  # Widget doesn't exist, stop updates
                
            # Additional check if widget was destroyed
            try:
                self.motor_status.winfo_exists()
            except:
                return  # Widget destroyed, stop updates
                
            if hasattr(self, 'motor_controller') and hasattr(self, 'motor_status'):
                if self.motor_controller.connected:
                    status_text = f"Motor Status: Connected (X:{getattr(self.motor_controller, 'port_x', 'N/A')}, Y:{getattr(self.motor_controller, 'port_y', 'N/A')})"
                    color = 'lightgreen'
                else:
                    status_text = "Motor Status: Not Connected - Check COM ports in Settings"
                    color = 'orange'
                
                self.motor_status.config(text=status_text, fg=color)
            
        except Exception as e:
            # Widget likely destroyed or app shutting down, stop the update cycle
            return
        
        # Schedule next update only if app still exists and not shutting down
        try:
            if (hasattr(self, 'after') and hasattr(self, '_after_ids') and 
                not getattr(self, '_shutting_down', False)):
                after_id = self.after(3000, self._update_motor_status)
                self._after_ids.append(after_id)
        except Exception:
            # App shutting down, stop scheduling updates
            pass

    def _background_initialization(self):
        """Jednorazowa inicjalizacja: PixeLink + pomiary + stan silników."""
        try:
            motors_ok = self.motor_controller.connected

            pixelink_ok = self.spectrometer_manager.initialize()
            if pixelink_ok:
                self.spectrometer_manager.start()

            def _ui_update():
                try:
                    self.motors_ready = motors_ok
                    self.pixelink_ready = pixelink_ok

                    if pixelink_ok:
                        self._set_pixelink_status("Online", 'lightgreen')
                        self._sync_camera_controls()
                    else:
                        self._set_pixelink_status("Offline", 'red')

                    self.load_measurements()
                    self._update_start_seq_state()
                except Exception as e:
                    print(f"Init UI update error: {e}")

            self.after_idle(_ui_update)
        except Exception as e:
            print(f"Background initialization error: {e}")

    def _set_pixelink_status(self, text, fg):
        """Update both main and reconnect Pixelink status labels consistently."""
        try:
            if hasattr(self, 'pixelink_status') and self.pixelink_status is not None:
                self.pixelink_status.config(text=text, fg=fg)
            if hasattr(self, 'pixelink_reconnect_status') and self.pixelink_reconnect_status is not None:
                self.pixelink_reconnect_status.config(text=text, fg=fg)
        except Exception:
            # Ignore UI update errors (e.g. during shutdown)
            pass

    def _sync_camera_controls(self):
        """Pobierz aktualne wartości z kamery i ustaw w GUI."""
        try:
            if not self.spectrometer_manager.hCamera:
                return

            current_exposure = self.spectrometer_manager.get_exposure()
            if current_exposure is not None:
                self._apply_exposure_ms(float(current_exposure))

            current_gain = self.spectrometer_manager.get_gain()
            if current_gain is not None:
                self._apply_gain_value(float(current_gain))
        except Exception as e:
            print(f"Sync camera controls error: {e}")

    def _force_pixelink_reconnect(self):
        def reconnect_thread():
            try:
                self.after_idle(lambda: self._set_pixelink_status("Disconnecting...", 'yellow'))
                
                if self.spectrometer_manager.running:
                    self.spectrometer_manager.stop()
                    time.sleep(1)
                
                self.after_idle(lambda: self._set_pixelink_status("Reconnecting...", 'yellow'))
                
                if self.spectrometer_manager.initialize():
                    self.spectrometer_manager.start()
                    self.pixelink_ready = True
                    self.after_idle(lambda: self._set_pixelink_status("Online", 'lightgreen'))
                    # Po udanym reconnect zsynchronizuj suwaki z aktualnymi wartościami kamery
                    self.after_idle(self._sync_camera_controls)
                    self.after_idle(self._update_start_seq_state)
                else:
                    self.pixelink_ready = False
                    self.after_idle(lambda: self._set_pixelink_status("Offline", 'red'))
                    self.after_idle(self._update_start_seq_state)
                    
            except Exception:
                self.pixelink_ready = False
                self.after_idle(lambda: self._set_pixelink_status("Offline", 'red'))
                self.after_idle(self._update_start_seq_state)
        
        threading.Thread(target=reconnect_thread, daemon=True).start()


    def on_closing(self):
        """Handle window closing - cleanup and exit program"""
        try:
            print("Closing application...")
            self.cleanup()
        except Exception as e:
            print(f"Error during cleanup: {e}")
        finally:
            try:
                self.destroy()
            except:
                pass
            # Force program termination
            import sys
            sys.exit(0)
    def cleanup(self):
        """Cleanup resources (stop threads, close devices, restore stdout)."""
        try:
            self._shutting_down = True
        except Exception:
            pass

        # Stop background update loop
        try:
            self._stop_threads = True
        except Exception:
            pass

        # Cancel scheduled after() callbacks if tracked
        try:
            if hasattr(self, '_after_ids'):
                for aid in self._after_ids:
                    try:
                        self.after_cancel(aid)
                    except Exception:
                        pass
                self._after_ids.clear()
        except Exception:
            pass

        # Stop hardware
        try:
            if hasattr(self, 'spectrometer_manager') and self.spectrometer_manager:
                self.spectrometer_manager.stop()
        except Exception as e:
            print(f"Cleanup spectrometer error: {e}")

        try:
            if hasattr(self, 'motor_controller') and self.motor_controller:
                self.motor_controller.close()
        except Exception as e:
            print(f"Cleanup motor error: {e}")

        # Clear image references to avoid PhotoImage errors at shutdown
        try:
            if hasattr(self, 'camera_canvas'):
                try:
                    self.camera_canvas.delete('all')
                except Exception:
                    pass
                self._camera_canvas_img = None
        except Exception:
            pass

        try:
            if hasattr(self, 'spectrum_image_canvas'):
                try:
                    self.spectrum_image_canvas.delete('all')
                except Exception:
                    pass
                self.spectrum_image_canvas_image = None
        except Exception:
            pass

        try:
            sys.stdout = sys.__stdout__
        except Exception:
            pass

    def _update_spectrum_plot(self):
        try:
            if not hasattr(self, 'spectrum_data') or len(self.spectrum_data) == 0:
                return
                
            if hasattr(self, 'spectrum_line'):
                self.spectrum_line.set_xdata(self.x_axis)
                self.spectrum_line.set_ydata(self.spectrum_data)
                
                if hasattr(self, 'x_axis') and len(self.x_axis) > 0:
                    self.spectrum_ax.set_xlim(self.x_axis[0], self.x_axis[-1])
                
                data_max = np.max(self.spectrum_data)
                data_min = np.min(self.spectrum_data)
                
                if data_max > data_min:
                    y_range = data_max - data_min
                    y_padding = y_range * 0.1
                    self.spectrum_ax.set_ylim(data_min - y_padding, data_max + y_padding)
                elif data_max > 0:
                    self.spectrum_ax.set_ylim(0, data_max * 1.2)
                
                def safe_canvas_update():
                    try:
                        if hasattr(self, 'spectrum_canvas'):
                            self.spectrum_canvas.draw_idle()
                    except Exception:
                        pass
                
                self.after_idle(safe_canvas_update)
                
        except Exception:
            pass

    def stop_measurement_sequence(self):
        """Stop running measurement sequence"""
        if self._sequence_running:
            self._sequence_stop_requested = True
            print("Stopping measurement sequence...")
            
            # Update UI immediately
            if hasattr(self, 'start_seq_btn'):
                self.start_seq_btn.config(state=NORMAL)
            if hasattr(self, 'stop_seq_btn'):
                self.stop_seq_btn.config(state=DISABLED)
            
            # The cleanup will be handled by the sequence thread
            print("Sequence stop requested - cleanup will follow")

    def start_measurement_sequence(self):
        print("Starting measurement sequence...")
        self._start_sequence_thread()
    
    def _start_sequence_thread(self):
        """Sekwencja pomiarowa: objazd obszaru + snake pattern bez sortowania danych."""

        def sequence():
            nonlocal_filename = None
            try:
                if self._sequence_running:
                    return

                self._sequence_running = True
                self._sequence_stop_requested = False

                if hasattr(self, 'start_seq_btn'):
                    self.start_seq_btn.config(state=DISABLED)
                if hasattr(self, 'stop_seq_btn'):
                    self.stop_seq_btn.config(state=NORMAL)

                motors_connected = getattr(self.motor_controller, 'connected', False)
                pixelink_available = getattr(self, 'pixelink_ready', False)

                if not motors_connected and not pixelink_available:
                    print("ERROR: No hardware available - check connections!")
                    return

                # Katalog na dane
                folder = "measurement_data"
                os.makedirs(folder, exist_ok=True)
                filename = os.path.join(folder, f"measurement_{time.strftime('%Y%m%d_%H%M%S')}_spectra.csv")
                nonlocal_filename = filename

                step_x = max(1, int(self.step_x.get()))
                step_y = max(1, int(self.step_y.get()))
                width_x = max(1, int(self.scan_width.get()))
                height_y = max(1, int(self.scan_height.get()))

                nx = max(1, (width_x // step_x) + 1)
                ny = max(1, (height_y // step_y) + 1)
                total_points = nx * ny

                print(f"Scan grid: {nx} x {ny} = {total_points} points")

                # Ustal kierunki poziomy/pionowy względem wybranego narożnika
                corner = self.starting_corner.get() if hasattr(self, 'starting_corner') else 'top-left'
                if corner == 'top-left':
                    horiz_dir = 'r'
                    vert_dir = 'd'
                elif corner == 'top-right':
                    horiz_dir = 'l'
                    vert_dir = 'd'
                elif corner == 'bottom-left':
                    horiz_dir = 'r'
                    vert_dir = 'u'
                elif corner == 'bottom-right':
                    horiz_dir = 'l'
                    vert_dir = 'u'
                else:
                    horiz_dir = 'r'
                    vert_dir = 'd'

                horiz_dir_opposite = 'l' if horiz_dir == 'r' else 'r'

                # Przeliczenie wymiarów skanu w µm przy uwzględnieniu powiększenia
                lens_mag = 1.0
                try:
                    if hasattr(self, 'lens_magnification_var'):
                        lens_mag = max(0.1, float(self.lens_magnification_var.get()))
                except Exception:
                    lens_mag = 1.0

                scan_width_um = int(width_x * lens_mag)
                scan_height_um = int(height_y * lens_mag)
                half_w = scan_width_um // 2
                half_h = scan_height_um // 2

                # Ruch z centrum do wybranego narożnika
                if motors_connected:
                    dx_corner = 0.0
                    dy_corner = 0.0
                    if 'left' in corner:
                        dx_corner = -half_w
                    elif 'right' in corner:
                        dx_corner = half_w
                    if 'top' in corner:
                        dy_corner = half_h
                    elif 'bottom' in corner:
                        dy_corner = -half_h

                    print(f"Moving from center to corner '{corner}' by ({dx_corner} µm, {dy_corner} µm)...")
                    self.motor_controller.move(dx_um=dx_corner, dy_um=dy_corner)
                    await_delay = float(options.get('await', 0.01)) * 20
                    time.sleep(await_delay)

                    # Objazd obwodu obszaru (start i koniec w narożniku startowym)
                    print("Driving around scan area perimeter...")
                    # 1) w poziomie
                    self.motor_controller.move(dx_um=scan_width_um if horiz_dir == 'r' else -scan_width_um)
                    time.sleep(await_delay)
                    # 2) w pionie
                    self.motor_controller.move(dy_um=scan_height_um if vert_dir == 'd' else -scan_height_um)
                    time.sleep(await_delay)
                    # 3) powrót w poziomie
                    self.motor_controller.move(dx_um=-scan_width_um if horiz_dir == 'r' else scan_width_um)
                    time.sleep(await_delay)
                    # 4) powrót w pionie
                    self.motor_controller.move(dy_um=-scan_height_um if vert_dir == 'd' else scan_height_um)
                    time.sleep(await_delay)

                    # Pytanie użytkownika czy obszar się zgadza (CustomToplevel)
                    if not self._confirm_area():
                        print("Area not confirmed by user - aborting sequence.")
                        return

                start_time = time.time()
                scan_completed = False

                with open(filename, "w", newline="") as f:
                    writer = csv.writer(f)

                    point_index = 0
                    for iy in range(ny):
                        if self._sequence_stop_requested:
                            break

                        # Kierunek wiersza w snake pattern
                        row_dir = horiz_dir if (iy % 2 == 0) else horiz_dir_opposite

                        for ix in range(nx):
                            if self._sequence_stop_requested:
                                break

                            point_index += 1
                            grid_x = ix
                            grid_y = iy

                            # Widmo z aktualnej klatki Pixelink
                            frame = None
                            if hasattr(self, 'pixelink_image_data') and self.pixelink_image_data is not None:
                                frame = self.pixelink_image_data
                            elif (hasattr(self, 'spectrometer_manager') and
                                  hasattr(self.spectrometer_manager, 'frame_buffer') and
                                  self.spectrometer_manager.frame_buffer.size > 0):
                                frame = self.spectrometer_manager.frame_buffer

                            if frame is not None:
                                try:
                                    if len(frame.shape) == 2:
                                        frame_gray = frame
                                    else:
                                        frame_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                                    spectrum_profile = np.mean(frame_gray, axis=0)
                                    if len(spectrum_profile) != 2048:
                                        x_old = np.linspace(0, 1, len(spectrum_profile))
                                        x_new = np.linspace(0, 1, 2048)
                                        spectrum_profile = np.interp(x_new, x_old, spectrum_profile)
                                    spectrum = self._apply_spectrum_roi(spectrum_profile)
                                except Exception as e:
                                    print(f"Spectrum calc error: {e}")
                                    spectrum = np.zeros(2048)
                            else:
                                spectrum = np.zeros(2048)

                            # Zapis bez sortowania – dokładnie w kolejności skanowania
                            writer.writerow([grid_x, grid_y] + spectrum.tolist())

                            elapsed = time.time() - start_time
                            progress = (point_index / total_points) * 100.0
                            eta = (elapsed / point_index * (total_points - point_index)) if point_index > 0 else 0.0
                            print(f"Point {point_index}/{total_points} ({progress:.1f}%) grid=({grid_x},{grid_y}) ETA={eta:.0f}s")

                            # Czas oczekiwania zależny od ekspozycji
                            try:
                                exposure_ms = float(self.exposure_var.get()) if hasattr(self, 'exposure_var') else float(options.get('exposure_time', 10.0))
                            except Exception:
                                exposure_ms = float(options.get('exposure_time', 10.0))
                            exposure_s = exposure_ms / 1000.0
                            configured_sleep = float(options.get('sequence_sleep', 0.5))
                            actual_sleep = max(configured_sleep, exposure_s + 0.1)
                            time.sleep(actual_sleep)

                            # Ruch stolika w snake pattern (tylko jeśli jest podłączony)
                            if motors_connected:
                                is_last_col = (ix == nx - 1)
                                is_last_row = (iy == ny - 1)

                                if not is_last_col:
                                    # Ruch w poziomie zgodnie z kierunkiem dla danego wiersza
                                    dx = step_x if row_dir == 'r' else -step_x
                                    self.motor_controller.move(dx_um=dx, dy_um=0.0)
                                elif not is_last_row:
                                    # Koniec wiersza – krok w pionie do następnego wiersza
                                    dy = step_y if vert_dir == 'd' else -step_y
                                    self.motor_controller.move(dx_um=0.0, dy_um=dy)

                    scan_completed = not self._sequence_stop_requested

                    # Prosta kontrola spójności: liczba punktów powinna zgadzać się z planem
                    if scan_completed and point_index != total_points:
                        print(f"WARNING: scanned points {point_index} != planned {total_points}")

                if scan_completed:
                    total_time = time.time() - start_time
                    print("SCAN COMPLETED!")
                    print(f"Saved {total_points} measurements to: {filename}")
                    print(f"Scan time: {total_time:.1f} seconds")
                    self.after(100, self.load_measurements)
                else:
                    print("Sequence stopped by user.")

            except Exception as e:
                print(f"Sequence error: {e}")
                # Usuń niedokończony plik
                if nonlocal_filename and os.path.exists(nonlocal_filename):
                    try:
                        os.remove(nonlocal_filename)
                    except Exception:
                        pass
            finally:
                self._sequence_running = False
                self._sequence_stop_requested = False
                if hasattr(self, 'start_seq_btn'):
                    self.start_seq_btn.config(state=NORMAL)
                if hasattr(self, 'stop_seq_btn'):
                    self.stop_seq_btn.config(state=DISABLED)

        if not getattr(self.motor_controller, 'connected', False):
            print("Motor controller not connected — running scan without moves.")
        threading.Thread(target=sequence, daemon=True).start()

    def apply_settings(self):
        """Apply and save settings"""
        global options
        
        step_x = max(2, self.step_x.get())
        step_y = max(2, self.step_y.get())
        scan_width = max(2, self.scan_width.get())
        scan_height = max(2, self.scan_height.get())
        # Ensure reasonable lens magnification
        if hasattr(self, 'lens_magnification_var'):
            try:
                lens_mag = float(self.lens_magnification_var.get())
            except Exception:
                lens_mag = options.get('lens_magnification', 1.0)
            if lens_mag <= 0:
                lens_mag = 1.0
            self.lens_magnification_var.set(lens_mag)
        else:
            lens_mag = options.get('lens_magnification', 1.0)
        
        if step_x != self.step_x.get():
            self.step_x.set(step_x)
        if step_y != self.step_y.get():
            self.step_y.set(step_y)
        if scan_width != self.scan_width.get():
            self.scan_width.set(scan_width)
        if scan_height != self.scan_height.get():
            self.scan_height.set(scan_height)
        
        settings = {
            'step_x': step_x,
            'step_y': step_y,
            'width': scan_width,
            'height': scan_height,
            'starting_corner': self.starting_corner.get() if hasattr(self, 'starting_corner') else options.get('starting_corner', 'top-left'),
            'port_x': self.port_x_var.get(),
            'port_y': self.port_y_var.get(),
            'sequence_sleep': self.sequence_sleep_var.get() if hasattr(self, 'sequence_sleep_var') else options.get('sequence_sleep', 0.1),
            'lens_magnification': float(lens_mag),
            'spectrum_range_min': float(self.spectrum_range_min_var.get()) if hasattr(self, 'spectrum_range_min_var') else options.get('spectrum_range_min', 0.0),
            'spectrum_range_max': float(self.spectrum_range_max_var.get()) if hasattr(self, 'spectrum_range_max_var') else options.get('spectrum_range_max', 2048.0),
            'await': 0.01
        }
        
        try:
            with open('options.json', 'w') as f:
                json.dump(settings, f, indent=4)
            print("Settings saved successfully")
            
            options.update(settings)

            # Update spectrum axis and clear spectrum data to match new range
            try:
                self._update_spectrum_axes()
                self.spectrum_data = np.zeros(len(self.x_axis))
                self._update_spectrum_plot()
            except Exception:
                pass
            
            self.motor_controller.close()
            self.motor_controller = MotorController(
                self.port_x_var.get(),
                self.port_y_var.get()
            )
            # Force immediate motor status update
            self.after(100, self._update_motor_status)  # Szybsza aktualizacja

        except Exception as e:
            print(f"Settings save error: {e}")

    # Removed unused calibration functions
        return
        

    def load_measurements(self):
        """Load measurement files list without caching data"""
        folder = "measurement_data"
        self.measurement_files = []  # Store only filenames, not data
        if not os.path.exists(folder):
            os.makedirs(folder)
        
        # Just collect filenames - don't load data into memory
        for filename in sorted(glob.glob(os.path.join(folder, "*_spectra.csv"))):
            self.measurement_files.append(filename)
            
        self.draw_measurements()
    
    def delete_all_measurements(self):
        """Delete all measurements"""
        if not self.measurement_files:
            CustomToplevel.alert(self, "No measurements to delete", "Info")
            return
        
        result = CustomToplevel.confirm(
            self,
            f"Are you sure you want to delete all {len(self.measurement_files)} measurements?\n"
            "This action cannot be undone!",
            "Delete All Measurements"
        )
        
        if result:
            try:
                folder = "measurement_data"
                deleted_count = 0
                
                # Delete all CSV files
                for filename in glob.glob(os.path.join(folder, "*_spectra.csv")):
                    if os.path.exists(filename):
                        os.remove(filename)
                        deleted_count += 1
                
                self.measurement_files.clear()
                self.draw_measurements()
                
                CustomToplevel.alert(self, f"Deleted {deleted_count} measurements", "Success")
                
            except Exception as e:
                print(f"Error deleting measurements: {e}")
                CustomToplevel.alert(self, f"Cannot delete measurements:\n{e}", "Error")

    def delete_measurement(self, measurement_index):
        """Delete selected measurement"""
        if 0 <= measurement_index < len(self.measurement_files):
            result = CustomToplevel.confirm(
                self,
                f"Are you sure you want to delete measurement {measurement_index + 1}?\n"
                "This action cannot be undone!",
                "Delete Measurement"
            )
            
            if result:
                try:
                    file_to_delete = self.measurement_files[measurement_index]
                    os.remove(file_to_delete)
                    
                    self.measurement_files.pop(measurement_index)
                    self.draw_measurements()
                    
                except Exception as e:
                    print(f"Error deleting measurement: {e}")
                    CustomToplevel.alert(self, f"Cannot delete measurement:\n{e}", "Error")

    def draw_measurements(self):
        """Draw measurement buttons in grid layout"""
        # Clear existing buttons
        for widget in self.results_frame.winfo_children():
            widget.destroy()
        
        if not self.measurement_files:
            # Show message if no measurements
            Label(
                self.results_frame, 
                text="No measurements\nRun measurement sequence to create data",
                bg=self.DGRAY, fg='lightgray', font=("Arial", 12),
                justify=CENTER
            ).grid(row=0, column=0, padx=20, pady=20)
        else:
            # Create grid of measurement buttons
            buttons_per_row = 5  # Number of buttons per row
            
            for i, filename in enumerate(self.measurement_files):
                row = i // buttons_per_row
                col = i % buttons_per_row
                
                # Create button frame for better styling
                button_frame = Frame(self.results_frame, bg=self.DGRAY, relief='raised', bd=1)
                button_frame.grid(row=row, column=col, padx=5, pady=5, sticky='nsew')
                
                # Main button
                btn = CButton(
                    button_frame,
                    text=f"Pomiar {i+1}",
                    command=lambda idx=i: self.show_measurement_by_index(idx),
                    width=12, height=2,
                    font=("Arial", 10, "bold")
                )
                btn.pack(fill=BOTH, expand=True, padx=2, pady=2)
                
                # Info label with filename
                basename = os.path.basename(filename)
                info_label = Label(
                    button_frame,
                    text=basename.replace('_spectra.csv', ''),
                    bg=self.RGRAY, fg='lightgray',
                    font=("Arial", 8), justify=CENTER
                )
                info_label.pack(fill=X, padx=2, pady=(0, 2))
                
                # Delete button (small)
                delete_btn = Button(
                    button_frame,
                    text="×",
                    command=lambda idx=i: self.delete_measurement(idx),
                    bg='darkred', fg='white', font=("Arial", 8, "bold"),
                    width=2, height=1, bd=0
                )
                delete_btn.pack(side=RIGHT, anchor='ne', padx=2, pady=2)
            
            # Configure grid weights for proper resizing
            for i in range(buttons_per_row):
                self.results_frame.columnconfigure(i, weight=1)
        
        # Update info label
        if hasattr(self, 'results_info'):
            self.results_info.config(text=f"Pomiary: {len(self.measurement_files)}")

    def show_measurement_by_index(self, index):
        """Otwórz wybrany pomiar w oknie HeatMapWindow (z pliku CSV)."""
        try:
            if not (0 <= index < len(self.measurement_files)):
                return
            filename = self.measurement_files[index]
            try:
                HeatMapWindow(self.window, filename)
            except Exception:
                HeatMapWindow(self, filename)
        except Exception as e:
            print(f"Error showing measurement {index}: {e}")

    def _confirm_area(self):
        """Pytanie o obszar korzystające z CustomToplevel.confirm."""
        try:
            return CustomToplevel.confirm(self, "Czy obszar się zgadza?", "Potwierdź obszar")
        except Exception:
            # Awaryjnie po prostu False
            return False


if __name__ == "__main__":
    try:
        app = SpektrometerApp()
        app.mainloop()
        
    except Exception as e:
        print(f"APPLICATION ERROR: {e}")
    except KeyboardInterrupt:
        print("Application interrupted by user")
    finally:
        try:
            if 'app' in locals():
                app.cleanup()
        except Exception as e:
            print(f"Error during cleanup: {e}")