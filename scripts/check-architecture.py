#!/usr/bin/env python3
"""ESP32S3の自作コードの依存方向を確認する。外部SDKの内部構造は対象外。"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
LIBRARIES = {"Safety.h", "Bench.h", "WeatherData.h", "Demo.h", "Config.h", "LocalConfig.example.h", "LocalConfig.h"}


# 関数名: layer / 内容: ファイルの責務から依存層を判定する。入力: path。出力: 層番号。
def layer(path):
    """関数名: layer / 内容: 自作ソースを下位0・ドライバー1・アプリ2へ分類する。"""
    if "library" in path.parts or path.name in LIBRARIES:
        return 0
    if "drivers" in path.parts:
        return 1
    return 2


# 関数名: main / 内容: 依存と静的APIを検証し、違反時は例外で停止する。
def main():
    """関数名: main / 内容: ローカルincludeと装置APIの配置を検証する。違反時は例外で停止。"""
    paths = sorted(path for path in FIRMWARE.rglob("*") if path.suffix in {".h", ".ino", ".cpp"} and "build" not in path.relative_to(FIRMWARE).parts)
    for path in paths:
        content = path.read_text()
        for include in re.findall(r'^\s*#include\s+"([^"]+)"', content, re.MULTILINE):
            target = (path.parent / include).resolve()
            if not target.exists() and include == "LocalConfig.h":
                continue
            assert target.exists(), f"Missing include: {path}: {include}"
            assert layer(target) <= layer(path), f"Upward include: {path} -> {target}"
        if layer(path) != 1:
            assert not re.search(r'\b(?:digitalWrite|digitalRead|pinMode|ledcAttach|ledcWrite|esp_task_wdt_\w+)\s*\(', content), path
        if layer(path) == 0:
            assert not re.search(r'#include\s*<(?:Arduino|Matter|WiFi)\.h>', content), path
    application = (FIRMWARE / "xiao_s3/Application.h").read_text()
    assert "xTaskCreateStatic(" in application and "xQueueCreateStatic(" in application
    assert not re.search(r'\b(?:xTaskCreate|xQueueCreate)\s*\(', application)
    driver = (FIRMWARE / "common/drivers/NetworkDriver.h").read_text()
    assert "application::" not in driver
    assert "onChange(receiveChange)" in driver
    print(f"PASS: architecture ({len(paths)} sources), static task/queue creation")


if __name__ == "__main__":
    main()
