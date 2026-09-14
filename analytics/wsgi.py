"""WSGI entry point for development and Gunicorn."""

from analytics_api import create_app

app = create_app()


if __name__ == "__main__":
    import os

    app.run(
        host=os.getenv("ANALYTICS_HOST", "0.0.0.0"),
        port=int(os.getenv("ANALYTICS_PORT", "8091")),
        debug=False,
    )

