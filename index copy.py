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
from matplotlib.patches import Rectangle
from PIL import Image, ImageTk
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


class CameraManager:
    """Manages camera operations in separate thread"""
    
    def __init__(self, camera_index=1):
        self.camera_index = camera_index
        self.detector = None
        self.running = False
        self.thread = None
        self.frame = None
        self.direction = "No movement"
        
    def start(self):
        """Start camera thread"""
        if not self.running:
            self.running = True
            # Remove CAP_DSHOW for Linux compatibility
            self.detector = cv2.VideoCapture(self.camera_index)
            
            # Check if camera opened successfully
            if not self.detector.isOpened():
                print(f"Failed to open camera {self.camera_index}")
                self.detector = None
                self.running = False
                return False
                
            self.thread = threading.Thread(target=self._camera_loop, daemon=True)
            self.thread.start()
            return True
        return False
    
    def stop(self):
        """Stop camera thread"""
        self.running = False
        if self.detector:
            self.detector.release()
        if self.thread:
            self.thread.join(timeout=1.0)
    
    def _camera_loop(self):
        """Main camera loop running in thread"""
        prev_frame = None
        
        while self.running:
            try:
                # Check if detector is valid before reading
                if not self.detector or not self.detector.isOpened():
                    time.sleep(0.1)
                    continue
                    
                ret, frame = self.detector.read()
                if not ret:
                    continue
                    
                frame = cv2.flip(frame, 1)
                
                # Motion detection
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                direction = "No movement"
                
                if prev_frame is not None:
                    flow = cv2.calcOpticalFlowFarneback(
                        prev_frame, gray, None, 0.5, 3, 15, 3, 5, 1.2, 0
                    )
                    mean_flow = np.mean(flow, axis=(0, 1))
                    magnitude = np.linalg.norm(mean_flow)
                    
                    if magnitude > 0.1:
                        if abs(mean_flow[0]) > abs(mean_flow[1]):
                            direction = 'Right' if mean_flow[0] > 0 else 'Left'
                        else:
                            direction = 'Down' if mean_flow[1] > 0 else 'Up'
                
                prev_frame = gray
                
                # Add crosshair
                height, width, _ = frame.shape
                center_x, center_y = width // 2, height // 2
                cv2.line(frame, (center_x - 20, center_y), (center_x + 20, center_y), (150, 150, 150), 1)
                cv2.line(frame, (center_x, center_y - 20), (center_x, center_y + 20), (150, 150, 150), 1)
                
                # Store frame and direction directly
                self.direction = direction
                self.frame = frame
                
                time.sleep(0.033)  # ~30 FPS
                
            except Exception as e:
                print(f"Camera error: {e}")
                time.sleep(0.1)
    
    def get_current_frame(self):
        """Get the current frame from camera"""
        return self.frame
    
    def get_current_direction(self):
        """Get the current movement direction"""
        return self.direction


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
            # print(f"Pixelink initialization exception: {e}")
            print(f"   Exception type: {type(e).__name__}")
            
            # Handle specific permission errors
            if "Permission denied" in str(e) or "Access denied" in str(e):
                print("Permission Issue Detected:")
                print("   • Add user to plugdev group: sudo usermod -a -G plugdev $USER")
                print("   • Or temporarily use: sudo python script.py")
                print("   • Then logout/login to apply group changes")
            
            # Try factory settings as fallback
            try:
                if hasattr(self, 'hCamera') and self.hCamera:
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
    """Controls stepper motors"""
    
    def __init__(self, port_x='COM5', port_y='COM9'):
        self.ports = []
        self.connected = False
        self.executor = ThreadPoolExecutor(max_workers=2)
        # Motor resolution: 1 pulse = 2 micrometers
        self.MICROMETERS_PER_PULSE = 2
        
        try:
            if self._check_ports(port_x, port_y):
                self.ports = [serial.Serial(port_x), serial.Serial(port_y)]
                self.connected = True
                # Store port names for status display
                self.port_x = port_x
                self.port_y = port_y
                print("Motors connected")
                # Update status in main app if available
                if hasattr(self, '_app_ref'):
                    self._app_ref.motors_ready = True
        except Exception as e:
            print(f"Motor connection error: {e}")
    
    def micrometers_to_pulses(self, micrometers):
        """Convert micrometers to motor pulses (1 pulse = 2 μm)"""
        return max(1, int(micrometers / self.MICROMETERS_PER_PULSE))
    
    def _check_ports(self, port_x, port_y):
        """Check if ports are available"""
        available_ports = [p.device for p in serial.tools.list_ports.comports()]
        return port_x in available_ports and port_y in available_ports
    
    def move(self, direction, step=None):
        """Move motors asynchronously - step parameter is in micrometers"""
        if not self.connected:
            return
            
        if step is None:
            # Convert micrometers to pulses for default steps
            step_x_pulses = self.micrometers_to_pulses(options['step_x'])
            step_y_pulses = self.micrometers_to_pulses(options['step_y'])
        else:
            # Convert provided step (in micrometers) to pulses
            step_x_pulses = step_y_pulses = self.micrometers_to_pulses(step)
            
        def _move():
            try:
                if direction == 'r':
                    self.ports[0].write(f"M:1+P{step_x_pulses}\r\n".encode())
                    self.ports[0].write('G:\r\n'.encode())
                elif direction == 'l':
                    self.ports[0].write(f"M:1-P{step_x_pulses}\r\n".encode())
                    self.ports[0].write('G:\r\n'.encode())
                elif direction == 'u':
                    self.ports[1].write(f"M:1+P{step_y_pulses}\r\n".encode())
                    self.ports[1].write('G:\r\n'.encode())
                elif direction == 'd':
                    self.ports[1].write(f"M:1-P{step_y_pulses}\r\n".encode())
                    self.ports[1].write('G:\r\n'.encode())
                elif direction == 'o':
                    self.ports[0].write("H:1\r\n".encode())
                    self.ports[1].write("H:1\r\n".encode())
            except Exception as e:
                print(f"Motor move error: {e}")
        
        self.executor.submit(_move)
    
    def close(self):
        """Close motor connections"""
        self.executor.shutdown(wait=True)
        for port in self.ports:
            try:
                port.close()
            except:
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
        # On Windows zachowujemy dotychczasowe zachowanie,
        # na Linuksie używamy standardowego iconify, żeby dało się
        # normalnie przywrócić okno z paska/z Alt+Tab.
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
    """Custom dialog window"""
    
    def __init__(self, *args, **kwargs):
        Toplevel.__init__(self, *args, **kwargs)
        CustomWindow.__init__(self, *args, **kwargs)
        self.overrideredirect(True)
        self.config(bg=self.DGRAY, highlightthickness=0)
    def confirm(self, message):
        """Show confirmation dialog"""
        Label(self.window, text=message, bg=self.DGRAY, fg='white', font=('Arial', 10)).pack(pady=10)
        yes = CButton(self.window, text="Yes", width=10, command=lambda: True)
        no = CButton(self.window, text="No", width=10, command=lambda: False)
        if yes and not no:
            return True
        else:
            return False


class HeatMapWindow(CustomToplevel):
    """Heatmap + spectrum window (GUI tylko do podglądu danych)."""
    
    def __init__(self, parent, measurement_index, data):
        CustomToplevel.__init__(self, parent)
        self.title(f'Measurement {measurement_index}')
        
        # Rozmiar okna ~80% ekranu, wyśrodkowany
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        window_width = int(screen_width * 0.8)
        window_height = int(screen_height * 0.8)
        x = (screen_width - window_width) // 2
        y = (screen_height - window_height) // 2
        self.geometry(f'{window_width}x{window_height}+{x}+{y}')
        
        # Ensure data is a numpy array of objects and normalize spectrum lengths
        self.data = np.array(data, dtype=object)
        self.parent = parent
        self._setup_data()
        self._create_widgets()
        
    
    def _setup_data(self):
        """Przygotuj dane do wizualizacji (siatka X/Y + widma)."""
        # Osie budujemy wyłącznie na podstawie współrzędnych (x, y) z pliku
        xs = sorted({row[0] for row in self.data})
        ys = sorted({row[1] for row in self.data})
        nx, ny = len(xs), len(ys)

        # Długość widma – bierzemy maksimum, krótsze widma dopadujemy zerami
        spectrum_len = max(len(row[2]) for row in self.data) if len(self.data) > 0 else 0
        self.cube = np.zeros((nx, ny, spectrum_len), dtype=np.float32)

        # Wypełnij kostkę danymi widmowymi: [x_idx, y_idx, lambda_idx]
        for row in self.data:
            x_val, y_val, spectrum = row[0], row[1], row[2]
            x_idx = xs.index(x_val)
            y_idx = ys.index(y_val)

            spec_arr = np.asarray(spectrum, dtype=np.float32)
            if spec_arr.size >= spectrum_len:
                self.cube[x_idx, y_idx, :] = spec_arr[:spectrum_len]
            else:
                padded = np.zeros(spectrum_len, dtype=np.float32)
                padded[:spec_arr.size] = spec_arr
                self.cube[x_idx, y_idx, :] = padded

        # Zakres kolorów dla heatmapy
        if self.cube.size > 0:
            self.vmin = float(self.cube.min())
            self.vmax = float(self.cube.max())
        else:
            self.vmin, self.vmax = 0.0, 1.0

        # Oś lambdas – prosto: indeksy widma (0..N-1)
        self.lambdas = np.arange(spectrum_len, dtype=float)
        self.current_lambda = 0
        self.unit = "px"  # traktujemy indeks widma jak pozycję piksela

    def _create_widgets(self):
        """Create GUI widgets"""
        # Main control frame at top
        main_control_frame = Frame(self.window, bg=self.DGRAY)
        main_control_frame.pack(fill=X, padx=10, pady=5)
        
        # Top row controls - left side for wavelength, right side for colormap
        top_row = Frame(main_control_frame, bg=self.DGRAY)
        top_row.pack(fill=X, pady=(0, 5))
        
        # Left side - Wavelength controls
        left_frame = Frame(top_row, bg=self.DGRAY)
        left_frame.pack(side=LEFT, fill=X, expand=True)
        
        # Wavelength label and value
        Label(left_frame, text="Wavelength:", bg=self.DGRAY, fg='white', font=('Arial', 10, 'bold')).pack(side=LEFT)
        self.wavelength_label = Label(left_frame, text="", bg=self.DGRAY, fg='lightgreen', font=('Arial', 10))
        self.wavelength_label.pack(side=LEFT, padx=(5,15))
        
        # Extended wavelength slider
        self.slider = Scale(
            left_frame, from_=0, to=self.cube.shape[2] - 1,
            orient=HORIZONTAL, command=self.on_slider,
            bg=self.DGRAY, fg='lightgray', length=500,  # Increased from 300 to 500
            highlightthickness=0, troughcolor=self.RGRAY,
            showvalue=False  # Hide the numeric labels above slider
        )
        self.slider.pack(side=LEFT, fill=X, expand=True, padx=(0,20))
        
        # Right side - Colormap selection
        right_frame = Frame(top_row, bg=self.DGRAY)
        right_frame.pack(side=RIGHT)
        
        Label(right_frame, text="Color Scale:", bg=self.DGRAY, fg='white', font=('Arial', 10, 'bold')).pack(side=LEFT, padx=(0,5))
        self.colormap_var = StringVar(value='hot')
        colormap_combo = ttk.Combobox(right_frame, textvariable=self.colormap_var, 
                                     values=['hot', 'viridis', 'plasma', 'cool', 'winter', 'autumn', 'spring', 'summer', 'gray', 'jet'],
                                     state='readonly', width=12)
        colormap_combo.pack(side=LEFT, padx=5)
        colormap_combo.bind('<<ComboboxSelected>>', lambda e: self._update_plots())
        
        # Bottom row controls - calibration info
        bottom_row = Frame(main_control_frame, bg=self.DGRAY)
        bottom_row.pack(fill=X, pady=(5, 0))

        # Figure: 2D heatmap (góra) + widmo (dół)
        self.fig = plt.figure(figsize=(12, 8), facecolor=self.DGRAY)
        gs = GridSpec(2, 1, height_ratios=[2, 1])
        self.ax2d = self.fig.add_subplot(gs[0, 0])
        self.ax_spectrum = self.fig.add_subplot(gs[1, 0])
        
        # Initialize flags
        self.colorbar = None
        self._layout_set = False
        self._colorbar_created = False
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.window)
        # Pierwsze rysowanie zrobi _update_plots; tutaj tylko podpinamy widget
        self.canvas.get_tk_widget().pack(fill=BOTH, expand=True, padx=5, pady=5)
        
        # Force window update and focus
        self.update_idletasks()
        self.lift()
        self.focus_force()
        self._update_plots()
    
    def on_slider(self, val):
        """Handle slider change"""
        self.current_lambda = int(val)
        # Update wavelength label
        lambda_val = self.lambdas[self.current_lambda]
        self.wavelength_label.config(text=f"{lambda_val:.1f} {self.unit}")
        self._update_plots()
    
    def _update_plots(self):
        """Odśwież 2D heatmapę oraz średnie widmo."""
        try:
            # Get current colormap
            cmap = self.colormap_var.get()
            
            # Clear all plots
            self.ax2d.clear()
            self.ax_spectrum.clear()
            
            lambda_val = self.lambdas[self.current_lambda]
            unit = self.unit
            
            # 2D heatmap z ustalonym zakresem kolorów i stałymi osiami
            data = self.cube[:, :, self.current_lambda]
            im = self.ax2d.imshow(
                data,
                cmap=cmap,
                origin='lower',
                interpolation='nearest',
                vmin=self.vmin,
                vmax=self.vmax,
            )
            self.ax2d.set_title(f"2D Heatmap - λ={lambda_val:.1f} {unit}", color='white', fontsize=12)
            self.ax2d.set_xlabel("X Position", color='white')
            self.ax2d.set_ylabel("Y Position", color='white')
            self.ax2d.set_aspect('equal')
            self.ax2d.set_anchor('C')
            
            # Create/update colorbar for 2D heatmap (bez zmiany geometrii osi)
            if self.colorbar is None:
                # Dodajemy colorbar tylko raz – ustawia geometrię figury
                self.colorbar = self.fig.colorbar(
                    im,
                    ax=self.ax2d,
                    fraction=0.046,
                    pad=0.04,
                    shrink=0.8,
                )
                self.colorbar.ax.tick_params(colors='white')
            else:
                # Kolejne aktualizacje tylko podmieniają mapowanie kolorów
                try:
                    self.colorbar.update_normal(im)
                except Exception:
                    pass
            
            # Spectrum plot (bottom, full width) – średnie widmo po całej próbce
            mean_profile = self.cube.mean(axis=(0, 1)) if self.cube.size > 0 else np.zeros_like(self.lambdas)
            self.ax_spectrum.plot(self.lambdas, mean_profile, color='orange', linewidth=2, 
                                label="Average Spectrum", alpha=0.8)
            self.ax_spectrum.axvline(lambda_val, color='red', linestyle='--', linewidth=2, 
                                   label=f"Current λ={lambda_val:.1f} {unit}")
            self.ax_spectrum.set_title("Average Spectrum", color='white', fontsize=12)
            self.ax_spectrum.set_ylabel("Intensity", color='white')
            self.ax_spectrum.legend(facecolor=self.DGRAY, edgecolor='white', 
                                  labelcolor='white', fontsize=10)
            self.ax_spectrum.grid(True, alpha=0.3, color='gray')
            
            # Style all plots
            for ax in [self.ax2d, self.ax_spectrum]:
                ax.set_facecolor(self.DGRAY)
                ax.tick_params(colors='white')
            
            self.canvas.draw()
            
        except Exception as e:
            print(f"Error updating plots: {e}")
            import traceback
            traceback.print_exc()
            print(f"Error updating plots: {e}")
            # Try to continue with basic plot update
            try:
                self.canvas.draw()
            except:
                pass


class SpektrometerApp(CustomTk):
    """Main application class"""
    
    def __init__(self):
        super().__init__()
        self.title("Spektrometr")

        # Override Tkinter global error handler so that background
        # callbacks after window close do not try to pop up tk dialogs.
        # Zamiast komunikatu "Error in bgerror: can't invoke 'tk' command" 
        # logujemy błąd tylko w konsoli i ignorujemy go przy zamknięciu.
        try:
            def _tk_error_handler(exc, val, tb):
                msg = str(val)
                # Typowy komunikat po zniszczeniu aplikacji ignorujemy całkowicie
                if "application has been destroyed" in msg or "can't invoke \"tk\" command" in msg:
                    return
                print(f"TK CALLBACK ERROR: {exc}: {val}")
            self.report_callback_exception = _tk_error_handler
        except Exception:
            pass
        
        # Set to maximum screen size
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        self.geometry(f'{screen_width}x{screen_height}+0+0')
        
        # Store options reference for access from other components
        self.options = options
        
        # Selected camera index from options
        self.camera_index = int(options.get('camera_index', 0))

        # Initialize managers
        self.camera_manager = CameraManager(camera_index=self.camera_index)
        self.spectrometer_manager = SpectrometerManager()
        self.motor_controller = MotorController(
            options.get('port_x', 'COM10'),
            options.get('port_y', 'COM11')
        )
        # Add reference to this app for status updates
        self.motor_controller._app_ref = self
        
        # Status variables for hardware
        self.motors_ready = getattr(self.motor_controller, 'connected', False)
        self.pixelink_ready = False
        
        # Variables
        self.measurement_files = []  # Store filenames only, not data
        self.current_image = None
        self.spectrum_data = np.zeros(2048)
        # Status variables for hardware
        self.motors_ready = False
        self.pixelink_ready = False
        
        self.pixelink_image_data = None  # Store current PixeLink frame
        
        # Sequence control flags
        self._sequence_running = False
        self._sequence_stop_requested = False
        self._shutting_down = False
        
        # Default spectrum range variables
        self.xmin_var = StringVar(value=options.get('xmin', '0'))
        self.xmax_var = StringVar(value=options.get('xmax', '2048'))
        

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
        
        # Draw placeholder if no camera
        
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
        
        # Configure color tags for console
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
        
    def start_camera(self):
        if self.camera_manager and not self.camera_manager.running:
            success = self.camera_manager.start()
            if success:
                self._camera_canvas_img = None
                self.cam_status.config(text="Camera started - Live view active", fg='lightgreen')
            else:
                self.cam_status.config(text="Failed to start camera", fg='red')

    def stop_camera(self):
        if self.camera_manager and self.camera_manager.running:
            self.camera_manager.stop()
            self.cam_status.config(text="Camera stopped", fg='orange')

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
        self.exposure_var = DoubleVar(value=self.options.get('exposure_time', 10.0))
        # Suwak pracuje w zakresie 0-1 (ułamek), przeliczany ręcznie na ms
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
        self.gain_var = DoubleVar(value=self.options.get('gain', 1.0))
        # Suwak w zakresie 0-1, przeliczany ręcznie na 1-10
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

        # ---- Spectrum ROI + Auto spectrum (moved from Settings tab) ----
        spectrum_ctrl_frame = Frame(controls_frame, bg=self.DGRAY)
        spectrum_ctrl_frame.pack(fill=X, padx=15, pady=(5, 0))

        # Ustal bazowy zakres osi (nm lub piksele) na podstawie opcji
        if self.options.get('lambda_calibration_enabled', True):
                base_min = float(self.options.get('lambda_min', 400.0))
                base_max = float(self.options.get('lambda_max', 700.0))
                units_label = "nm"
        else:
                base_min = float(self.options.get('xmin', '0'))
                base_max = float(self.options.get('xmax', '2048'))
                units_label = "px"

        self.spectrum_range_min_var = DoubleVar(value=self.options.get('spectrum_range_min', base_min))
        self.spectrum_range_max_var = DoubleVar(value=self.options.get('spectrum_range_max', base_max))

        Label(spectrum_ctrl_frame, text="Spectrum ROI:", bg=self.DGRAY, fg='white',
            font=("Arial", 9, "bold")).grid(row=0, column=0, sticky=W, pady=2)
        Label(spectrum_ctrl_frame, text="Min:", bg=self.DGRAY, fg='white',
            font=("Arial", 9)).grid(row=0, column=1, sticky=W, padx=(10, 2))
        self.spectrum_range_min_entry = Entry(
            spectrum_ctrl_frame,
            textvariable=self.spectrum_range_min_var,
            bg=self.RGRAY, fg='white', width=8
        )
        self.spectrum_range_min_entry.grid(row=0, column=2, sticky=W)
        Label(spectrum_ctrl_frame, text="Max:", bg=self.DGRAY, fg='white',
            font=("Arial", 9)).grid(row=0, column=3, sticky=W, padx=(10, 2))
        self.spectrum_range_max_entry = Entry(
            spectrum_ctrl_frame,
            textvariable=self.spectrum_range_max_var,
            bg=self.RGRAY, fg='white', width=8
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
        
        # Style - larger fonts for better readability
        self.spectrum_ax.set_xlabel("Pixel", color='white', fontsize=14)  # Increased from 12
        self.spectrum_ax.set_ylabel("Intensity", color='white', fontsize=14)  # Increased from 12
        self.spectrum_ax.set_title("Spectrum", color='white', fontsize=16)  # Increased from 14
        self.spectrum_ax.tick_params(colors='white', labelsize=12)  # Increased from 10
        self.spectrum_ax.grid(True, alpha=0.3, color='gray')
        
        self.spectrum_fig.tight_layout()
        
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

    def _apply_exposure_ms(self, exposure_ms: float):
        """Zastosuj ekspozycję podaną w milisekundach (jedna logika dla suwaka i przycisków)."""
        try:
            self.exposure_value_label.configure(text=f"{exposure_ms:.1f} ms")
            # Zapis w opcjach
            self.options['exposure_time'] = float(exposure_ms)
            self.save_options()

            # Tutaj NIE przestawiamy suwaka – suwak jest tylko wejściem użytkownika.

            # Ustawienie ekspozycji w kamerze w osobnym wątku
            if hasattr(self, 'spectrometer_manager') and self.spectrometer_manager:
                def _set_exp():
                    try:
                        self.spectrometer_manager.set_exposure(exposure_ms)
                    except Exception as e:
                        print(f"Exposure set error: {e}")
                threading.Thread(target=_set_exp, daemon=True).start()
        except Exception:
            pass

    def _apply_gain_value(self, gain_value: float):
        """Zastosuj gain w jednostkach logicznych (1-10)."""
        try:
            self.gain_value_label.configure(text=f"{gain_value:.1f}")
            self.options['gain'] = float(gain_value)
            self.save_options()

            # Nie przestawiamy gain suwaka programowo – tylko wartości logiczne.

            if hasattr(self, 'spectrometer_manager') and self.spectrometer_manager:
                def _set_gain():
                    try:
                        self.spectrometer_manager.set_gain(gain_value)
                    except Exception as e:
                        print(f"Gain set error: {e}")
                threading.Thread(target=_set_gain, daemon=True).start()
        except Exception:
            pass

    def _on_exposure_change(self, value):
        """Callback z suwaka ekspozycji.

        Na Linuksie Tk potrafi zwracać zakres 0-1 niezależnie od ustawionego from_/to,
        więc traktujemy 0-1 jako ułamek zakresu i przeskalowujemy do 0.1-1000 ms.
        """
        try:
            value_str = str(value).replace(',', '.')
            raw = float(value_str)

            # Zakres docelowy w ms
            min_ms, max_ms = 0.1, 1000.0

            if 0.0 <= raw <= 1.0:
                # Linux: suwak zwraca 0-1 -> przeskaluj do min_ms-max_ms
                exposure_ms = min_ms + raw * (max_ms - min_ms)
            else:
                # Normalny przypadek: wartość już jest w ms
                exposure_ms = raw

            # Utrzymuj zmienną w logice ms
            if hasattr(self, 'exposure_var'):
                self.exposure_var.set(exposure_ms)

            self._apply_exposure_ms(exposure_ms)
        except Exception:
            pass

    def _on_gain_change(self, value):
        """Callback z suwaka gain.

        Analogicznie: jeśli suwak oddaje 0-1, to traktujemy to jako ułamek zakresu 1-10.
        """
        try:
            value_str = str(value).replace(',', '.')
            raw = float(value_str)

            min_gain, max_gain = 1.0, 10.0

            if 0.0 <= raw <= 1.0:
                gain_value = min_gain + raw * (max_gain - min_gain)
            else:
                gain_value = raw

            if hasattr(self, 'gain_var'):
                self.gain_var.set(gain_value)

            self._apply_gain_value(gain_value)
        except Exception:
            pass

    def _update_start_seq_state(self):
        try:
            if not hasattr(self, 'start_seq_btn'):
                return
                
            motors_connected = getattr(self, 'motors_ready', False)
            pixelink_connected = getattr(self, 'pixelink_ready', False)
            
            can_start = (motors_connected or pixelink_connected) and not getattr(self, '_sequence_running', False)
            
            if can_start:
                self.start_seq_btn.configure(state=NORMAL, text="START SEQUENCE", bg='#28a745', fg='white')
            else:
                if not motors_connected and not pixelink_connected:
                    text = "HARDWARE DISCONNECTED"
                elif getattr(self, '_sequence_running', False):
                    text = "SEQUENCE RUNNING"
                else:
                    text = "SEQUENCE DISABLED"
                    
                self.start_seq_btn.configure(state=DISABLED, text=text, bg='gray', fg='white')
        except Exception:
            pass


    def _setup_results_tab(self):
        """Setup results tab"""
        # Control buttons at top
        control_frame = Frame(self.tab_results, bg=self.DGRAY)
        control_frame.pack(fill=X, padx=5, pady=5)
        
        CButton(control_frame, text="Refresh", command=self.load_measurements).pack(side=LEFT, padx=5)
        CButton(control_frame, text="Export All", command=self.export_measurements).pack(side=LEFT, padx=5)
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
            Entry(settings_frame, textvariable=var, bg=self.RGRAY, fg='white').grid(row=i, column=1, sticky=EW, pady=5)

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

        # Sequence sleep setting
        Label(settings_frame, text="Sequence Sleep (s):", bg=self.DGRAY, fg='white').grid(row=row_base+3, column=0, sticky=W, pady=5)
        self.sequence_sleep_var = DoubleVar(value=options.get('sequence_sleep', 0.1))
        sequence_sleep_entry = Entry(settings_frame, textvariable=self.sequence_sleep_var, bg=self.MGRAY, fg='white')
        sequence_sleep_entry.grid(row=row_base+3, column=1, sticky=EW, pady=5)
        
        # Info about automatic timing
        timing_info = Label(settings_frame, 
                           text="Auto-adjusted based on camera exposure time", 
                           bg=self.DGRAY, fg='lightgray', font=("Arial", 8))
        timing_info.grid(row=row_base+4, column=0, columnspan=2, sticky=W, pady=2)

        CButton(settings_frame, text="Refresh Ports", command=self.refresh_ports).grid(row=row_base+1, column=2, rowspan=3, padx=10, sticky=N)

        # Camera settings
        cam_row = row_base + 5  # Updated due to added timing info row
        Label(settings_frame, text="Camera Settings", font=("Arial", 14, "bold"), 
              bg=self.DGRAY, fg='white').grid(row=cam_row, column=0, columnspan=3, pady=10, sticky=W)
        
        Label(settings_frame, text="Camera Index:", bg=self.DGRAY, fg='white').grid(row=cam_row+1, column=0, sticky=W, pady=5)
        self.camera_index_var = IntVar(value=options.get('camera_index', 0))
        cams = [0, 1, 2]  # self._list_cameras() # REMOVED TEMPORARILY
        self.camera_combo = ttk.Combobox(settings_frame, values=cams, state='readonly', width=10)
        try:
            self.camera_combo.set(str(self.camera_index_var.get()) if self.camera_index_var.get() in cams else str(cams[0]))
        except Exception:
            pass
        self.camera_combo.grid(row=cam_row+1, column=1, sticky=EW, pady=5)
        CButton(settings_frame, text="Refresh", command=lambda: print("Camera refresh disabled")).grid(row=cam_row+1, column=2, padx=10)
        
        # Wavelength calibration and spectrum settings UI zostały przeniesione / uproszczone
        # (lambda i ROI są teraz konfigurowane w zakładce Spectrum).

        # Apply button - make it more prominent
        apply_frame = Frame(settings_frame, bg=self.DGRAY)
        apply_frame.grid(row=cam_row+3, column=0, columnspan=3, pady=20)
        
        CButton(apply_frame, text="SAVE SETTINGS", command=self.apply_settings, 
               font=("Arial", 12, "bold"), fg='yellow').pack(pady=5)
        
        Label(apply_frame, text="Click to save all changes to options.json", 
              bg=self.DGRAY, fg='lightgray', font=("Arial", 9)).pack()
        
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
                self.options['spectrum_range_min'] = float(self.spectrum_range_min_var.get())
            if hasattr(self, 'spectrum_range_max_var'):
                self.options['spectrum_range_max'] = float(self.spectrum_range_max_var.get())

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
            if self.options.get('lambda_calibration_enabled', False):
                base_min = float(self.options.get('lambda_min', 0.0))
                base_max = float(self.options.get('lambda_max', 2048.0))
            else:
                base_min = float(self.options.get('xmin', 0.0))
                base_max = float(self.options.get('xmax', 2048.0))

            if hasattr(self, 'spectrum_range_min_var'):
                self.spectrum_range_min_var.set(base_min)
            if hasattr(self, 'spectrum_range_max_var'):
                self.spectrum_range_max_var.set(base_max)

            # Zastosuj od razu nowy (pełny) zakres
            self._apply_spectrum_roi_settings()
        except Exception:
            pass

    def save_options(self):
        try:
            if hasattr(self, 'step_x'):
                self.options['step_x'] = self.step_x.get()
            if hasattr(self, 'step_y'):
                self.options['step_y'] = self.step_y.get()
            if hasattr(self, 'scan_width'):
                self.options['width'] = self.scan_width.get()
            if hasattr(self, 'scan_height'):
                self.options['height'] = self.scan_height.get()
            if hasattr(self, 'starting_corner'):
                self.options['starting_corner'] = self.starting_corner.get()
            if hasattr(self, 'lens_magnification_var'):
                self.options['lens_magnification'] = float(self.lens_magnification_var.get())
            
            if hasattr(self, 'port_x_var'):
                self.options['port_x'] = self.port_x_var.get()
            if hasattr(self, 'port_y_var'):
                self.options['port_y'] = self.port_y_var.get()
                
            if hasattr(self, 'sequence_sleep_var'):
                self.options['sequence_sleep'] = self.sequence_sleep_var.get()

            if hasattr(self, 'spectrum_range_min_var'):
                self.options['spectrum_range_min'] = float(self.spectrum_range_min_var.get())
            if hasattr(self, 'spectrum_range_max_var'):
                self.options['spectrum_range_max'] = float(self.spectrum_range_max_var.get())
            
            with open('options.json', 'w') as f:
                json.dump(self.options, f, indent=4)
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
        try:
            if self.spectrometer_manager.initialize():
                self.spectrometer_manager.start()
                self.after_idle(self._sync_camera_controls)
            
            self.load_measurements()
            self._force_pixelink_init()
            
        except Exception:
            pass

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
        try:
            if hasattr(self, 'spectrometer_manager') and self.spectrometer_manager and self.spectrometer_manager.hCamera:
                current_exposure = self.spectrometer_manager.get_exposure()
                if current_exposure is not None:
                    # get_exposure already returns milliseconds – keep same scale
                    exposure_ms = float(current_exposure)
                    if hasattr(self, 'exposure_var'):
                        self.exposure_var.set(exposure_ms)
                    # Zastosuj pełną logikę (label, opcje, suwak)
                    self._apply_exposure_ms(exposure_ms)
                
                current_gain = self.spectrometer_manager.get_gain()
                if current_gain is not None:
                    if hasattr(self, 'gain_var'):
                        self.gain_var.set(current_gain)
                    self._apply_gain_value(float(current_gain))
                        
        except Exception:
            pass

    def _force_pixelink_init(self):
        try:
            if self.spectrometer_manager.initialize():
                self.pixelink_ready = True
                self._set_pixelink_status("Ready", 'orange')
                self.after_idle(self._auto_start_pixelink)
                self.after_idle(self._update_start_seq_state)
                return True
            else:
                self.pixelink_ready = False
                self._set_pixelink_status("Offline", 'red')
                self.after_idle(self._update_start_seq_state)
                return False
        except Exception:
            self.pixelink_ready = False
            self._set_pixelink_status("Offline", 'red')
            self.after_idle(self._update_start_seq_state)
            return False
        
    def _auto_start_pixelink(self):
        try:
            self.start_pixelink()
        except Exception:
            pass

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
        """Cleanup resources"""
        # Mark app as shutting down
        self._shutting_down = True
        
        # Cancel any pending after() calls to prevent "invalid command name" errors
        try:
            # Cancel all tracked after IDs
            if hasattr(self, '_after_ids'):
                for after_id in self._after_ids:
                    try:
                        self.after_cancel(after_id)
                    except:
                        pass
                self._after_ids.clear()
        except:
            pass
            
        # Minimal cleanup logging (console widget will show details)
        try:
            if hasattr(self, 'console_output'):
                self.console_output('Cleaning up resources...')
        except Exception:
            pass
        self._stop_threads = True
        try:
            if hasattr(self, 'camera_manager'):
                self.camera_manager.stop()
            if hasattr(self, 'spectrometer_manager'):
                self.spectrometer_manager.stop()
            if hasattr(self, 'motor_controller'):
                self.motor_controller.close()
        except Exception as e:
            try:
                self.console_output(f"Cleanup error: {e}")
            except Exception:
                print(f"Cleanup error: {e}")
        # Clear image references to avoid PhotoImage __del__ issues during interpreter shutdown
        try:
            # Remove any canvas contents and drop references to PhotoImage objects
            try:
                if hasattr(self, 'camera_canvas'):
                    try:
                        self.camera_canvas.delete('all')
                    except Exception:
                        pass
                    # Remove any stored reference
                    self._camera_canvas_img = None
                    try:
                        self.camera_canvas.image = None
                    except Exception:
                        pass
            except Exception:
                pass
        except Exception:
            pass

        try:
            try:
                if hasattr(self, 'spectrum_image_canvas'):
                    try:
                        self.spectrum_image_canvas.delete('all')
                    except Exception:
                        pass
                    # Remove stored reference
                    self.spectrum_image_canvas_image = None
            except Exception:
                pass
        except Exception:
            pass
        # Restore stdout
        try:
            sys.stdout = sys.__stdout__
        except Exception:
            pass
        # Attempt to update UI and allow process to exit cleanly
        try:
            self.update_idletasks()
        except Exception:
            pass

    def _calculate_spectrum_from_frame(self, frame):
        try:
            if frame is None or frame.size == 0:
                return
                
            if len(frame.shape) == 3:
                frame_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY) if frame.shape[2] == 3 else frame[:,:,0]
            else:
                frame_gray = frame
                
            spectrum_profile = np.mean(frame_gray, axis=0)
            
            if len(spectrum_profile) != 2048:
                x_old = np.linspace(0, 1, len(spectrum_profile))
                x_new = np.linspace(0, 1, 2048)
                spectrum_profile = np.interp(x_new, x_old, spectrum_profile)
            
            # Apply configured spectrum range (ROI)
            spectrum_profile = self._apply_spectrum_roi(spectrum_profile)
            self.spectrum_data = spectrum_profile
            self.after_idle(self._update_spectrum_plot)
                
        except Exception:
            pass

    def _apply_spectrum_roi(self, spectrum_array):
        """Apply current spectrum ROI to a 1D spectrum array."""
        try:
            if spectrum_array is None:
                return spectrum_array
            spectrum_array = np.asarray(spectrum_array)

            if not hasattr(self, 'spectrum_roi_indices') or self.spectrum_roi_indices is None:
                return spectrum_array

            if len(self.spectrum_roi_indices) == len(spectrum_array):
                return spectrum_array[self.spectrum_roi_indices]

            # Fallback if spectrum length changed
            valid_indices = [i for i in self.spectrum_roi_indices if i < len(spectrum_array)]
            if not valid_indices:
                return spectrum_array
            return spectrum_array[valid_indices]
        except Exception:
            return spectrum_array

    def _update_spectrum_axes(self):
        try:
            calibrated = (hasattr(self, 'options') and 
                          self.options.get('lambda_calibration_enabled', False) and
                          'lambda_min' in self.options and 'lambda_max' in self.options)

            if calibrated:
                base_min = float(self.options['lambda_min'])
                base_max = float(self.options['lambda_max'])
                base_axis = np.linspace(base_min, base_max, 2048)
                xlabel = "Wavelength (nm)"
                default_title = f"Spectrum ({base_min:.0f}-{base_max:.0f} nm)"
            else:
                base_min, base_max = 0.0, 2048.0
                base_axis = np.linspace(base_min, base_max, 2048)
                xlabel = "Pixel"
                default_title = "Spectrum"

            roi_min = float(self.options.get('spectrum_range_min', base_min)) if hasattr(self, 'options') else base_min
            roi_max = float(self.options.get('spectrum_range_max', base_max)) if hasattr(self, 'options') else base_max
            if roi_min >= roi_max:
                roi_min, roi_max = base_min, base_max

            mask = (base_axis >= roi_min) & (base_axis <= roi_max)
            if not np.any(mask):
                mask = np.ones_like(base_axis, dtype=bool)

            self.spectrum_roi_indices = np.where(mask)[0]
            self.x_axis = base_axis[self.spectrum_roi_indices]

            if calibrated:
                if roi_min == base_min and roi_max == base_max:
                    title = default_title
                else:
                    title = f"Spectrum ({roi_min:.0f}-{roi_max:.0f} nm)"
            else:
                if roi_min == base_min and roi_max == base_max:
                    title = default_title
                else:
                    title = f"Spectrum (pixels {roi_min:.0f}-{roi_max:.0f})"
            
            if hasattr(self, 'spectrum_ax'):
                self.spectrum_ax.set_xlabel(xlabel, color='white', fontsize=10)
                self.spectrum_ax.set_title(title, color='white', fontsize=12)
                
        except Exception:
            self.x_axis = np.linspace(0, 2048, 2048)
            self.spectrum_roi_indices = None

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

    def init_pixelink(self):
        try:
            self._set_pixelink_status("Connecting...", 'orange')
            
            if self.spectrometer_manager.initialize():
                self.pixelink_ready = True
                self._set_pixelink_status("Initializing...", 'orange')
                self._update_start_seq_state()
                return True
            else:
                self.pixelink_ready = False
                self._set_pixelink_status("Offline", 'red')
                self._update_start_seq_state()
                return False
        except Exception:
            self.pixelink_ready = False
            self._set_pixelink_status("Offline", 'red')
            self._update_start_seq_state()
            return False

    def start_pixelink(self):
        try:
            if hasattr(self.spectrometer_manager, 'hCamera') and self.spectrometer_manager.hCamera:
                self.spectrometer_manager.start()
                self._set_pixelink_status("Online", 'lightgreen')
            else:
                self._set_pixelink_status("Starting...", 'orange')
        except Exception:
            self._set_pixelink_status("Offline", 'red')

    def stop_pixelink(self):
        try:
            self.spectrometer_manager.stop()
            self._set_pixelink_status("Stopped", 'yellow')
        except Exception:
            self._set_pixelink_status("Offline", 'red')

                
        except Exception as e:
            print(f"Pixelink display update error: {e}")

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
        
        # Check hardware availability using status variables
        motors_available = getattr(self, 'motors_ready', False)
        pixelink_available = getattr(self, 'pixelink_ready', False)
        
        # Sequence requires at least one system working
        if not motors_available and not pixelink_available:
            messagebox.showerror(
                "Sequence Error", 
                "Cannot start measurement sequence:\n\n"
                "No hardware available.\n\n"
                "Please connect motors or PixeLink camera."
            )
            return
        
        # Zablokuj edycję ROI na czas sekwencji, żeby format danych był stały
        try:
            if hasattr(self, 'spectrum_range_min_entry'):
                self.spectrum_range_min_entry.config(state=DISABLED)
            if hasattr(self, 'spectrum_range_max_entry'):
                self.spectrum_range_max_entry.config(state=DISABLED)
            if hasattr(self, 'spectrum_reset_btn'):
                self.spectrum_reset_btn.config(state=DISABLED)
            if hasattr(self, 'spectrum_apply_btn'):
                self.spectrum_apply_btn.config(state=DISABLED)
        except Exception:
            pass

        # Start sequence immediately - the sequence logic will handle missing components
        self._start_sequence_thread()
    
    def _start_sequence_thread(self):
        """Start the actual measurement sequence in thread"""
        
        def sequence():
            try:
                self._sequence_running = True
                self._sequence_stop_requested = False
                
                if hasattr(self, 'start_seq_btn'):
                    self.start_seq_btn.config(state=DISABLED)
                if hasattr(self, 'stop_seq_btn'):
                    self.stop_seq_btn.config(state=NORMAL)
                
                motor_connected = self.motor_controller.connected if hasattr(self, 'motor_controller') else False
                if not motor_connected:
                    print("WARNING: Motors not connected - running in simulation mode")
                    
                # Check hardware status using status variables
                motors_available = getattr(self, 'motors_ready', False)
                pixelink_available = getattr(self, 'pixelink_ready', False)
                
                if not motors_available and not pixelink_available:
                    print("ERROR: No hardware available - check connections!")
                    return
                
                if not motors_available:
                    # print("INFO: Motors not connected - PixeLink only mode")
                    pass
                elif not pixelink_available:
                    print("WARNING: PixeLink not ready - Motors only mode")
                else:
                    print("✅ Both motors and PixeLink ready for sequence")

                pos_x = 0
                pos_y = 0

                def move_motor_tracked(direction, distance_um):
                    """Move motors and track relative position from center (only if connected)."""
                    nonlocal pos_x, pos_y
                    if not motor_connected:
                        return
                    try:
                        self.motor_controller.move(direction, distance_um)
                        if direction == 'r':
                            pos_x += distance_um
                        elif direction == 'l':
                            pos_x -= distance_um
                        elif direction == 'd':
                            pos_y += distance_um
                        elif direction == 'u':
                            pos_y -= distance_um
                    except Exception as e:
                        print(f"Motor move error (tracked): {e}")

                def return_to_center():
                    """Return stage to center based on tracked position."""
                    nonlocal pos_x, pos_y
                    if not motor_connected:
                        return
                    try:
                        # First correct X
                        if pos_x > 0:
                            self.motor_controller.move('l', pos_x)
                        elif pos_x < 0:
                            self.motor_controller.move('r', -pos_x)
                        # Then correct Y
                        if pos_y > 0:
                            self.motor_controller.move('u', pos_y)
                        elif pos_y < 0:
                            self.motor_controller.move('d', -pos_y)
                    except Exception as e:
                        print(f"Return to center error: {e}")
                    finally:
                        pos_x = 0
                        pos_y = 0

                # Create data folder
                folder = "measurement_data"
                os.makedirs(folder, exist_ok=True)
                filename = os.path.join(folder, f"measurement_{time.strftime('%Y%m%d_%H%M%S')}_spectra.csv")
                    
                step_x = self.step_x.get()
                step_y = self.step_y.get()
                width_x = self.scan_width.get()
                height_y = self.scan_height.get()
                
                # Calculate number of scan points
                nx = max(1, (width_x // step_x) + 1)
                ny = max(1, (height_y // step_y) + 1)
                total_points = nx * ny
                
                # Initialize progress tracking
                start_time = time.time()
                scan_completed = False
                lens_mag = float(self.lens_magnification_var.get())
                
                with open(filename, "w", newline="") as f:
                    writer = csv.writer(f)

                    scan_width = int(width_x * lens_mag)
                    scan_height = int(height_y * lens_mag)
                
                    starting_corner = self.starting_corner.get()
                    
                    offset_x = -scan_width // 2
                    offset_y = -scan_height // 2
                    
                    if starting_corner == 'top-right':
                        offset_x = scan_width // 2
                        offset_y = -scan_height // 2
                    elif starting_corner == 'bottom-left':
                        offset_x = -scan_width // 2
                        offset_y = scan_height // 2
                    elif starting_corner == 'bottom-right':
                        offset_x = scan_width // 2
                        offset_y = scan_height // 2
                    
                    # Helper: perform a preview perimeter pass (end at starting corner)
                    def preview_perimeter():
                        try:
                            print("🔁 Performing perimeter pass around scan area...")

                            # 1) Move from current center to selected starting corner of scan area
                            if offset_x != 0:
                                dir_x = 'l' if offset_x < 0 else 'r'
                                move_motor_tracked(dir_x, abs(offset_x))
                            if offset_y != 0:
                                dir_y = 'u' if offset_y < 0 else 'd'
                                move_motor_tracked(dir_y, abs(offset_y))

                            # Allow motors to finish initial move
                            time.sleep(1)

                            # 2) Perimeter pass: drive around the edges of the scan area once
                            perim_moves = []
                            if starting_corner == 'top-left':
                                perim_moves = [('r', scan_width), ('d', scan_height), ('l', scan_width), ('u', scan_height)]
                            elif starting_corner == 'top-right':
                                perim_moves = [('l', scan_width), ('d', scan_height), ('r', scan_width), ('u', scan_height)]
                            elif starting_corner == 'bottom-left':
                                perim_moves = [('r', scan_width), ('u', scan_height), ('l', scan_width), ('d', scan_height)]
                            elif starting_corner == 'bottom-right':
                                perim_moves = [('l', scan_width), ('u', scan_height), ('r', scan_width), ('d', scan_height)]

                            # Execute perimeter moves with small pauses for stabilization
                            for d, s in perim_moves:
                                print(f"➡️ Perimeter move: {d} {s} μm")
                                try:
                                    move_motor_tracked(d, s)
                                except Exception as _e:
                                    print(f"Perimeter move failed: {_e}")
                                # small delay to allow mechanical movement (may be adjusted)
                                time.sleep(0.2)
                        except Exception as e:
                            print(f"Perimeter pass error: {e}")

                    # Perform preview perimeter pass if motor controller is connected
                    # (use low-level connection flag, not the higher-level status).
                    if motor_connected:
                        preview_perimeter()

                    # After perimeter pass, confirm that the scanned area is correct
                    # We must show the dialog in the main Tk thread.
                    from threading import Event
                    confirm_event = Event()
                    confirm_result = {'ok': False}

                    def ask_confirm():
                        try:
                            # Bring main window to front so the confirm dialog is visible
                            try:
                                self.lift()
                                self.focus_force()
                            except Exception:
                                pass
                            # Use existing helper that already handles CustomWindow/messagebox
                            result = self._confirm_area()
                        except Exception:
                            result = False
                        confirm_result['ok'] = bool(result)
                        confirm_event.set()

                    # Schedule confirmation dialog in UI thread and wait here
                    self.after(0, ask_confirm)
                    confirm_event.wait()

                    if not confirm_result['ok']:
                        print("Sequence cancelled by user after area preview. Returning to center...")
                        # User rejected area - explicitly return to center
                        if motor_connected:
                            return_to_center()
                        return

                    # User confirmed area: we are already at the starting corner
                    # (if motors are connected), so we can start the scan immediately.
                    if motor_connected:
                        # Small pause to let mechanics settle before measurements
                        time.sleep(1)

                    point_index = 0

                    # Main scanning loop (snake pattern): POMIAR -> CZEKANIE -> KROK
                    for iy in range(ny):
                        # Check for stop request
                        if self._sequence_stop_requested:
                            break

                        # Czy startujemy od lewej strony próbki?
                        left_side_start = starting_corner in ['top-left', 'bottom-left']

                        # Ustal kierunek przebiegu wiersza (snake)
                        if iy % 2 == 0:
                            # Parzysty wiersz: bazowy kierunek
                            if left_side_start:
                                x_range = range(nx)
                            else:
                                x_range = range(nx - 1, -1, -1)
                        else:
                            # Nieparzysty wiersz: odwrócony kierunek
                            if left_side_start:
                                x_range = range(nx - 1, -1, -1)
                            else:
                                x_range = range(nx)

                        last_ix_in_row = x_range[-1]

                        for ix in x_range:
                            # Check for stop request
                            if self._sequence_stop_requested:
                                break

                            point_index += 1

                            # Oblicz fizyczny indeks kolumny/rzędu niezależny
                            # od kierunku snake i wybranego narożnika.
                            if starting_corner in ['top-left', 'bottom-left']:
                                # Lewe narożniki: X rośnie od lewej do prawej
                                if iy % 2 == 0:
                                    phys_x = ix
                                else:
                                    phys_x = (nx - 1) - ix
                            else:
                                # Prawe narożniki: X rośnie też od lewej do prawej
                                if iy % 2 == 0:
                                    phys_x = (nx - 1) - ix
                                else:
                                    phys_x = ix

                            if starting_corner in ['top-left', 'top-right']:
                                # Górne narożniki: Y rośnie z góry na dół
                                phys_y = iy
                            else:
                                # Dolne narożniki: odwróć oś Y tak, aby 0 było u góry
                                phys_y = (ny - 1) - iy

                            grid_x = int(phys_x)
                            grid_y = int(phys_y)
                            
                            # Acquire fresh spectrum data from current camera frame
                            if hasattr(self, 'pixelink_image_data') and self.pixelink_image_data is not None:
                                # Calculate spectrum from current frame
                                frame = self.pixelink_image_data
                                if len(frame.shape) == 3:  # Color image
                                    frame_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                                else:
                                    frame_gray = frame
                                roi_min = int(self.options.get('spectrum_range_min', 0))
                                roi_max = int(self.options.get('spectrum_range_max', frame_gray.shape[1]-1))
                                # Calculate spectrum by averaging vertically (horizontal profile)
                                spectrum_profile = np.mean(frame_gray, axis=0)
                                spectrum = spectrum_profile[roi_min:roi_max+1]

                            # Save measurement data with grid coordinates (x_pixel, y_pixel, spectrum_values)
                            writer.writerow([grid_x, grid_y] + spectrum.tolist())
                            
                            # Progress update
                            elapsed = time.time() - start_time
                            progress = (point_index / total_points) * 100
                            eta = (elapsed / point_index * (total_points - point_index)) if point_index > 0 else 0

                            print(f"📊 Punkt {point_index}/{total_points} ({progress:.1f}%) - "
                                f"Siatka: ({grid_x}, {grid_y}) μm - ETA: {eta:.0f}s")
                            
                            # Smart delay based on camera frame rate and exposure time
                            # Get current exposure time from UI or options
                            if hasattr(self, 'exposure_var'):
                                exposure_time_ms = float(self.exposure_var.get())
                            else:
                                exposure_time_ms = float(self.options.get('exposure_time', 10.0))
                            
                            exposure_time_s = exposure_time_ms / 1000.0  # Convert ms to seconds
                            
                            # Frame rate is limited by exposure time + readout time
                            # Add buffer for camera processing and readout (typically ~50-100ms)
                            min_frame_time = exposure_time_s + 0.1  # exposure + 100ms buffer
                            
                            # Use configured sequence_sleep but ensure it's not less than frame time
                            configured_sleep = float(self.options.get('sequence_sleep', 0.5))
                            actual_sleep = max(configured_sleep, min_frame_time)

                            # Najpierw odczekaj w aktualnym punkcie (pomiar przy
                            # nieruchomym stoliku), a dopiero potem wykonaj ruch.
                            print(f"🕒 Wait: {actual_sleep:.2f}s (exposure: {exposure_time_ms}ms + buffer)")
                            time.sleep(actual_sleep)

                            # RUCH: przejście do kolejnego punktu siatki
                            if ix != last_ix_in_row:
                                # Ruch w poziomie w obrębie tego samego wiersza
                                if starting_corner in ['top-left', 'bottom-left']:
                                    if iy % 2 == 0:
                                        move_motor_tracked('r', step_x)  # Even row: right
                                    else:
                                        move_motor_tracked('l', step_x)  # Odd row: left
                                else:
                                    if iy % 2 == 0:
                                        move_motor_tracked('l', step_x)  # Even row: left
                                    else:
                                        move_motor_tracked('r', step_x)  # Odd row: right
                            elif iy != ny - 1:
                                # Koniec wiersza, przejście w pionie do kolejnego
                                if starting_corner in ['top-left', 'top-right']:
                                    move_motor_tracked('d', step_y)
                                else:
                                    move_motor_tracked('u', step_y)

                            # Krótka pauza na ustabilizowanie po ruchu
                            time.sleep(0.01)
                
                # If we reached this point and no stop was requested, the scan finished
                if not self._sequence_stop_requested:
                    total_time = time.time() - start_time
                    print("SCAN COMPLETED!")
                    print(f"Saved {total_points} measurements to: {filename}")
                    print(f"Scan time: {total_time:.1f} seconds")
                    scan_completed = True
                    self.after(100, self.load_measurements)
                
            except Exception as e:
                print(f"Sequence error: {e}")
            finally:
                # Always try to return to the center of the scan area
                try:
                    print("🔙 Returning to center position...")
                    return_to_center()
                    print("✅ Returned to center")
                except Exception as e:
                    print(f"Error while returning to center: {e}")

                # If scan was interrupted, delete the incomplete file
                if not scan_completed and 'filename' in locals():
                    try:
                        if os.path.exists(filename):
                            os.remove(filename)
                    except:
                        pass
                
                # Reset sequence flags and button states
                self._sequence_running = False
                self._sequence_stop_requested = False
                if hasattr(self, 'start_seq_btn'):
                    self.start_seq_btn.config(state=NORMAL)
                if hasattr(self, 'stop_seq_btn'):
                    self.stop_seq_btn.config(state=DISABLED)

                # Odblokuj kontrolki ROI po zakończeniu sekwencji
                try:
                    if hasattr(self, 'spectrum_range_min_entry'):
                        self.spectrum_range_min_entry.config(state=NORMAL)
                    if hasattr(self, 'spectrum_range_max_entry'):
                        self.spectrum_range_max_entry.config(state=NORMAL)
                    if hasattr(self, 'spectrum_reset_btn'):
                        self.spectrum_reset_btn.config(state=NORMAL)
                    if hasattr(self, 'spectrum_apply_btn'):
                        self.spectrum_apply_btn.config(state=NORMAL)
                except Exception:
                    pass
        
        # Run sequence in separate thread regardless of motor connection
        if not self.motor_controller.connected:
            print("Motor controller not connected — running in simulation (no moves).")
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
            'xmin': self.xmin_var.get(),
            'xmax': self.xmax_var.get(),
            'port_x': self.port_x_var.get(),
            'port_y': self.port_y_var.get(),
            'sequence_sleep': self.sequence_sleep_var.get() if hasattr(self, 'sequence_sleep_var') else options.get('sequence_sleep', 0.1),
            'camera_index': int(self.camera_combo.get()) if hasattr(self, 'camera_combo') and self.camera_combo.get() != '' and self.camera_combo.get().isdigit() else options.get('camera_index', 0),
            'lambda_min': float(self.lambda_min_var.get()) if hasattr(self, 'lambda_min_var') else options.get('lambda_min', 400.0),
            'lambda_max': float(self.lambda_max_var.get()) if hasattr(self, 'lambda_max_var') else options.get('lambda_max', 700.0),
            'lambda_calibration_enabled': bool(self.lambda_cal_enabled_var.get()) if hasattr(self, 'lambda_cal_enabled_var') else options.get('lambda_calibration_enabled', True),
            'lens_magnification': float(lens_mag),
            'spectrum_range_min': float(self.spectrum_range_min_var.get()) if hasattr(self, 'spectrum_range_min_var') else options.get('spectrum_range_min', options.get('lambda_min', 0.0)),
            'spectrum_range_max': float(self.spectrum_range_max_var.get()) if hasattr(self, 'spectrum_range_max_var') else options.get('spectrum_range_max', options.get('lambda_max', 2048.0)),
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

            new_cam = settings.get('camera_index', 0)
            if new_cam != self.camera_index:
                self.camera_manager.stop()
                self.camera_manager = CameraManager(camera_index=new_cam)
                self.camera_index = new_cam
            
        except Exception as e:
            print(f"Settings save error: {e}")

    # Removed unused calibration functions
        return
        
    
    def generate_scan_points(self):
        """Generate scan points based on full image and step settings using snake pattern"""
        # Use full image dimensions
        if hasattr(self, 'pixelink_image_data') and self.pixelink_image_data is not None:
            h, w = self.pixelink_image_data.shape[:2]
            x, y = 0, 0  # Start from top-left corner
        else:
            # Default image size if no camera data
            w, h = 640, 480
            x, y = 0, 0
            
        step_x = self.step_x.get()
        step_y = self.step_y.get()
        
        # Calculate number of scan points
        nx = max(1, (w // step_x) + 1)
        ny = max(1, (h // step_y) + 1)
        
        points = []
        
        # Generate points using snake pattern (S-shaped scanning)
        for iy in range(ny):
            # Determine scan direction for current row
            if iy % 2 == 0:
                # Even row: left to right
                x_range = range(nx)
            else:
                # Odd row: right to left
                x_range = range(nx - 1, -1, -1)
                
            for ix in x_range:
                # Calculate pixel positions
                pixel_x = x + (ix * step_x)
                pixel_y = y + (iy * step_y)
                points.append((pixel_x, pixel_y))
        
        print(f"Generated {len(points)} scan points")
        return points
        
    # Removed unused helper functions
    
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
    
    def _load_measurement_data_on_demand(self, filename):
        """Load measurement data only when needed - optimized with numpy"""
        data = []
        try:
            # Use numpy for faster loading of large files
            raw_data = np.loadtxt(filename, delimiter=',')
            if raw_data.ndim == 1:
                raw_data = raw_data.reshape(1, -1)
            
            for row in raw_data:
                if len(row) < 3:
                    continue
                x = int(row[0])
                y = int(row[1])
                spectrum = row[2:].tolist()
                data.append([x, y, spectrum])
        except Exception as e:
            print(f"Error loading file {filename}: {e}")
        return data
    
    def export_measurements(self):
        """Export all measurements to a single file"""
        if not self.measurement_files:
            messagebox.showinfo("Info", "No measurements to export")
            return
        
        # Ask for save location
        filename = filedialog.asksaveasfilename(
            title="Export Measurements",
            defaultextension=".csv",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")]
        )
        
        if filename:
            try:
                with open(filename, 'w', newline='') as f:
                    writer = csv.writer(f)
                    
                    # NO HEADER - compatible format: x_pixel, y_pixel, spectrum_values
                    # writer.writerow(['measurement_id', 'x', 'y'] + [f'wavelength_{i}' for i in range(2048)])
                    
                    # Write all measurements - load on demand
                    for measurement_id, measurement_file in enumerate(self.measurement_files):
                        measurement_data = self._load_measurement_data_on_demand(measurement_file)
                        for point in measurement_data:
                            x, y, spectrum = point
                            writer.writerow([x, y] + spectrum)
                    
                    messagebox.showinfo("Success", f"Measurements exported to:\n{filename}")
                    
            except Exception as e:
                print(f"Export error: {e}")
                messagebox.showerror("Error", f"Cannot export measurements:\n{e}")

    def delete_all_measurements(self):
        """Delete all measurements"""
        if not self.measurement_files:
            messagebox.showinfo("Info", "No measurements to delete")
            return
        
        result = messagebox.askyesno(
            "Delete All Measurements",
            f"Are you sure you want to delete all {len(self.measurement_files)} measurements?\n"
            "This action cannot be undone!"
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
                
                messagebox.showinfo("Success", f"Deleted {deleted_count} measurements")
                
            except Exception as e:
                print(f"Error deleting measurements: {e}")
                messagebox.showerror("Error", f"Cannot delete measurements:\n{e}")

    def delete_measurement(self, measurement_index):
        """Delete selected measurement"""
        if 0 <= measurement_index < len(self.measurement_files):
            result = messagebox.askyesno(
                "Delete Measurement",
                f"Are you sure you want to delete measurement {measurement_index + 1}?\n"
                "This action cannot be undone!"
            )
            
            if result:
                try:
                    file_to_delete = self.measurement_files[measurement_index]
                    os.remove(file_to_delete)
                    
                    self.measurement_files.pop(measurement_index)
                    self.draw_measurements()
                    
                except Exception as e:
                    print(f"Error deleting measurement: {e}")
                    messagebox.showerror("Error", f"Cannot delete measurement:\n{e}")

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

    def show_measurement_by_index(self, measurement_index):
        """Show selected measurement by index - load data on demand"""
        if 0 <= measurement_index < len(self.measurement_files):
            filename = self.measurement_files[measurement_index]
            # Load data only when needed
            measurement_data = self._load_measurement_data_on_demand(filename)
            HeatMapWindow(self, measurement_index + 1, measurement_data)

    def move_motor(self, direction):
        """Manual motor movement function"""
        try:
            step_size = self.motor_step_var.get()
            
            if not self.motor_controller.connected:
                self.motor_status.config(text=f"Not connected - simulated move {direction} {step_size}")
                return
            
            self.motor_status.config(text=f"Moving {direction} {step_size} steps...")
            
            if direction == 'o':
                self.motor_controller.move('o')
            else:
                self.motor_controller.move(direction, step_size)
            
            self.after(200, lambda: self.motor_status.config(text="Motor Status: Ready"))
            
        except Exception as e:
            error_msg = f"Motor Status: Error - {e}"
            self.motor_status.config(text=error_msg)
            print(f"Motor movement error: {e}")

    # Duplicate cleanup removed; using unified cleanup above

    def _confirm_area(self):
        """Use CustomWindow minimal confirmation instead of messagebox."""
        try:
            return self.confirm(title="Potwierdź obszar", message="Czy obszar się zgadza?")
        except Exception:
            # Fallback to standard dialog
            return messagebox.askyesno("Potwierdź obszar", "Czy obszar się zgadza?")


if __name__ == "__main__":
    try:
        app = SpektrometerApp()
        app.mainloop()
        
    except Exception as e:
        print(f"APPLICATION ERROR: {e}")
        import traceback
        traceback.print_exc()
    except KeyboardInterrupt:
        print("Application interrupted by user")
    finally:
        try:
            if 'app' in locals():
                app.cleanup()
        except Exception as e:
            print(f"Error during cleanup: {e}")