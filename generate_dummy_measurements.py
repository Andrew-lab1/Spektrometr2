import os
import csv
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
        # Pozycja maksimum przesuwa się wraz z numerem pliku
        if count > 1:
            center = 0.15 + 0.7 * (i / (count - 1))
        else:
            center = 0.5
        width = 0.08
        base_profile = np.exp(-((wl_norm - center) ** 2) / (2 * width * width)) * 1000.0

        filename = os.path.join(folder, f"dummy_{i+1:03d}_spectra.csv")
        try:
            with open(filename, "w", newline="") as f:
                writer = csv.writer(f)
                for iy in range(ny):
                    for ix in range(nx):
                        scale = 0.7 + 0.3 * ((ix + iy) / max(1, (nx + ny - 2)))
                        spectrum = (base_profile * scale).tolist()
                        writer.writerow([ix, iy] + spectrum)
            created += 1
        except Exception as e:
            print(f"Error writing dummy file {filename}: {e}")

    print(f"Generated {created} dummy measurement files in '{folder}'")


if __name__ == "__main__":
    generate_dummy_measurements()
