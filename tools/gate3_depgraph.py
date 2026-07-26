#!/usr/bin/env python3
"""Gate 3: dependency graph from HF config + architecture templates (config/depgraph-templates.json)."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CFG = ROOT / "config" / "kimi-linear-48b-proxy.config.json"
TEMPLATES = json.loads((ROOT / "config" / "depgraph-templates.json").read_text())


def pick_template(cfg, path):
    mt = (cfg.get("model_type") or "").lower()
    ne = cfg.get("num_experts") or cfg.get("n_routed_experts") or 0
    nl = cfg.get("num_hidden_layers") or cfg.get("n_layer") or 0
    if cfg.get("block_attn_res") or mt in ("kimi_k3", "k3") or ne >= 512 or nl > 60:
        return TEMPLATES["kimi_k3"]
    if cfg.get("proxy_for") or mt in ("kimi_linear", "kimi-linear") or nl <= 40:
        return TEMPLATES["kimi_linear"]
    return TEMPLATES["kimi_k3"]


def is_proxy(cfg, path):
    if cfg.get("proxy_for") or "proxy" in path.name.lower():
        return True
    return path.name == "config.json" and cfg.get("model_type") == "kimi_linear"


def layers(n, tpl):
    bs = max(1, n // 8)
    out = []
    for i in range(n):
        L = {"layer": i, "between_experts": tpl["between_experts"],
             "expert_shardable": tpl["expert_shardable"], "local_stateful": tpl["local_stateful"]}
        if tpl["block_attn_res"]:
            L["block_attn_res"] = {"block_index": i // bs, "boundary": (i + 1) % bs == 0}
        out.append(L)
    return out


def main():
    cfg_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CFG
    out = ROOT / "results" / "gate3.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    if not cfg_path.exists():
        r = {"gate": 3, "verdict_status": "no_config", "trust_verdict": False,
             "contiguous_segments_possible": False}
        out.write_text(json.dumps(r, indent=2))
        (ROOT / "results" / "gate3_depgraph.json").write_text(out.read_text())
        print(f"Wrote {out}"); return
    cfg = json.loads(cfg_path.read_text())
    tpl = pick_template(cfg, cfg_path)
    proxy = is_proxy(cfg, cfg_path)
    nl = cfg.get("num_hidden_layers") or cfg.get("n_layer") or 0
    status = "provisional-on-proxy-config" if proxy else "from-config"
    ev = "Attention, router, shared expert local between expert calls."
    if tpl["block_attn_res"]:
        ev += " Block AttnRes block history stays on requester."
    r = {
        "gate": 3, "verdict_status": status, "trust_verdict": not proxy and tpl["id"] == "kimi_k3",
        "config_path": str(cfg_path), "config_model_type": cfg.get("model_type"),
        "proxy_for": cfg.get("proxy_for"), "architecture_template": tpl["id"],
        "architecture_label": tpl["label"],
        "checkpoint_target": "Kimi K3 (2.8T) — NOT answered by this proxy config" if proxy else "Config-native checkpoint",
        "num_layers": nl, "num_experts": cfg.get("num_experts") or cfg.get("n_routed_experts"),
        "layers": layers(nl, tpl), "contiguous_segments_possible": False,
        "round_trip_boundaries": "every MoE layer" + ("; Block AttnRes boundaries" if tpl["block_attn_res"] else ""),
        "evidence": ev,
        "re_run_when": "Real K3 HF config when weights land" if proxy else None,
    }
    if note := tpl.get("block_attn_res_note"):
        r["block_attn_res_note"] = note
    text = json.dumps(r, indent=2)
    out.write_text(text)
    (ROOT / "results" / "gate3_depgraph.json").write_text(text)
    print(f"Wrote {out} ({status}, template={tpl['id']})")

if __name__ == "__main__":
    main()
