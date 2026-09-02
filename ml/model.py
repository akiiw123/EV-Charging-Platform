"""多步分位数负荷预测模型。

结构: LSTM 编码器聚合过去 seq_len 小时特征 -> 上下文向量；对每个未来时刻，
拼接 未来可知特征(时间/天气/价格) 与 站点嵌入 -> 共享 MLP 解码，
同时输出 负荷 与 忙桩比例 的多个分位数（默认 0.05/0.5/0.95，即 90% 置信区间）。
"""

from __future__ import annotations

from typing import Sequence

import torch
from torch import nn

LOAD_CHANNEL = 0   # 输出通道 0: 负荷（归一化）
BUSY_CHANNEL = 1   # 输出通道 1: 忙桩比例 0-1


class QuantileLoss(nn.Module):
    """分位数（pinball）损失: pred (B, H, 2, Q), target (B, H, 2)。"""

    def __init__(self, quantiles: Sequence[float]):
        super().__init__()
        self.register_buffer(
            "quantiles", torch.tensor(sorted(quantiles), dtype=torch.float32)
        )

    def forward(self, prediction: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        error = target.unsqueeze(-1) - prediction                  # (B, H, 2, Q)
        loss = torch.maximum(self.quantiles * error, (self.quantiles - 1.0) * error)
        return loss.mean()


class LoadForecaster(nn.Module):
    """站点级多步分位数预测器（站点 ID 以 embedding 编码，含未知站点槽位）。"""

    def __init__(
        self,
        past_dim: int,
        future_dim: int,
        horizon: int,
        num_zones: int,
        quantiles: Sequence[float] = (0.05, 0.5, 0.95),
        hidden_size: int = 64,
        num_layers: int = 2,
        emb_dim: int = 16,
        dropout: float = 0.1,
    ):
        super().__init__()
        self.horizon = horizon
        self.hidden_size = hidden_size
        self.emb_dim = emb_dim
        self.num_quantiles = len(quantiles)
        # +1 个槽位留给训练时未见过的站点（推理时兜底）
        self.zone_embedding = nn.Embedding(num_zones + 1, emb_dim)
        self.encoder = nn.LSTM(
            input_size=past_dim + emb_dim,
            hidden_size=hidden_size,
            num_layers=num_layers,
            batch_first=True,
            dropout=dropout if num_layers > 1 else 0.0,
        )
        self.decoder = nn.Sequential(
            nn.Linear(hidden_size + emb_dim + future_dim, hidden_size),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(hidden_size, 2 * self.num_quantiles),
        )

    def forward(
        self, past: torch.Tensor, future: torch.Tensor, zone_index: torch.Tensor
    ) -> torch.Tensor:
        """past (B, L, past_dim), future (B, H, future_dim), zone_index (B,)。

        返回 (B, horizon, 2, Q)：通道 0 负荷（归一化），通道 1 忙桩比例。
        """
        batch, seq_len, _ = past.shape
        horizon = future.shape[1]
        emb = self.zone_embedding(zone_index)                       # (B, E)

        enc_in = torch.cat(
            [past, emb.unsqueeze(1).expand(batch, seq_len, self.emb_dim)], dim=-1
        )
        _, (h_n, _) = self.encoder(enc_in)
        context = h_n[-1]                                          # (B, hidden) 顶层最后时刻

        dec_in = torch.cat(
            [
                context.unsqueeze(1).expand(batch, horizon, self.hidden_size),
                future,
                emb.unsqueeze(1).expand(batch, horizon, self.emb_dim),
            ],
            dim=-1,
        )
        out = self.decoder(dec_in)                                 # (B, H, 2*Q)
        return out.view(batch, horizon, 2, self.num_quantiles)
