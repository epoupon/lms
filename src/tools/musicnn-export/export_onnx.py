#!/usr/bin/env python3
"""
Export MusicNN (oriyonay/musicnn-pytorch) to ONNX, stopping at the 200-D embedding
(after fc1 + BN, before the fc2 classification head).

Everything (model code + weights) is downloaded automatically from HuggingFace.
No git clone required.

Input tensor  : mel_patch  [1, 187, 96]   float32
                            batch=1, T=187 frames, mel=96 bands
                            (log-mel spectrogram, pre-BN — bn_input is inside the graph)
Output tensor : embedding  [1, 200]        float32

Usage:
    pip install torch huggingface_hub onnx onnxruntime
    cd tools/musicnn
    python export_onnx.py --model MSD_musicnn --output MSD_musicnn_embedding.onnx
"""

import argparse
import importlib.util
import sys
from pathlib import Path

import torch
import torch.nn as nn
import torch.nn.functional as F

HF_REPO = "oriyonay/musicnn-pytorch"
HF_REVISION = "394be17b3a5c2c2e1bb8a6593cfc3c5557eb8a82"

# ---------------------------------------------------------------------------
# Download musicnn_torch.py from HuggingFace if needed
# ---------------------------------------------------------------------------
def _import_musicnn_torch() -> type:
    """Import MusicNN from the HF repo's musicnn_torch.py (downloaded on demand).

    musicnn_torch.py imports librosa and soundfile at module level, but those are
    only needed by the audio-loading helpers (batch_data, extractor, top_tags).
    We stub them out so the import succeeds without those optional dependencies.
    """
    import sys
    import types

    try:
        from huggingface_hub import hf_hub_download
    except ImportError:
        print("ERROR: huggingface_hub not installed. Run: pip install huggingface_hub", file=sys.stderr)
        sys.exit(1)

    # Stub out optional audio-loading dependencies that musicnn_torch.py imports
    # at module level but that we don't need for model-architecture access.
    for mod_name in ("librosa", "librosa.feature", "soundfile"):
        if mod_name not in sys.modules:
            sys.modules[mod_name] = types.ModuleType(mod_name)

    local_py = hf_hub_download(repo_id=HF_REPO, filename="musicnn_torch.py", revision=HF_REVISION)
    spec = importlib.util.spec_from_file_location("musicnn_torch", local_py)
    module = importlib.util.module_from_spec(spec)   # type: ignore[arg-type]
    spec.loader.exec_module(module)                   # type: ignore[union-attr]
    return module.MusicNN


# ---------------------------------------------------------------------------
# Wrapper: runs the full MusicNN forward up to the 200-D embedding.
#
# MusicNN.forward() from musicnn_torch.py (abridged):
#   x = x.unsqueeze(1)               -- adds channel dim: [B, 1, T, mel]
#   x = bn_input(x)
#   frontend_features = cat([f74, f77, s1, s2, s3], dim=2)   -- [B, T, 561]
#   mid_feats = midend(...)                                   -- list of 4 tensors
#   z = cat(mid_feats, dim=2)                                 -- [B, T, 753]
#   logits, mean_pool, max_pool = backend(z)
#     backend: max_pool + mean_pool interleaved via stack+view -- [B, 1506]
#              bn_in -> fc1 -> relu -> bn_fc1 -> fc2
#   We stop before fc2 and return bn_fc1 output.
# ---------------------------------------------------------------------------
class MusicNNEmbeddingWrapper(nn.Module):
    def __init__(self, model: nn.Module) -> None:
        super().__init__()
        self.model = model

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x: [batch, T=187, mel=96]  (same as musicnn_torch extractor input)
        m = self.model

        # Replicate MusicNN.forward() up to the embedding
        h = x.unsqueeze(1)                                        # [B, 1, T, mel]
        h = m.bn_input(h)
        f74 = m.timbral_1(h).transpose(1, 2)
        f77 = m.timbral_2(h).transpose(1, 2)
        s1 = m.temp_1(h).transpose(1, 2)
        s2 = m.temp_2(h).transpose(1, 2)
        s3 = m.temp_3(h).transpose(1, 2)
        frontend = torch.cat([f74, f77, s1, s2, s3], dim=2)      # [B, T, 561]
        mid_feats = m.midend(frontend.transpose(1, 2))            # list of 4 tensors
        z = torch.cat(mid_feats, dim=2)                           # [B, T, 753]

        # Backend — replicate exactly, stopping before fc2
        be = m.backend
        max_pool = torch.max(z, dim=1).values                     # [B, 753]
        mean_pool = torch.mean(z, dim=1)                          # [B, 753]
        # musicnn_torch uses stack+view to interleave, NOT cat
        pooled = torch.stack([max_pool, mean_pool], dim=2)        # [B, 753, 2]
        pooled = pooled.view(pooled.size(0), -1)                  # [B, 1506]  interleaved
        pooled = be.bn_in(pooled)
        pooled = F.relu(be.fc1(pooled))
        embedding = be.bn_fc1(pooled)                             # [B, 200]
        return embedding


def main() -> None:
    parser = argparse.ArgumentParser(description="Export MusicNN to ONNX (embedding output)")
    parser.add_argument("--model", default="MSD_musicnn",
                        choices=["MTT_musicnn", "MSD_musicnn"],
                        help="Which checkpoint to download from HuggingFace (default: MSD_musicnn)")
    parser.add_argument("--output", default="MSD_musicnn_embedding.onnx",
                        help="Output ONNX file path (default: MSD_musicnn_embedding.onnx)")
    parser.add_argument("--opset", type=int, default=17,
                        help="ONNX opset version (default: 17)")
    args = parser.parse_args()

    print("Importing MusicNN architecture from HuggingFace ...", file=sys.stderr)
    MusicNN = _import_musicnn_torch()

    try:
        from huggingface_hub import hf_hub_download
    except ImportError:
        print("ERROR: huggingface_hub not installed. Run: pip install huggingface_hub", file=sys.stderr)
        sys.exit(1)
    hf_path = f"weights/{args.model}.pt"
    print(f"Downloading {hf_path} from {HF_REPO} ...", file=sys.stderr)
    local_path = hf_hub_download(repo_id=HF_REPO, filename=hf_path, revision=HF_REVISION)
    sd = torch.load(local_path, map_location="cpu", weights_only=True)

    # num_classes=50 for both MTT and MSD standard models
    model = MusicNN(num_classes=50)
    model.load_state_dict(sd)
    model.eval()

    wrapper = MusicNNEmbeddingWrapper(model)
    wrapper.eval()

    # Input: [batch=1, T=187, mel=96]
    dummy = torch.zeros(1, 187, 96, dtype=torch.float32)

    with torch.no_grad():
        ref_out = wrapper(dummy)
    print(f"Reference output shape : {ref_out.shape}", file=sys.stderr)
    print(f"Reference output range : [{ref_out.min().item():.4f}, {ref_out.max().item():.4f}]", file=sys.stderr)

    output_path = Path(args.output)
    print(f"Exporting to {output_path} (opset={args.opset}) ...", file=sys.stderr)

    torch.onnx.export(
        wrapper,
        dummy,
        str(output_path),
        input_names=["mel_patch"],
        output_names=["embedding"],
        dynamic_axes=None,          # fixed shape: batch=1 always
        opset_version=args.opset,
        do_constant_folding=True,
        dynamo=False,               # force legacy exporter (no onnxscript dependency)
    )

    size_mb = output_path.stat().st_size / 1024 / 1024
    print(f"Done. {output_path} ({size_mb:.2f} MB)", file=sys.stderr)

    # Quick round-trip check with onnxruntime if available
    try:
        import numpy as np
        import onnxruntime as ort
        sess = ort.InferenceSession(str(output_path), providers=["CPUExecutionProvider"])
        ort_out = sess.run(["embedding"], {"mel_patch": dummy.numpy()})[0]
        max_diff = float(abs(ref_out.numpy() - ort_out).max())
        print(f"ORT round-trip max diff vs PyTorch : {max_diff:.2e}", file=sys.stderr)
        if max_diff > 1e-4:
            print("WARNING: large diff — check the model graph", file=sys.stderr)
        else:
            print("Round-trip OK.", file=sys.stderr)
    except ImportError:
        print("onnxruntime not installed; skipping round-trip check.", file=sys.stderr)


if __name__ == "__main__":
    main()
