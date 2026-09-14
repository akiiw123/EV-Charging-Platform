import os

bind = os.getenv("ANALYTICS_BIND", "0.0.0.0:8091")
workers = int(os.getenv("ANALYTICS_WORKERS", "2"))
threads = int(os.getenv("ANALYTICS_THREADS", "4"))
timeout = int(os.getenv("ANALYTICS_TIMEOUT", "30"))
accesslog = "-"
errorlog = "-"
capture_output = True

