"""Verify DataV geometry follows actual cards without a window resize."""
import os
from playwright.sync_api import sync_playwright

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True, **({'executable_path': os.environ['PLAYWRIGHT_CHROMIUM_EXECUTABLE']} if os.getenv('PLAYWRIGHT_CHROMIUM_EXECUTABLE') else {}))
    try:
        page = browser.new_page(viewport={'width': 1440, 'height': 900})
        errors = []
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.goto(os.getenv('DASHBOARD_URL', 'http://127.0.0.1:8091/dashboard/'), wait_until='networkidle')
        def check(selector):
            page.wait_for_function('''selector => [...document.querySelectorAll(selector)].every(card => {
              const svg=card.querySelector(':scope > svg');
              const path=svg?.querySelector('defs path');
              if (!svg || !path) return false;
              const box=path.getBBox();
              return Math.abs(Number(svg.getAttribute('width'))-card.clientWidth)<=1
                && Math.abs(Number(svg.getAttribute('height'))-card.clientHeight)<=1
                && Math.abs(box.width-(card.clientWidth-5))<=1
                && Math.abs(box.height-(card.clientHeight-5))<=1;
            })''', arg=selector)
        for theme in ['day', 'night']:
            for _ in range(3):
                if page.evaluate('document.documentElement.dataset.theme') == theme:
                    break
                page.locator('button[data-theme-mode]').click()
            assert page.evaluate('document.documentElement.dataset.theme') == theme
            for repeat in range(3):
                page.locator('.kpi-grid').hover()
                page.wait_for_timeout(320)
                assert page.locator('.kpi-grid').bounding_box()['height']>=100
                check('.kpi-card')
                if theme=='day' and repeat==0:
                    page.locator('.kpi-grid').screenshot(path=os.getenv('CARD_RESIZE_SCREENSHOT','/tmp/card-resize-expanded.png'))
                page.locator('.topbar h1').hover()
                page.wait_for_timeout(320)
                check('.kpi-card')
                for side in ['left', 'right']:
                    rail=page.locator(f'.chart-column--{side}')
                    rail.hover();page.wait_for_timeout(320)
                    check(f'.chart-column--{side} > .dashboard-card')
                    page.locator('.topbar h1').hover();page.wait_for_timeout(650)
                    check(f'.chart-column--{side} > .dashboard-card')
                    dock=page.locator(f'.performance-dock--{side}')
                    dock.focus();page.wait_for_timeout(350)
                    assert dock.bounding_box()['height']>=295
                    check(f'.performance-dock--{side}')
                    page.locator('.top-actions button').first.focus();page.wait_for_timeout(350)
                    check(f'.performance-dock--{side}')
        assert not errors, errors
        print('PASS: both themes, three expansion/collapse cycles, KPI/side rails/docks; SVG dimensions and path bounds match, no page errors')
    finally:
        browser.close()
