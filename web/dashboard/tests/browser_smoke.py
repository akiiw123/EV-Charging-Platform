"""Smoke-test the built Vue/DataV dashboard against a running Flask service."""

import os

from playwright.sync_api import sync_playwright


url = os.getenv("DASHBOARD_URL", "http://127.0.0.1:8091/dashboard/")
with sync_playwright() as playwright:
    browser = playwright.chromium.launch(channel="chrome", headless=True)
    page = browser.new_page(viewport={"width": 1920, "height": 1080})
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    page.goto(url, wait_until="networkidle")
    page.wait_for_function("document.querySelectorAll('.chart-canvas canvas').length >= 9")
    page.wait_for_function("document.querySelector('#map')?.dataset.renderedStations === '3460'")
    assert page.get_by_text("分析接口在线").is_visible()
    assert page.locator(".dv-border-box-8").count() >= 10
    assert not errors, errors

    page.set_viewport_size({"width": 390, "height": 844})
    page.wait_for_timeout(500)
    overflow = page.evaluate("document.documentElement.scrollWidth - document.documentElement.clientWidth")
    assert overflow <= 1, overflow
    browser.close()

print("Vue/DataV dashboard: API online, 9 charts, 3460 stations, responsive layout")
