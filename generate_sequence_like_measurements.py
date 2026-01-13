import os
import csv
import time
import json
from typing import Tuple

import numpy as np

"""Generator plików pomiarowych zgodnych z aktualną sekwencją z index.py.

Uruchom z katalogu projektu:

        python generate_sequence_like_measurements.py

Skrypt:
- wczytuje options.json (step_x, step_y, width, height, lens_magnification,
    starting_corner, lambda_*, spectrum_range_*, sequence_exposure_times),
- wylicza liczbę punktów siatki tak jak sekwencja (z uwzględnieniem powiększenia obiektywu),
- przechodzi po punktach w snake pattern z tym samym rozumieniem starting_corner,
- symuluje wiele czasów ekspozycji na każdy punkt,
- zapisuje:
    * główny plik measurement_YYYYMMDD_HHMMSS[_NNN]_spectra.csv
        z jedną kolumną widma (dla pierwszego czasu) – kompatybilny z GUI,
    * katalog measurement_data/points_YYYYMMDD_HHMMSS[_NNN]/ z plikami
        point_xX_yY.csv w formacie: lambda, I_t1, I_t2, ...

Widma są syntetyczne, ale liczba punktów i oś (lambda / piksele) są zgodne
z aktualnym ROI używanym przez aplikację.
"""


def _load_options(path: str = "options.json") -> dict:
    """Wczytaj options.json lub zwróć sensowne domyślne wartości."""
    defaults = {
        "step_x": 20,
        "step_y": 20,
        "width": 200,   # w µm w płaszczyźnie próbki
        "height": 200,
        "starting_corner": "top-left",
        "lens_magnification": 1.0,
    }
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        defaults.update(data or {})
    except Exception:
        pass
    return defaults


def _get_sequence_exposure_list_ms(opts: dict) -> list[float]:
    """Zwróć listę czasów ekspozycji (ms) używanych w sekwencji.

    Zasady (jak w index.py):
    - jeśli sequence_exposure_times jest puste -> lista z jedną wartością z exposure_time,
    - jeśli są wartości, parsujemy wszystkie poprawne liczby,
    - każdą wartość ograniczamy do zakresu [0.1 ms, 1000 ms].
    """
    MIN_MS = 0.1
    MAX_MS = 1000.0

    # Bazowa wartość
    try:
        base_ms = float(opts.get("exposure_time", 10.0))
    except Exception:
        base_ms = 10.0

    seq_text = str(opts.get("sequence_exposure_times", "") or "").strip()
    values: list[float] = []

    if seq_text:
        try:
            parts = [p.strip() for p in seq_text.replace(";", ",").split(",")]
            for p in parts:
                if not p:
                    continue
                try:
                    v = float(p.replace(",", "."))
                except Exception:
                    print(f"Invalid sequence exposure value ignored: {p}")
                    continue
                if v < MIN_MS:
                    v = MIN_MS
                if v > MAX_MS:
                    print(f"Sequence exposure {v:.1f} ms exceeds camera limit; clamped to {MAX_MS:.1f} ms")
                    v = MAX_MS
                values.append(v)
        except Exception:
            values = []

    if not values:
        v = base_ms
        if v < MIN_MS:
            v = MIN_MS
        if v > MAX_MS:
            print(f"Sequence exposure {v:.1f} ms exceeds camera limit; clamped to {MAX_MS:.1f} ms")
            v = MAX_MS
        values = [v]

    return values


def _compute_grid_from_options(opts: dict) -> Tuple[int, int, int, int, str]:
    """Zwróć (points_x, points_y, step_x, step_y, starting_corner) jak w sekwencji."""
    try:
        step_x = max(1, int(float(opts.get("step_x", 20))))
        step_y = max(1, int(float(opts.get("step_y", 20))))
    except Exception:
        step_x, step_y = 20, 20

    try:
        sample_width = max(1, int(float(opts.get("width", 200))))
        sample_height = max(1, int(float(opts.get("height", 200))))
    except Exception:
        sample_width, sample_height = 200, 200

    try:
        lens_mag = float(opts.get("lens_magnification", 1.0))
    except Exception:
        lens_mag = 1.0
    if lens_mag <= 0:
        lens_mag = 1.0

    # index.py: scan_width = sample_width * lens_magnification
    scan_width = int(sample_width * lens_mag)
    scan_height = int(sample_height * lens_mag)

    points_x = (scan_width // step_x) + 1
    points_y = (scan_height // step_y) + 1

    starting_corner = str(opts.get("starting_corner", "top-left")).strip() or "top-left"

    return points_x, points_y, step_x, step_y, starting_corner


def _compute_axis_with_roi(opts: dict) -> np.ndarray:
    """Odtwórz oś lambda/piksele z uwzględnieniem aktualnego ROI (jak w index.py).

    Zwraca wektor długości N (N <= 2048), używany jako kolumna "lambda" w
    plikach punktowych oraz jako baza do długości widma.
    """
    try:
        calibrated = bool(opts.get("lambda_calibration_enabled", False)) and (
            "lambda_min" in opts and "lambda_max" in opts
        )

        if calibrated:
            base_min = float(opts.get("lambda_min", 400.0))
            base_max = float(opts.get("lambda_max", 700.0))
            base_axis = np.linspace(base_min, base_max, 2048)
        else:
            base_min, base_max = 0.0, 2048.0
            base_axis = np.linspace(base_min, base_max, 2048)

        roi_min = float(opts.get("spectrum_range_min", base_min))
        roi_max = float(opts.get("spectrum_range_max", base_max))
        if roi_min >= roi_max:
            roi_min, roi_max = base_min, base_max

        mask = (base_axis >= roi_min) & (base_axis <= roi_max)
        if not np.any(mask):
            mask = np.ones_like(base_axis, dtype=bool)

        axis_vals = base_axis[mask]
        return axis_vals.astype(float)
    except Exception:
        # awaryjnie prosta oś pikselowa
        return np.linspace(0.0, 2048.0, 2048)


def _generate_spectrum(axis_len: int, center_norm: float, scale: float, point_index: int, total_points: int) -> np.ndarray:
    """Wygeneruj gładkie widmo z dodatkowym pikiem zależnym od punktu siatki."""
    wl_norm = np.linspace(0.0, 1.0, axis_len)
    width = 0.08
    base_profile = np.exp(-((wl_norm - center_norm) ** 2) / (2 * width * width)) * 1000.0

    spectrum = base_profile * scale

    if total_points < 1:
        return spectrum

    peak_pos = int((point_index / max(1, total_points - 1)) * (axis_len - 1))
    peak_width = max(3, axis_len // 200)
    for k in range(max(0, peak_pos - peak_width), min(axis_len, peak_pos + peak_width)):
        spectrum[k] += 500.0

    return spectrum


def generate_sequence_like_measurements(count: int = 1) -> None:
    opts = _load_options()
    points_x, points_y, step_x, step_y, starting_corner = _compute_grid_from_options(opts)
    axis_vals = _compute_axis_with_roi(opts)
    axis_len = len(axis_vals)

    exposures_ms = _get_sequence_exposure_list_ms(opts)
    try:
        times_str = ", ".join(f"{e:.1f}" for e in exposures_ms)
        print(f"Dummy sequence exposure times (ms): {times_str}")
    except Exception:
        pass
    folder = "measurement_data"
    os.makedirs(folder, exist_ok=True)

    total_points = points_x * points_y
    created = 0

    for i in range(count):
        # Jak w generate_dummy_measurements: przesuwamy maksimum między plikami
        if count > 1:
            center = 0.15 + 0.7 * (i / (count - 1))
        else:
            center = 0.5

        ts = time.strftime("%Y%m%d_%H%M%S")
        if count == 1:
            session_id = ts
        else:
            session_id = f"{ts}_{i+1:03d}"

        filename = os.path.join(folder, f"measurement_{session_id}_spectra.csv")
        points_folder = os.path.join(folder, f"points_{session_id}")
        os.makedirs(points_folder, exist_ok=True)

        try:
            with open(filename, "w", newline="") as f:
                writer = csv.writer(f)

                # Logika snake + starting_corner zgodna z index.py (sekcja sekwencji)
                left_side_start = starting_corner in ["top-left", "bottom-left"]
                point_index = 0

                for iy in range(points_y):
                    # Ustal porządek przejścia po X w danym wierszu
                    if iy % 2 == 0:
                        x_range = range(points_x) if left_side_start else range(points_x - 1, -1, -1)
                    else:
                        x_range = range(points_x - 1, -1, -1) if left_side_start else range(points_x)

                    for ix in x_range:
                        point_index += 1

                        # Odtworzenie phys_x/phys_y jak w index.py
                        if starting_corner in ["top-left", "bottom-left"]:
                            if iy % 2 == 0:
                                phys_x = ix
                            else:
                                phys_x = (points_x - 1) - ix
                        else:
                            if iy % 2 == 0:
                                phys_x = (points_x - 1) - ix
                            else:
                                phys_x = ix

                        if starting_corner in ["top-left", "top-right"]:
                            phys_y = iy
                        else:
                            phys_y = (points_y - 1) - iy

                        grid_x = int(phys_x)
                        grid_y = int(phys_y)

                        # Skala amplitudy zależna od położenia w siatce
                        norm_xy = (grid_x + grid_y) / max(1, (points_x + points_y - 2))
                        base_scale = 0.5 + 0.5 * norm_xy

                        # Wygeneruj widma dla wszystkich czasów ekspozycji
                        spectra_for_point: list[np.ndarray] = []
                        max_exp = max(exposures_ms) if exposures_ms else 1.0
                        for exp_ms in exposures_ms:
                            # lekka zmiana amplitudy w zależności od czasu ekspozycji
                            rel = float(exp_ms) / max_exp if max_exp > 0 else 1.0
                            scale = base_scale * (0.7 + 0.6 * rel)
                            spec = _generate_spectrum(axis_len, center, scale, point_index - 1, total_points)
                            # przytnij / dopasuj długość do osi (powinna już być zgodna)
                            if len(spec) != axis_len:
                                x_old = np.linspace(0.0, 1.0, len(spec))
                                x_new = np.linspace(0.0, 1.0, axis_len)
                                spec = np.interp(x_new, x_old, spec)
                            spectra_for_point.append(np.asarray(spec, dtype=float))

                        if not spectra_for_point:
                            continue

                        # Do głównego pliku zapisujemy tylko widmo dla pierwszego czasu
                        primary_spectrum = spectra_for_point[0]
                        writer.writerow([grid_x, grid_y] + primary_spectrum.tolist())

                        # Zapis pliku punktowego: lambda, I_t1, I_t2, ...
                        try:
                            point_file = os.path.join(points_folder, f"point_x{grid_x}_y{grid_y}.csv")
                            with open(point_file, "w", newline="") as pf:
                                pw = csv.writer(pf)
                                header = ["lambda"] + [f"I_{exp:.1f}ms" for exp in exposures_ms]
                                pw.writerow(header)

                                for idx in range(axis_len):
                                    row = [float(axis_vals[idx])]
                                    for spec in spectra_for_point:
                                        row.append(float(spec[idx]))
                                    pw.writerow(row)
                        except Exception as e:
                            print(f"Error writing point file for ({grid_x},{grid_y}): {e}")

            created += 1
            print(
                f"Generated sequence-like files for session {session_id}: "
                f"{filename} + {points_folder} (points: {total_points}, grid {points_x}x{points_y})"
            )
        except Exception as e:
            print(f"Error writing file {filename}: {e}")

    print(f"Generated {created} sequence-like measurement file(s) in '{folder}'")


if __name__ == "__main__":
    # Zmień count, jeśli chcesz więcej plików na jedno uruchomienie
    generate_sequence_like_measurements(count=1)
