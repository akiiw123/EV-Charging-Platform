"""向开发服务器和 Gunicorn 暴露 Flask WSGI 应用。

输入：analytics_api.create_app 从环境变量加载的配置。
输出/接口：模块级 ``app`` 对象；直接运行本文件时启动 Flask 开发服务。
"""

from analytics_api import create_app

app = create_app()


if __name__ == "__main__":
    import os

    app.run(
        host=os.getenv("ANALYTICS_HOST", "0.0.0.0"),
        port=int(os.getenv("ANALYTICS_PORT", "8091")),
        debug=False,
    )
