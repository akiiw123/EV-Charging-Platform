"""通过真实 HTTP 访问正在运行的 Flask 服务，冒烟检查全部分析接口。

输入：ANALYTICS_BASE_URL，默认 http://127.0.0.1:8091。
输出/接口：逐个打印 PASS；状态码、统一响应格式或数据组数量异常时立即失败。
"""

from __future__ import annotations

import json
import os
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from smoke_real_mysql import ENDPOINTS


def main() -> None:
    base_url = os.getenv("ANALYTICS_BASE_URL", "http://127.0.0.1:8091").rstrip("/")
    for endpoint in ENDPOINTS:
        request = Request(
            f"{base_url}{endpoint}",
            headers={"X-Request-ID": "http-smoke"},
        )
        try:
            with urlopen(request, timeout=10) as response:
                payload = json.load(response)
                status = response.status
        except (HTTPError, URLError) as exc:
            raise RuntimeError(f"{endpoint} request failed: {exc}") from exc

        if status != 200 or payload.get("code") != 0:
            raise RuntimeError(
                f"{endpoint} failed: status={status} payload={payload}"
            )
        data = payload.get("data")
        size = len(data) if isinstance(data, (dict, list)) else 0
        print(f"PASS {endpoint} size={size}")


if __name__ == "__main__":
    main()
