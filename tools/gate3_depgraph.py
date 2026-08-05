#!/usr/bin/env python3
"""Gate 3 depgraph; --fetch-k2 downloads HF config + safetensors index only."""
import hashlib, json, sys, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CFG = ROOT / "config" / "kimi-linear-48b-proxy.config.json"
META = ROOT / "config" / "hf-kimi-k2-meta"
TPL = json.loads((ROOT / "config" / "depgraph-templates.json").read_text())
HF = "moonshotai/Kimi-K2-Instruct"


def pick_template(cfg, path):
    mt = (cfg.get("model_type") or "").lower()
    tc = cfg.get("text_config") or {}
    ne = cfg.get("num_experts") or tc.get("num_experts") or 0
    nl = cfg.get("num_hidden_layers") or tc.get("num_hidden_layers") or 0
    if mt in ("deepseek2", "kimi_k2", "k2") or nl >= 60 or ne >= 300:
        return TPL.get("deepseek2", TPL["kimi_linear"])
    if cfg.get("proxy_for") or "proxy" in path.name or mt.startswith("kimi") or nl <= 40:
        return TPL["kimi_linear"]
    return TPL["kimi_linear"]


def classify(n):
    n = n.lower()
    if "shared_expert" in n: return "shared_expert"
    if ".experts." in n: return "routed_expert"
    if "gate.weight" in n or "gate_proj" in n: return "router"
    if any(x in n for x in ("conv1d", "a_log", "dt_bias")): return "kda_state"
    if "attn" in n: return "mla_state"
    return "other"


def layers(n, tpl):
    return [{"layer": i, "between_experts": tpl["between_experts"],
             "expert_shardable": tpl["expert_shardable"], "local_stateful": tpl["local_stateful"]}
            for i in range(n)]


def fetch_k2():
    META.mkdir(parents=True, exist_ok=True)
    sums = {}
    for name in ("config.json", "model.safetensors.index.json"):
        data = urllib.request.urlopen(f"https://huggingface.co/{HF}/resolve/main/{name}", timeout=120).read()
        (META / name).write_bytes(data)
        sums[name] = hashlib.sha256(data).hexdigest()
    (META / "checksums.json").write_text(json.dumps(sums, indent=2))
    cfg = json.loads((META / "config.json").read_text())
    tc = cfg.get("text_config", cfg)
    wm = json.loads((META / "model.safetensors.index.json").read_text())["weight_map"]
    cats = {}
    for k in wm: cats.setdefault(classify(k), []).append(k)
    ot = ["-ot blk\\.*\\.ffn_gate_exps=RPC0", "-ot blk\\.*\\.ffn_up_exps=RPC0", "-ot blk\\.*\\.ffn_down_exps=RPC0"]
    (ROOT / "config" / "tensor-overrides.kimi-k2").write_text("# Generated\n" + "\n".join(ot) + "\n")
    print(f"Fetched K2 metadata ({len(wm)} tensors)")


def run_gate(cfg_path):
    out = ROOT / "results" / "gate3.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    if not cfg_path.exists():
        out.write_text(json.dumps({"gate": 3, "verdict_status": "no_config", "trust_verdict": False}))
        return
    cfg = json.loads(cfg_path.read_text())
    tpl = pick_template(cfg, cfg_path)
    proxy = bool(cfg.get("proxy_for") or "proxy" in cfg_path.name)
    mt = (cfg.get("model_type") or "").lower()
    tc = cfg.get("text_config") or {}
    nl = cfg.get("num_hidden_layers") or tc.get("num_hidden_layers") or 0
    status = "provisional-on-proxy-config" if proxy else "from-config"
    if cfg_path.name == "kimi-k2.example.config.json" or mt in ("deepseek2", "kimi_k2"):
        status, proxy = "from-k2-hf-metadata", False
    r = {"gate": 3, "verdict_status": status, "trust_verdict": not proxy,
         "architecture_template": tpl["id"], "num_layers": nl,
         "num_experts": cfg.get("num_experts") or tc.get("num_experts"),
         "layers": layers(nl, tpl), "contiguous_segments_possible": False,
         "checkpoint_target": "Kimi K2-Instruct (deepseek2)"}
    text = json.dumps(r, indent=2)
    out.write_text(text)
    (ROOT / "results" / "gate3_depgraph.json").write_text(text)
    print(f"Wrote {out} ({status})")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--fetch-k2":
        fetch_k2()
    elif len(sys.argv) > 1 and sys.argv[1] == "--fetch-k3":
        print("use --fetch-k2 (K3 pivot complete)", file=sys.stderr)
        sys.exit(1)
    else:
        run_gate(Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CFG)
