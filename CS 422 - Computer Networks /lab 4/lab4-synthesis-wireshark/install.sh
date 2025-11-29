#!/bin/bash
# install.sh - instala dependencias para lab4 (Linux / macOS)
python3 -m pip install --upgrade pip
python3 -m pip install scapy
# opcional: si quieres imprimir en colores o parseo extra
python3 -m pip install argparse
echo "Instalación completada. Ejecuta: python3 lab4.py [CASE] [pcapfile] [dest_website]"
