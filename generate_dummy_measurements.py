import os
import csv
import time
import numpy as np

"""Prosty skrypt do wygenerowania wielu plików wynikowych
w katalogu `measurement_data`, kompatybilnych z HeatMapWindow.

Uruchom z katalogu projektu:

    python generate_dummy_measurements.py

Domyślnie tworzy 100 plików dummy_XXX_spectra.csv z siatką 10x10
i sztucznym, gładkim widmem (jedno maksimum przesuwające się
pomiędzy plikami).
"""


def generate_dummy_measurements(count: int = 100, nx: int = 10, ny: int = 10) -> None:
    folder = "measurement_data"
    os.makedirs(folder, exist_ok=True)

    axis_len = 2048  # długość widma – zgodna z kamerą / kodem
    wl_norm = np.linspace(0.0, 1.0, axis_len)

    created = 0

    for i in range(count):
        # Każdy plik ma inne bazowe widmo (maksimum przesuwa się między plikami)
        if count > 1:
            center = 0.15 + 0.7 * (i / (count - 1))
        else:
            center = 0.5
        width = 0.08
        base_profile = np.exp(-((wl_norm - center) ** 2) / (2 * width * width)) * 1000.0

        # Nadaj nazwę pliku w takim samym stylu jak sekwencja w index copy:
        # measurement_YYYYMMDD_HHMMSS_spectra.csv
        ts = time.strftime('%Y%m%d_%H%M%S')
        if count == 1:
            filename = os.path.join(folder, f"measurement_{ts}_spectra.csv")
        else:
            filename = os.path.join(folder, f"measurement_{ts}_{i+1:03d}_spectra.csv")
        try:
            with open(filename, "w", newline="") as f:
                writer = csv.writer(f)
                # Zapis punktów w snake pattern (tak jak logicznie idzie sekwencja skanu):
                # pierwszy wiersz 0..nx-1, drugi wiersz nx-1..0, itd.
                for iy in range(ny):
                    if iy % 2 == 0:
                        x_range = range(nx)
                    else:
                        x_range = range(nx - 1, -1, -1)

                    for ix in x_range:
                        # Dla każdego punktu (ix, iy) dodatkowo modyfikujemy widmo,
                        # żeby było wyraźnie inne:
                        # - amplituda zależy od (ix, iy)
                        # - dodajemy mały lokalny pik przesunięty wg indeksu punktu
                        scale = 0.5 + 0.5 * ((ix + iy) / max(1, (nx + ny - 2)))
                        spectrum = base_profile * scale

                        point_index = iy * nx + (ix if iy % 2 == 0 else (nx - 1 - ix))
                        peak_pos = int((point_index / max(1, nx * ny - 1)) * (axis_len - 1))
                        peak_width = max(3, axis_len // 200)
                        for k in range(max(0, peak_pos - peak_width), min(axis_len, peak_pos + peak_width)):
                            spectrum[k] += 500.0

                        writer.writerow([ix, iy] + spectrum.tolist())
            created += 1
        except Exception as e:
            print(f"Error writing dummy file {filename}: {e}")

    print(f"Generated {created} dummy measurement files in '{folder}'")


if __name__ == "__main__":
    generate_dummy_measurements()
