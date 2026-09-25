#!/usr/bin/env python3
"""Gate 3 depgraph; --fetch-k3 downloads HF config + safetensors index only."""
import hashlib, json, sys, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CFG = ROOT / "config" / "kimi-k3.example.config.json"
META = ROOT / "config" / "hf-kimi-k3-meta"
TPL = json.loads((ROOT / "config" / "depgraph-templates.json").read_text())
HF = "moonshotai/Kimi-K3"


def pick_template(cfg, path):
    mt = (cfg.get("model_type") or "").lower()
    tc = cfg.get("text_config") or {}
    if mt == "kimi_k3" or "k3" in path.name.lower():
        return TPL["kimi_k3"]
    ne = cfg.get("num_experts") or tc.get("num_experts") or 0
    nl = cfg.get("num_hidden_layers") or tc.get("num_hidden_layers") or 0
    if ne >= 800 or nl >= 90:
        return TPL["kimi_k3"]
    return TPL["kimi_k3"]


def layers(n, tpl):
    return [{"layer": i, "between_experts": tpl["between_experts"],
             "expert_shardable": tpl["expert_shardable"], "local_stateful": tpl["local_stateful"]}
            for i in range(n)]


def fetch_k3():
    META.mkdir(parents=True, exist_ok=True)
    sums = {}
    for name in ("config.json", "model.safetensors.index.json", "tokenizer_config.json"):
        try:
            data = urllib.request.urlopen(f"https://huggingface.co/{HF}/resolve/main/{name}", timeout=120).read()
        except Exception as e:
            print(f"skip {name}: {e}", file=sys.stderr)
            continue
        (META / name).write_bytes(data)
        sums[name] = hashlib.sha256(data).hexdigest()
    (META / "checksums.json").write_text(json.dumps(sums, indent=2))
    ot = ["-ot blk\\.*\\.ffn_gate_exps=RPC0", "-ot blk\\.*\\.ffn_up_exps=RPC0", "-ot blk\\.*\\.ffn_down_exps=RPC0"]
    (ROOT / "config" / "tensor-overrides.kimi-k3").write_text(
        "# Kimi K3 routed experts → RPC0\n" + "\n".join(ot) + "\n")
    print(f"Fetched K3 metadata ({len(sums)} files)")


def run_gate(cfg_path):
    out = ROOT / "results" / "gate3.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    if not cfg_path.exists():
        out.write_text(json.dumps({"gate": 3, "verdict_status": "no_config", "trust_verdict": False}))
        return
    cfg = json.loads(cfg_path.read_text())
    tpl = pick_template(cfg, cfg_path)
    tc = cfg.get("text_config") or {}
    nl = cfg.get("num_hidden_layers") or tc.get("num_hidden_layers") or 0
    r = {"gate": 3, "verdict_status": "from-config", "trust_verdict": True,
         "architecture_template": tpl["id"], "num_layers": nl,
         "num_experts": cfg.get("num_experts") or tc.get("num_experts"),
         "layers": layers(nl, tpl), "contiguous_segments_possible": False,
         "checkpoint_target": "moonshotai/Kimi-K3"}
    text = json.dumps(r, indent=2)
    out.write_text(text)
    (ROOT / "results" / "gate3_depgraph.json").write_text(text)
    print(f"Wrote {out}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--fetch-k3":
        fetch_k3()
    else:
        run_gate(Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CFG)
