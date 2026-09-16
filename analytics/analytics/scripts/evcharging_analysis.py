"""兼容旧目录的分析入口，实际转交给唯一的新分析脚本执行。

输入：原样接收命令行参数。
输出/接口：运行 ``analytics/scripts/evcharging_analysis.py``，本文件不重复维护分析逻辑。
"""
from pathlib import Path
import runpy
import sys

target=Path(__file__).resolve().parents[2]/"scripts"/"evcharging_analysis.py"
sys.path.insert(0,str(target.parent))
if __name__=="__main__":
    runpy.run_path(str(target),run_name="__main__")
