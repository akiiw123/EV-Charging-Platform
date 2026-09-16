"""配置 Gunicorn 如何运行 Flask 分析服务。

输入：ANALYTICS_BIND、WORKERS、THREADS、TIMEOUT 等环境变量。
输出/接口：默认在 8091 端口启动多进程多线程服务，并把访问日志输出到控制台。
"""

import os

bind = os.getenv("ANALYTICS_BIND", "0.0.0.0:8091")
workers = int(os.getenv("ANALYTICS_WORKERS", "2"))
threads = int(os.getenv("ANALYTICS_THREADS", "4"))
timeout = int(os.getenv("ANALYTICS_TIMEOUT", "30"))
accesslog = "-"
errorlog = "-"
capture_output = True
