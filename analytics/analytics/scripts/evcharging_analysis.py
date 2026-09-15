"""Compatibility entry: delegates to the unified analysis script."""
from pathlib import Path
import runpy
import sys

target=Path(__file__).resolve().parents[2]/"scripts"/"evcharging_analysis.py"
sys.path.insert(0,str(target.parent))
if __name__=="__main__":
    runpy.run_path(str(target),run_name="__main__")
