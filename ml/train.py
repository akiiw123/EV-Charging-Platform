"""训练入口: 在 UrbanEV zone 级数据上训练多步分位数负荷预测模型。

用法（在 ml 目录下）:
    python train.py --data-dir ./data --epochs 8
快速体验（少量站点、CPU 可跑）:
    python train.py --data-dir ./data --zones 30 --epochs 3
产物写入 ./artifacts: model.pt（权重） + meta.json（配置/归一化统计量/站点表）。
"""

from __future__ import annotations

import argparse
import json
import random
import time
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np
import pandas as pd
import torch
from torch import nn
from torch.utils.data import DataLoader

from data import ForecastDataset, build_features, load_urbanev, zone_token
from model import QuantileLoss, LoadForecaster

SCRIPT_DIR = Path(__file__).resolve().parent
REPORT_HORIZONS = (1, 6, 24)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="UrbanEV 负荷预测模型训练")
    parser.add_argument("--data-dir", type=Path, default=SCRIPT_DIR / "data",
                        help="UrbanEV data 目录（含 volume/occupancy/weather 等 csv）")
    parser.add_argument("--artifacts", type=Path, default=SCRIPT_DIR / "artifacts",
                        help="模型产物输出目录")
    parser.add_argument("--seq-len", type=int, default=24, help="历史窗口小时数")
    parser.add_argument("--horizon", type=int, default=24, help="预测小时数")
    parser.add_argument("--epochs", type=int, default=10)
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--hidden", type=int, default=64)
    parser.add_argument("--layers", type=int, default=2)
    parser.add_argument("--emb-dim", type=int, default=16)
    parser.add_argument("--dropout", type=float, default=0.1)
    parser.add_argument("--quantiles", type=str, default="0.05,0.5,0.95")
    parser.add_argument("--stride", type=int, default=1, help="滑窗采样步长（>1 可加速训练）")
    parser.add_argument("--zones", type=int, default=0,
                        help="参与训练的站点数（0=全部，等间隔抽样）")
    parser.add_argument("--train-ratio", type=float, default=0.7)
    parser.add_argument("--val-ratio", type=float, default=0.1)
    parser.add_argument("--patience", type=int, default=3, help="早停耐心值")
    parser.add_argument("--num-workers", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--device", type=str, default="auto", choices=["auto", "cpu", "cuda"])
    return parser.parse_args()


def set_seed(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    if torch.cuda.is_available():
        torch.cuda.manual_seed_all(seed)


def pick_device(choice: str) -> torch.device:
    if choice == "cuda" and torch.cuda.is_available():
        return torch.device("cuda")
    if choice == "auto" and torch.cuda.is_available():
        return torch.device("cuda")
    return torch.device("cpu")


def run_epoch(
    model: nn.Module,
    loader: DataLoader,
    loss_fn: QuantileLoss,
    device: torch.device,
    optimizer: torch.optim.Optimizer | None = None,
) -> float:
    training = optimizer is not None
    model.train() if training else model.eval()
    total, count = 0.0, 0
    with torch.set_grad_enabled(training):
        for batch in loader:
            past = batch["past"].to(device)
            future = batch["future"].to(device)
            target = batch["target"].to(device)
            zone = batch["zone"].to(device)
            loss = loss_fn(model(past, future, zone), target)
            if training:
                optimizer.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(model.parameters(), 5.0)
                optimizer.step()
            total += float(loss.item()) * past.size(0)
            count += past.size(0)
    return total / max(count, 1)


@torch.no_grad()
def collect_predictions(
    model: nn.Module, loader: DataLoader, device: torch.device
) -> Tuple[np.ndarray, np.ndarray]:
    model.eval()
    preds, targets = [], []
    for batch in loader:
        pred = model(
            batch["past"].to(device), batch["future"].to(device), batch["zone"].to(device)
        )
        preds.append(pred.cpu().numpy())
        targets.append(batch["target"].numpy())
    return np.concatenate(preds), np.concatenate(targets)   # (N, H, 2, Q), (N, H, 2)


def summarize_channel(
    pred_q: np.ndarray, truth: np.ndarray, quantiles: np.ndarray, horizons: Tuple[int, ...]
) -> Dict[str, Dict[str, float]]:
    """pred_q (N, H, Q) 已按分位数排序; truth (N, H)。"""
    point_idx = int(np.argmin(np.abs(quantiles - 0.5)))
    point = pred_q[..., point_idx]
    lower, upper = pred_q[..., 0], pred_q[..., -1]

    def metrics(prediction: np.ndarray, actual: np.ndarray) -> Dict[str, float]:
        error = prediction - actual
        return {
            "mae": float(np.mean(np.abs(error))),
            "rmse": float(np.sqrt(np.mean(error ** 2))),
        }

    def picp(pred_lower: np.ndarray, pred_upper: np.ndarray, actual: np.ndarray) -> float:
        return float(np.mean((actual >= pred_lower) & (actual <= pred_upper)))

    report: Dict[str, Dict[str, float]] = {
        "overall": {**metrics(point, truth), "picp": picp(lower, upper, truth)}
    }
    for h in horizons:
        report[f"h+{h}"] = {
            **metrics(point[:, h - 1], truth[:, h - 1]),
            "picp": picp(lower[:, h - 1], upper[:, h - 1], truth[:, h - 1]),
        }
    return report


def main() -> None:
    args = parse_args()
    set_seed(args.seed)
    device = pick_device(args.device)
    quantiles = np.sort(np.array([float(q) for q in args.quantiles.split(",")], dtype=np.float64))
    print(f"设备: {device}  分位数: {quantiles.tolist()}")

    data = load_urbanev(args.data_dir)
    T = len(data.times)
    train_end = int(T * args.train_ratio)
    val_end = train_end + int(T * args.val_ratio)
    print(f"数据: {T} 小时 x {len(data.zones)} 个站点 "
          f"({data.times[0]} ~ {data.times[-1]}); "
          f"训练 [0,{train_end}) 验证 [{train_end},{val_end}) 测试 [{val_end},{T})")

    past, future, targets, stats = build_features(data, train_end)

    zones = list(data.zones)
    if args.zones and args.zones < len(zones):
        picked = np.linspace(0, len(zones) - 1, args.zones).astype(int)
        past = past[:, picked]
        future = future[:, picked]
        targets = targets[:, picked]
        zones = [zones[i] for i in picked]
        print(f"按 --zones 抽样 {len(zones)} 个站点参与训练")

    loaders = {
        name: DataLoader(
            ForecastDataset(past, future, targets, args.seq_len, args.horizon, region,
                            stride=args.stride if name == "train" else 1),
            batch_size=args.batch_size,
            shuffle=name == "train",
            num_workers=args.num_workers,
        )
        for name, region in (
            ("train", (0, train_end)),
            ("valid", (train_end, val_end)),
            ("test", (val_end, T)),
        )
    }
    for name, loader in loaders.items():
        print(f"{name} 样本数: {len(loader.dataset)}")
        if len(loader.dataset) == 0:
            raise SystemExit(
                f"{name} 集为空: 该时间段不足 seq_len+horizon={args.seq_len + args.horizon} 小时; "
                "可减小 --seq-len/--horizon 或继续积累数据"
            )

    model = LoadForecaster(
        past_dim=past.shape[2],
        future_dim=future.shape[2],
        horizon=args.horizon,
        num_zones=len(zones),
        quantiles=tuple(float(q) for q in quantiles),
        hidden_size=args.hidden,
        num_layers=args.layers,
        emb_dim=args.emb_dim,
        dropout=args.dropout,
    ).to(device)
    print(f"模型参数量: {sum(p.numel() for p in model.parameters()):,}")

    loss_fn = QuantileLoss(tuple(float(q) for q in quantiles)).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr)

    best_val, best_state, bad_epochs = float("inf"), None, 0
    for epoch in range(1, args.epochs + 1):
        started = time.time()
        train_loss = run_epoch(model, loaders["train"], loss_fn, device, optimizer)
        val_loss = run_epoch(model, loaders["valid"], loss_fn, device)
        cost = time.time() - started
        marker = ""
        if val_loss < best_val:
            best_val, bad_epochs = val_loss, 0
            best_state = {k: v.detach().cpu().clone() for k, v in model.state_dict().items()}
            marker = " *"
        else:
            bad_epochs += 1
        print(f"epoch {epoch:03d}/{args.epochs}  train={train_loss:.4f}  "
              f"valid={val_loss:.4f}  {cost:.1f}s{marker}")
        if bad_epochs >= args.patience:
            print(f"连续 {args.patience} 轮验证损失未下降，提前停止")
            break

    if best_state is not None:
        model.load_state_dict(best_state)

    pred, truth = collect_predictions(model, loaders["test"], device)
    pred_q = np.sort(pred, axis=-1)                                    # 保证分位数单调

    load_report = summarize_channel(
        stats.decode_load(pred_q[..., 0, :]),
        stats.decode_load(truth[..., 0]),
        quantiles, REPORT_HORIZONS,
    )
    busy_report = summarize_channel(pred_q[..., 1, :], truth[..., 1], quantiles, REPORT_HORIZONS)

    def render(name: str, unit: str, report: Dict[str, Dict[str, float]]) -> str:
        lines = [f"  {name}（{unit}）"]
        for scope, m in report.items():
            lines.append(
                f"    {scope:<8} MAE={m['mae']:.4f}  RMSE={m['rmse']:.4f}  "
                f"PICP={m['picp']:.2%}"
            )
        return "\n".join(lines)

    print("\n== 测试集评估 ==")
    print(render("负荷", "kWh", load_report))
    print(render("忙桩比例", "0-1", busy_report))

    args.artifacts.mkdir(parents=True, exist_ok=True)
    torch.save({"state_dict": model.state_dict()}, args.artifacts / "model.pt")
    meta = {
        "dataset": "UrbanEV",
        "created_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "seq_len": args.seq_len,
        "horizon": args.horizon,
        "quantiles": [float(q) for q in quantiles],
        "zones": zones,
        "pile_totals": {z: int(data.pile_totals.get(z, 0)) for z in zones},
        "stats": stats.to_dict(),
        "model": {
            "past_dim": int(past.shape[2]),
            "future_dim": int(future.shape[2]),
            "hidden_size": args.hidden,
            "num_layers": args.layers,
            "emb_dim": args.emb_dim,
            "dropout": args.dropout,
            "num_zones": len(zones),
        },
        "data_range": [str(data.times[0]), str(data.times[-1])],
        "test_metrics": {"load_kwh": load_report, "busy_ratio": busy_report},
    }
    with open(args.artifacts / "meta.json", "w", encoding="utf-8") as fh:
        json.dump(meta, fh, ensure_ascii=False, indent=2)
    print(f"\n产物已保存: {args.artifacts / 'model.pt'}  {args.artifacts / 'meta.json'}")
    print("启动推理服务: python service.py --data-dir ./data")


if __name__ == "__main__":
    main()
