import sys, os, time, math, struct, threading
import numpy as np
import serial
import serial.tools.list_ports
import soundfile as sf
from scipy.signal import resample_poly
from math import gcd
from PyQt6.QtWidgets import QApplication, QMainWindow, QWidget, QLabel, QPushButton, QSlider, QFileDialog, QHBoxLayout, QVBoxLayout, QSizePolicy, QTextEdit
from PyQt6.QtCore import Qt, QTimer, QThread, pyqtSignal
from PyQt6.QtGui import QColor, QPainter, QPen, QBrush, QPainterPath, QMouseEvent, QFont, QFontDatabase

# Protocol constants
TARGET_FS    = 44100
CHUNK_PAIRS  = 512          # stereo pairs per binary chunk (~11.6 ms)
BAUD_RATE    = 115200
CHUNK_MAGIC  = b'\xAA\x55'
KNOWN_VIDS   = {0x239A, 0x2E8A}
KNOWN_KW     = ['feather','rp2040','adafruit','circuitpython','tinyusb','pico','raspberry']

# Audio helpers
def load_audio(path: str) -> tuple[np.ndarray, float]:
    """Return (stereo_int16 ndarray shape (N,2), duration_sec)."""
    data, fs = sf.read(path, dtype='float32', always_2d=True)
    if data.shape[1] == 1:
        data = np.repeat(data, 2, axis=1)
    elif data.shape[1] > 2:
        data = data[:, :2]
    if fs != TARGET_FS:
        g = gcd(int(fs), TARGET_FS)
        up, down = TARGET_FS // g, int(fs) // g
        L = resample_poly(data[:, 0], up, down).astype(np.float32)
        R = resample_poly(data[:, 1], up, down).astype(np.float32)
        data = np.stack([L, R], axis=1)
    peak = np.max(np.abs(data))
    if peak > 0:
        data /= peak
    s16 = (np.clip(data, -1, 1) * 32767).astype(np.int16)
    return s16, len(s16) / TARGET_FS

def build_peaks(samples: np.ndarray, n: int = 300) -> np.ndarray:
    """Downsample to n peak values (mono mix, 0..1)."""
    total = len(samples)
    if total == 0:
        return np.zeros(n)
    seg = max(1, total // n)
    out = np.empty(n, dtype=float)
    for i in range(n):
        s, e = i * seg, min((i + 1) * seg, total)
        out[i] = np.max(np.abs(samples[s:e])) / 32767.0
    return out

def make_chunk(pairs_i16: np.ndarray) -> bytes:
    """Build binary chunk: magic(2) + count(2LE) + samples(N*4)."""
    n = len(pairs_i16)
    hdr  = CHUNK_MAGIC + struct.pack('<H', n)
    body = pairs_i16.astype('<i2').tobytes()
    return hdr + body

# Port scanner thread
def _is_rp2040(p) -> bool:
    desc = (p.description  or '').lower()
    mfr  = (p.manufacturer or '').lower()
    hwid = (p.hwid         or '').lower()
    if p.vid in KNOWN_VIDS:
        return True
    return any(kw in desc or kw in mfr or kw in hwid for kw in KNOWN_KW)

class PortScanner(QThread):
    device_found = pyqtSignal(str, str)   # port, description
    device_lost  = pyqtSignal()
    scan_result  = pyqtSignal(list)       # list of strings for log

    def __init__(self):
        super().__init__()
        self._stop    = threading.Event()
        self._current = None

    def run(self):
        log_throttle = 0
        while not self._stop.is_set():
            ports = serial.tools.list_ports.comports()
            log_throttle += 1
            if log_throttle % 6 == 1:
                self.scan_result.emit([
                    f"{p.device}  VID={hex(p.vid) if p.vid else '?'}  "
                    f"PID={hex(p.pid) if p.pid else '?'}  {p.description}"
                    for p in ports
                ] or ["(no serial ports found)"])
            found = next((p for p in ports if _is_rp2040(p)), None)
            if found and found.device != self._current:
                self._current = found.device
                self.device_found.emit(found.device, found.description or found.device)
            elif not found and self._current:
                self._current = None
                self.device_lost.emit()
            time.sleep(1.5)

    def stop(self):
        self._stop.set()

# Stream worker thread
class StreamWorker(QThread):
    position_changed = pyqtSignal(int)   # absolute sample index
    finished         = pyqtSignal()
    error            = pyqtSignal(str)
    log_msg          = pyqtSignal(str)

    def __init__(self, port: str, samples: np.ndarray,
                 start_idx: int, loop: bool, volume: float):
        super().__init__()
        self.port      = port
        self.samples   = samples      # full (N,2) int16 array
        self.start_idx = start_idx
        self.loop      = loop
        self.volume    = volume       # 0.0–1.0
        self._pause    = threading.Event(); self._pause.set()
        self._stop     = threading.Event()

    def pause(self):          self._pause.clear()
    def resume(self):         self._pause.set()
    def stop(self):           self._stop.set(); self._pause.set()
    def set_volume(self, v):  self.volume = v
    def set_loop(self, v):    self.loop = v

    def run(self):
        self.log_msg.emit(f"[MAP] Opening {self.port} …")
        try:
            ser = serial.Serial(self.port, BAUD_RATE, timeout=0.1, write_timeout=2)
        except serial.SerialException as e:
            self.error.emit(str(e)); return

        # Send MODE USB + PLAY
        time.sleep(0.1)
        ser.reset_input_buffer()
        for cmd in (b"MODE USB\n", b"PLAY\n"):
            ser.write(cmd); ser.flush(); time.sleep(0.05)
        self.log_msg.emit("[MAP] Streaming …")

        total = len(self.samples)
        idx   = self.start_idx
        chunk = CHUNK_PAIRS
        chunk_dur = chunk / TARGET_FS

        # ── Pre-fill phase ──────────────────────────────────────────────────
        # The RP2040 firmware has an 8192-sample audio FIFO.  Send enough
        # chunks to fill it before starting paced delivery; this gives the
        # device a comfortable cushion so playback never starves during the
        # brief moment between the first write and the steady-state loop.
        FIFO_SAMPLES   = 8192
        PREFILL_CHUNKS = FIFO_SAMPLES // chunk          # = 16 for CHUNK_PAIRS=512
        for _ in range(PREFILL_CHUNKS):
            if self._stop.is_set() or idx >= total:
                break
            end  = min(idx + chunk, total)
            data = self.samples[idx:end].copy()
            if self.volume < 1.0:
                data = (data.astype(np.int32) * int(self.volume * 256) >> 8).clip(
                    -32768, 32767).astype(np.int16)
            try:
                ser.write(make_chunk(data)); ser.flush()
            except serial.SerialException as e:
                self.error.emit(str(e)); return
            idx = end

        # ── Paced delivery phase ─────────────────────────────────────────────
        # Use a monotonic deadline so each chunk is sent at a fixed wall-clock
        # interval regardless of sleep jitter or system load.  Targeting 90 %
        # of chunk_dur keeps us slightly ahead of real time; the RP2040's FIFO
        # absorbs the small surplus, and natural USB flow-control takes over once
        # the FIFO is full.  The old `time.sleep(chunk_dur * 0.85)` approach
        # caused underruns because Python's sleep can overshoot by 10-15 ms on
        # most OSes, which is larger than the 1.7 ms margin that 0.85× leaves.
        deadline = time.monotonic()

        while not self._stop.is_set():
            self._pause.wait()
            if self._stop.is_set(): break

            # Drain any device responses
            if ser.in_waiting:
                try:
                    txt = ser.read(ser.in_waiting).decode('utf-8', errors='replace').strip()
                    if txt:
                        self.log_msg.emit(f"[RP2040] {txt}")
                except Exception:
                    pass

            end  = min(idx + chunk, total)
            data = self.samples[idx:end].copy()

            # Apply volume
            if self.volume < 1.0:
                data = (data.astype(np.int32) * int(self.volume * 256) >> 8).clip(
                    -32768, 32767).astype(np.int16)
            try:
                ser.write(make_chunk(data)); ser.flush()
            except serial.SerialException as e:
                self.error.emit(str(e)); break
            idx = end
            self.position_changed.emit(idx)
            if idx >= total:
                if self.loop:
                    idx = 0
                    self.log_msg.emit("[MAP] Looping")
                else:
                    break

            # Advance the deadline by 90 % of a chunk's duration and sleep
            # until we reach it.  Skip the sleep if we're already late, and
            # reset the deadline if we've drifted more than one full chunk
            # behind (e.g. after a long pause) to avoid a burst catch-up.
            deadline += chunk_dur * 0.90
            now       = time.monotonic()
            remaining = deadline - now
            if remaining > 0.001:
                time.sleep(remaining)
            elif remaining < -chunk_dur:
                deadline = time.monotonic()   # clock skew reset

        try:
            ser.write(b"STOP\n"); ser.flush()
            time.sleep(0.05)
            ser.close()
        except Exception:
            pass
        self.log_msg.emit("[MAP] Stream ended.")
        self.finished.emit()

# Serial reader thread (for ADC data and responses when not streaming)
class SerialReader(QThread):
    adc_sample = pyqtSignal(int)    # raw 12-bit ADC value
    log_msg    = pyqtSignal(str)

    def __init__(self, port: str):
        super().__init__()
        self.port  = port
        self._stop = threading.Event()

    def stop(self): self._stop.set()

    def run(self):
        try:
            ser = serial.Serial(self.port, BAUD_RATE, timeout=0.1)
        except Exception as e:
            self.log_msg.emit(f"[READER] Open error: {e}"); return

        # Give the CDC link time to stabilise (same pattern as StreamWorker).
        # Then send STOP before MODE ADC: if the RP2040 was left in streaming
        # mode by a previous session that closed without a clean shutdown, the
        # binary state-machine intercepts all bytes and text commands never
        # reach process_cmd().  Sending STOP first exits that state when the
        # device is already in text mode; the RP2040-side watchdog (which
        # clears g_playing on USB disconnect) handles the stuck case on
        # reconnect so that MODE ADC is always received as a text command.
        time.sleep(0.1)
        ser.reset_input_buffer()
        ser.write(b"\nSTOP\n"); ser.flush()
        time.sleep(0.05)
        ser.reset_input_buffer()
        ser.write(b"MODE ADC\n"); ser.flush()
        while not self._stop.is_set():
            try:
                line = ser.readline().decode('ascii', errors='replace').strip()
            except serial.SerialException:
                break
            if not line: continue
            if line.startswith("ADC "):
                try:
                    self.adc_sample.emit(int(line.split()[1]))
                except (ValueError, IndexError):
                    pass
            elif line.startswith("OK ") or line.startswith("STATUS ") or line.startswith("HELLO "):
                self.log_msg.emit(f"[RP2040] {line}")
        try: ser.close()
        except Exception: pass


# Waveform / oscilloscope widget
class WaveformWidget(QWidget):
    seek_requested = pyqtSignal(float)  # 0.0–1.0

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(72)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setMouseTracking(True)

        self._peaks    : np.ndarray | None = None
        self._progress : float             = 0.0
        self._hover    : float             = -1.0
        self._adc_hist : list[float]       = [0.0] * 220
        self._mode     : str               = 'USB'   # 'USB' | 'ADC'

    def set_mode(self, m: str):
        self._mode = m
        self.update()

    def set_peaks(self, peaks: np.ndarray):
        self._peaks = peaks
        self.update()

    def clear_peaks(self):
        self._peaks = None
        self._progress = 0.0
        self.update()

    def set_progress(self, p: float):
        self._progress = max(0.0, min(1.0, p))
        self.update()

    def push_adc(self, raw: int):
        v = (raw / 4095.0) * 2.0 - 1.0
        self._adc_hist.pop(0)
        self._adc_hist.append(v)
        self.update()

    def mouseMoveEvent(self, e: QMouseEvent):
        self._hover = e.position().x() / max(self.width(), 1)
        self.update()

    def leaveEvent(self, e):
        self._hover = -1.0
        self.update()

    def mousePressEvent(self, e: QMouseEvent):
        if self._mode == 'USB':
            self.seek_requested.emit(
                max(0.0, min(1.0, e.position().x() / max(self.width(), 1))))

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        w, h = self.width(), self.height()
        mid  = h // 2

        p.fillRect(0, 0, w, h, QColor(0, 0, 0))

        if self._mode == 'ADC':
            self._draw_oscilloscope(p, w, h, mid)
            p.end(); return

        # File waveform
        if self._peaks is None or len(self._peaks) == 0:
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(QColor(25, 25, 25))
            bw = max(2, w // 100)
            for x in range(0, w, bw + 2):
                bh = max(2, int(h * 0.08))
                p.drawRect(x, mid - bh, bw, bh * 2)
            p.end(); return

        n    = len(self._peaks)
        step = w / n
        bw   = max(1, int(step))

        for i, pk in enumerate(self._peaks):
            x     = int(i * step)
            ratio = (i + 1) / n
            bh    = max(2, int(pk * mid * 0.92))
            if ratio <= self._progress:
                col = QColor(255, 255, 255)
            elif self._hover >= 0 and ratio <= self._hover:
                col = QColor(160, 160, 160)
            else:
                col = QColor(55, 55, 55)
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(col)
            p.drawRect(x, mid - bh, bw - 1, bh * 2)

        # Playhead
        px = int(self._progress * w)
        p.setPen(QPen(QColor(255, 255, 255), 1))
        p.drawLine(px, 0, px, h)
        p.end()

    def _draw_oscilloscope(self, p: QPainter, w: int, h: int, mid: int):
        n    = len(self._adc_hist)
        pts  = []
        for i, v in enumerate(self._adc_hist):
            x = int(i * w / (n - 1))
            y = int(mid - v * mid * 0.88)
            pts.extend([x, y])
        # Draw as polyline
        p.setPen(QPen(QColor(200, 200, 200), 1))
        for i in range(0, len(pts) - 2, 2):
            p.drawLine(pts[i], pts[i+1], pts[i+2], pts[i+3])
        # Label
        p.setPen(QPen(QColor(80, 80, 80), 1))
        p.setFont(QFont("Courier New", 7))
        p.drawText(6, 14, "ADC LIVE")

# Animated play button (matches reference)
class PlayButton(QWidget):
    clicked = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(56, 56)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self._playing = False
        self._angle   = 0
        t = QTimer(self)
        t.timeout.connect(self._tick)
        t.start(30)

    def set_playing(self, v: bool):
        self._playing = v
        self.update()

    def _tick(self):
        if self._playing:
            self._angle = (self._angle + 4) % 360
            self.update()

    def mousePressEvent(self, _):
        self.clicked.emit()

    def paintEvent(self, _):
        p  = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        cx, cy, r = 28, 28, 25

        p.setPen(QPen(QColor(255, 255, 255), 2))
        p.setBrush(QColor(0, 0, 0))
        p.drawEllipse(cx - r, cy - r, r * 2, r * 2)

        if self._playing:
            pen = QPen(QColor(255, 255, 255), 2)
            pen.setCapStyle(Qt.PenCapStyle.RoundCap)
            pen.setStyle(Qt.PenStyle.DashLine)
            p.setPen(pen)
            p.setBrush(Qt.BrushStyle.NoBrush)
            p.drawArc(cx - r + 3, cy - r + 3, (r - 3) * 2, (r - 3) * 2,
                      self._angle * 16, 120 * 16)

        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(255, 255, 255))
        if self._playing:
            p.drawRect(cx - 8, cy - 9, 5, 18)
            p.drawRect(cx + 3, cy - 9, 5, 18)
        else:
            path = QPainterPath()
            path.moveTo(cx - 6, cy - 10)
            path.lineTo(cx - 6, cy + 10)
            path.lineTo(cx + 12, cy)
            path.closeSubpath()
            p.drawPath(path)
        p.end()

# Pulsing device indicator dot
class DeviceDot(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(10, 10)
        self._on    = False
        self._phase = 0
        t = QTimer(self)
        t.timeout.connect(self._tick)
        t.start(40)

    def set_connected(self, v: bool):
        self._on = v
        self.update()

    def _tick(self):
        if self._on:
            self._phase = (self._phase + 5) % 360
            self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        if self._on:
            bright = int(140 + 115 * math.sin(math.radians(self._phase)))
            p.setBrush(QColor(bright, bright, bright))
        else:
            p.setBrush(QColor(35, 35, 35))
        p.setPen(Qt.PenStyle.NoPen)
        p.drawEllipse(0, 0, 10, 10)
        p.end()

#  Main Window
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Modular Audio Processor")
        self.setMinimumSize(700, 580)
        self.resize(780, 630)
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)

        self._samples    : np.ndarray | None = None
        self._duration   : float             = 0.0
        self._port       : str | None        = None
        self._mode       : str               = 'USB'  # 'USB' | 'ADC'
        self._worker     : StreamWorker | None  = None
        self._reader     : SerialReader | None  = None
        self._stream_off : int               = 0   # offset into samples
        self._drag_pos                       = None
        self._scan_throttle                  = 0
        self._build_ui()
        self._apply_style()
        self._scanner = PortScanner()
        self._scanner.device_found.connect(self._on_device_found)
        self._scanner.device_lost.connect(self._on_device_lost)
        self._scanner.scan_result.connect(self._on_scan_result)
        self._scanner.start()
        self._ui_timer = QTimer()
        self._ui_timer.timeout.connect(self._tick_ui)
        self._ui_timer.start(100)
        self._log("[MAP] Modular Audio Processor started")
        self._log("[MAP] Scanning for RP2040 USB-CDC device …")

    # UI Layout
    def _build_ui(self):
        root = QWidget(self)
        root.setObjectName("root")
        self.setCentralWidget(root)
        outer = QVBoxLayout(root)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        # Title bar
        tb = QWidget(); tb.setObjectName("titlebar"); tb.setFixedHeight(38)
        tbl = QHBoxLayout(tb); tbl.setContentsMargins(14, 0, 8, 0)
        self._dot       = DeviceDot()
        lbl_title       = QLabel("MODULAR AUDIO PROCESSOR"); lbl_title.setObjectName("appTitle")
        self._lbl_port  = QLabel("No device");               self._lbl_port.setObjectName("portLabel")
        btn_close       = QPushButton("✕");                  btn_close.setObjectName("btnClose")
        btn_close.setFixedSize(26, 26); btn_close.clicked.connect(self.close)
        tbl.addWidget(self._dot); tbl.addSpacing(8)
        tbl.addWidget(lbl_title); tbl.addStretch()
        tbl.addWidget(self._lbl_port); tbl.addSpacing(12)
        tbl.addWidget(btn_close)

        # Body
        body = QWidget(); body.setObjectName("body")
        bl = QVBoxLayout(body); bl.setContentsMargins(28, 18, 28, 18); bl.setSpacing(12)

        # Mode toggle
        mode_row = QHBoxLayout(); mode_row.setSpacing(6)
        lbl_m = QLabel("INPUT SOURCE"); lbl_m.setObjectName("sectionLabel")
        self._btn_mode_usb = QPushButton("DIGITAL")
        self._btn_mode_usb.setObjectName("btnModeActive")
        self._btn_mode_usb.setCheckable(True); self._btn_mode_usb.setChecked(True)
        self._btn_mode_usb.clicked.connect(lambda: self._set_mode('USB'))
        self._btn_mode_adc = QPushButton("ANALOG")
        self._btn_mode_adc.setObjectName("btnModeInactive")
        self._btn_mode_adc.setCheckable(True)
        self._btn_mode_adc.clicked.connect(lambda: self._set_mode('ADC'))
        mode_row.addWidget(lbl_m); mode_row.addSpacing(10)
        mode_row.addWidget(self._btn_mode_usb)
        mode_row.addWidget(self._btn_mode_adc)
        mode_row.addStretch()

        # Track info row
        info_row = QHBoxLayout()
        self._lbl_track = QLabel("No file loaded"); self._lbl_track.setObjectName("trackTitle")
        self._lbl_total = QLabel("--:--");           self._lbl_total.setObjectName("monoLabel")
        info_row.addWidget(self._lbl_track); info_row.addStretch(); info_row.addWidget(self._lbl_total)

        # Waveform
        self._wave = WaveformWidget()
        self._wave.seek_requested.connect(self._on_seek)

        # Elapsed
        time_row = QHBoxLayout()
        self._lbl_elapsed = QLabel("0:00"); self._lbl_elapsed.setObjectName("monoLabel")
        time_row.addWidget(self._lbl_elapsed); time_row.addStretch()

        # Transport controls
        ctrl = QHBoxLayout(); ctrl.setSpacing(10)
        btn_open = QPushButton("OPEN FILE"); btn_open.setObjectName("btnPrimary")
        btn_open.clicked.connect(self._open_file)
        self._btn_loop = QPushButton("LOOP"); self._btn_loop.setObjectName("btnToggle")
        self._btn_loop.setCheckable(True); self._btn_loop.setFixedWidth(64)
        self._play_btn = PlayButton()
        self._play_btn.clicked.connect(self._toggle_play)
        btn_stop = QPushButton("STOP"); btn_stop.setObjectName("btnDanger")
        btn_stop.setFixedWidth(64); btn_stop.clicked.connect(self._stop)

        vol_row = QHBoxLayout(); vol_row.setSpacing(8)
        lbl_vol = QLabel("VOL"); lbl_vol.setObjectName("monoSmall")
        self._vol_slider = QSlider(Qt.Orientation.Horizontal)
        self._vol_slider.setObjectName("volSlider")
        self._vol_slider.setRange(0, 100); self._vol_slider.setValue(80)
        self._vol_slider.setFixedWidth(110)
        self._vol_slider.valueChanged.connect(self._on_volume)
        vol_row.addWidget(lbl_vol); vol_row.addWidget(self._vol_slider)

        ctrl.addWidget(btn_open); ctrl.addStretch()
        ctrl.addWidget(self._btn_loop)
        ctrl.addWidget(self._play_btn)
        ctrl.addWidget(btn_stop)
        ctrl.addStretch(); ctrl.addLayout(vol_row)

        # Status
        self._lbl_status = QLabel("Ready"); self._lbl_status.setObjectName("statusLabel")

        # Debug log
        lbl_debug = QLabel("SERIAL DEBUG"); lbl_debug.setObjectName("debugHeader")
        self._debug = QTextEdit()
        self._debug.setObjectName("debugConsole")
        self._debug.setReadOnly(True)
        self._debug.setFixedHeight(120)
        self._debug.setFont(QFont("Courier New", 8))
        bl.addLayout(mode_row)
        bl.addLayout(info_row)
        bl.addWidget(self._wave)
        bl.addLayout(time_row)
        bl.addLayout(ctrl)
        bl.addWidget(self._lbl_status)
        bl.addWidget(lbl_debug)
        bl.addWidget(self._debug)
        outer.addWidget(tb)
        outer.addWidget(body, 1)

        # Drag to move (frameless)
        tb.mousePressEvent   = self._tb_press
        tb.mouseMoveEvent    = self._tb_move
        tb.mouseReleaseEvent = lambda e: setattr(self, '_drag_pos', None)

    def _apply_style(self):
        self.setStyleSheet("""
            QWidget#root {background: #000000;
                border: 1px solid #222222;
                border-radius: 10px;
            }
            QWidget#titlebar {
                background: #080808;
                border-top-left-radius: 10px;
                border-top-right-radius: 10px;
                border-bottom: 1px solid #1a1a1a;
            }
            QWidget#body { background: transparent; }
            QLabel#appTitle {font-family: 'Courier New', monospace; font-size: 15px; font-weight: bold; letter-spacing: 5px; color: #ffffff;}
            QLabel#portLabel {font-family: 'Courier New', monospace; font-size: 5px; color: #444444;}
            QLabel#sectionLabel {font-family: 'Courier New', monospace; font-size: 12px; letter-spacing: 3px; color: #444444;}
            QLabel#trackTitle {font-family: 'Courier New', monospace; font-size: 15px; font-weight: bold; color: #ffffff;}
            QLabel#monoLabel {font-family: 'Courier New', monospace; font-size: 20px; color: #666666;}
            QLabel#monoSmall {font-family: 'Courier New', monospace; font-size: 15px; letter-spacing: 2px; color: #444444;}
            QLabel#statusLabel {font-family: 'Courier New', monospace; font-size: 15px; letter-spacing: 1px; color: #444444;}
            QLabel#debugHeader {font-family: 'Courier New', monospace; font-size: 14px; letter-spacing: 3px; color: #2a2a2a;}
            QPushButton#btnClose {background: transparent; color: #333333; border: none; font-size: 12px;}
            QPushButton#btnClose:hover {color: #ffffff; }
            QPushButton#btnModeActive, QPushButton#btnModeInactive {font-family: 'Courier New', monospace; font-size: 9px; letter-spacing: 2px; border-radius: 3px; padding: 5px 14px; border: 1px solid #2a2a2a;}
            QPushButton#btnModeActive:checked,
            QPushButton#btnModeInactive:checked {background: #ffffff; color: #000000; border-color: #ffffff;}
            QPushButton#btnModeActive:!checked,
            QPushButton#btnModeInactive:!checked {background: #0a0a0a; color: #444444;}
            QPushButton#btnModeActive:hover:!checked,
            QPushButton#btnModeInactive:hover:!checked {color: #aaaaaa; }

            QPushButton#btnPrimary {
                background: #ffffff; color: #000000;
                border: none; border-radius: 3px;
                padding: 7px 16px;
                font-family: 'Courier New', monospace;
                font-size: 10px; font-weight: bold; letter-spacing: 2px;
            }
            QPushButton#btnPrimary:hover { background: #cccccc; }

            QPushButton#btnToggle {
                background: #0d0d0d; color: #484848;
                border: 1px solid #2a2a2a; border-radius: 3px;
                font-family: 'Courier New', monospace;
                font-size: 9px; letter-spacing: 2px; padding: 6px 0;
            }
            QPushButton#btnToggle:checked {
                background: #ffffff; color: #000000; border-color: #ffffff;
            }

            QPushButton#btnDanger {
                background: #0d0d0d; color: #555555;
                border: 1px solid #222222; border-radius: 3px;
                font-family: 'Courier New', monospace;
                font-size: 9px; letter-spacing: 2px; padding: 6px 0;
            }
            QPushButton#btnDanger:hover { color: #ffffff; border-color: #ffffff; }

            QSlider#volSlider::groove:horizontal {
                height: 2px; background: #2a2a2a;
            }
            QSlider#volSlider::sub-page:horizontal { background: #ffffff; }
            QSlider#volSlider::handle:horizontal {
                width: 10px; height: 10px; margin: -4px 0;
                background: #ffffff; border-radius: 5px;
            }

            QTextEdit#debugConsole {
                background: #030303; color: #00cc44;
                border: 1px solid #141414; border-radius: 3px;
                font-family: 'Courier New', monospace; font-size: 14px;
            }
        """)

    # Drag / move
    def _tb_press(self, e: QMouseEvent):
        if e.button() == Qt.MouseButton.LeftButton:
            self._drag_pos = e.globalPosition().toPoint() - self.frameGeometry().topLeft()

    def _tb_move(self, e: QMouseEvent):
        if self._drag_pos and e.buttons() & Qt.MouseButton.LeftButton:
            self.move(e.globalPosition().toPoint() - self._drag_pos)

    # Logging
    def _log(self, msg: str):
        ts = time.strftime("%H:%M:%S")
        self._debug.append(f"{ts}  {msg}")
        sb = self._debug.verticalScrollBar()
        sb.setValue(sb.maximum())

    # Device events
    def _on_scan_result(self, ports: list):
        self._scan_throttle += 1
        if self._scan_throttle % 8 == 1:
            if ports:
                self._log("[MAP] Port scan:")
                for e in ports: self._log(f"       {e}")
            else:
                self._log("[MAP] No serial ports found")

    def _on_device_found(self, port: str, desc: str):
        self._port = port
        self._dot.set_connected(True)
        self._lbl_port.setText(port)
        self._lbl_status.setText(f"Device on {port}")
        self._log(f"[MAP] RP2040 detected → {port}  ({desc})")
        # If in ADC mode, start the reader immediately
        if self._mode == 'ADC':
            self._start_adc_reader()

    def _on_device_lost(self):
        self._port = None
        self._dot.set_connected(False)
        self._lbl_port.setText("No device")
        self._lbl_status.setText("Device disconnected")
        self._log("[MAP] RP2040 disconnected")
        self._stop_all_workers()

    # Mode switching
    def _set_mode(self, m: str):
        if m == self._mode:
            return
        self._stop_all_workers()
        self._mode = m

        # Update toggle button visuals
        usb_active = (m == 'USB')
        self._btn_mode_usb.setChecked(usb_active)
        self._btn_mode_adc.setChecked(not usb_active)
        self._btn_mode_usb.setObjectName("btnModeActive" if usb_active else "btnModeInactive")
        self._btn_mode_adc.setObjectName("btnModeActive" if not usb_active else "btnModeInactive")
        self._apply_style()
        self._wave.set_mode(m)
        self._log(f"[MAP] Mode → {m}")
        if m == 'ADC':
            self._lbl_status.setText("ADC mode — monitoring input")
            if self._port:
                self._start_adc_reader()
        else:
            self._lbl_status.setText("USB mode — ready to stream")

    # ADC reader management
    def _start_adc_reader(self):
        if self._reader and self._reader.isRunning():
            return
        if not self._port:
            return
        self._reader = SerialReader(self._port)
        self._reader.adc_sample.connect(self._wave.push_adc)
        self._reader.log_msg.connect(self._log)
        self._reader.start()
        self._log(f"[MAP] ADC reader started on {self._port}")

    def _stop_adc_reader(self):
        if self._reader:
            self._reader.stop()
            self._reader.wait(1000)
            self._reader = None

    def _stop_all_workers(self):
        self._stop()
        self._stop_adc_reader()

    # File loading
    def _open_file(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Open Audio File", "",
            "Audio (*.wav *.flac *.ogg *.aiff *.mp3 *.m4a);;All (*)"
        )
        if not path: return
        self._stop()
        self._lbl_status.setText("Loading …")
        self._log(f"[MAP] Loading: {os.path.basename(path)}")
        QApplication.processEvents()
        try:
            self._samples, self._duration = load_audio(path)
            name = os.path.basename(path)
            self._lbl_track.setText(name)
            m, s = divmod(int(self._duration), 60)
            self._lbl_total.setText(f"{m}:{s:02d}")
            self._wave.set_peaks(build_peaks(self._samples, 300))
            self._wave.set_progress(0.0)
            self._lbl_elapsed.setText("0:00")
            self._lbl_status.setText("Loaded — ready to stream")
            self._log(f"[MAP] OK  {len(self._samples)} samples  {m}:{s:02d}  @ {TARGET_FS} Hz")
        except Exception as ex:
            self._lbl_status.setText(f"Load error: {ex}")
            self._log(f"[MAP] Load error: {ex}")

    # Transport
    def _toggle_play(self):
        if self._worker and self._worker.isRunning():
            if self._play_btn._playing:
                self._worker.pause()
                self._play_btn.set_playing(False)
                self._lbl_status.setText("Paused")
                self._log("[MAP] Paused")
            else:
                self._worker.resume()
                self._play_btn.set_playing(True)
                self._lbl_status.setText("Streaming …")
                self._log("[MAP] Resumed")
        else:
            self._start_stream()

    def _start_stream(self, start_idx: int = 0):
        if self._mode != 'USB':
            self._log("[MAP] Switch to USB mode to stream files")
            return
        if self._samples is None:
            self._lbl_status.setText("No file loaded"); return
        if not self._port:
            self._lbl_status.setText("No RP2040 detected"); return

        self._stream_off = start_idx
        vol = self._vol_slider.value() / 100.0
        self._worker = StreamWorker(
            self._port, self._samples, start_idx,
            loop=self._btn_loop.isChecked(), volume=vol
        )
        self._worker.position_changed.connect(self._on_position)
        self._worker.finished.connect(self._on_finished)
        self._worker.error.connect(self._on_error)
        self._worker.log_msg.connect(self._log)
        self._worker.start()
        self._play_btn.set_playing(True)
        self._lbl_status.setText("Streaming to RP2040 …")

    def _stop(self):
        if self._worker:
            self._worker.stop()
            self._worker.wait(2000)
            self._worker = None
        self._play_btn.set_playing(False)
        self._wave.set_progress(0.0)
        self._lbl_elapsed.setText("0:00")
        self._lbl_status.setText("Stopped")

    # Seek
    def _on_seek(self, ratio: float):
        if self._samples is None: return
        target      = int(ratio * len(self._samples))
        was_playing = bool(self._worker and self._worker.isRunning()
                           and self._play_btn._playing)
        if self._worker:
            self._worker.stop()
            self._worker.wait(1000)
            self._worker = None
        self._play_btn.set_playing(False)

        if was_playing:
            self._start_stream(start_idx=target)
        else:
            self._wave.set_progress(ratio)
            m, s = divmod(int(target / TARGET_FS), 60)
            self._lbl_elapsed.setText(f"{m}:{s:02d}")

    # Volume
    def _on_volume(self, val: int):
        if self._worker:
            self._worker.set_volume(val / 100.0)

    # Position callback
    def _on_position(self, idx: int):
        if self._samples is None: return
        # idx is already the absolute sample index within self._samples
        # (StreamWorker emits it as `end`, which starts from start_idx and
        # increments toward total).  Adding _stream_off here was wrong: it
        # double-counted the seek offset, making elapsed time and the progress
        # playhead jump ahead by the full seek position on every update.
        ratio = min(idx / max(len(self._samples), 1), 1.0)
        self._wave.set_progress(ratio)
        m, s = divmod(int(idx / TARGET_FS), 60)
        self._lbl_elapsed.setText(f"{m}:{s:02d}")

    def _on_finished(self):
        self._play_btn.set_playing(False)
        self._lbl_status.setText("Finished")
        self._log("[MAP] Stream finished")

    def _on_error(self, msg: str):
        self._play_btn.set_playing(False)
        self._lbl_status.setText(f"Error: {msg}")
        self._log(f"[MAP] Error: {msg}")

    # UI tick
    def _tick_ui(self):
        if self._worker:
            self._worker.set_loop(self._btn_loop.isChecked())

    # Close
    def closeEvent(self, e):
        self._stop_all_workers()
        self._scanner.stop()
        self._scanner.wait(2000)
        super().closeEvent(e)

def main():
    app = QApplication(sys.argv)
    app.setApplicationName("Modular Audio Processor")
    win = MainWindow()
    win.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
