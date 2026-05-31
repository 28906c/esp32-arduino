#!/usr/bin/env python3
# prepare_speech_dataset.py — AISHELL-1 / Common Voice preprocessor stub
# M3: defines the pipeline structure. M5: implements actual download + processing.

import argparse
import os

# Project target word list (命名训练 + 听理解)
TARGET_WORDS = {
    "苹果", "杯子", "太阳", "钥匙", "电话",
    "香蕉", "橘子", "西瓜", "跑步", "睡觉",
    "看书", "红色", "阳光", "下雨", "下雪",
    "刮风", "喝水", "吃饭", "走路", "大声",
}

def main():
    p = argparse.ArgumentParser(description="Prepare speech dataset for aphasia rehab")
    p.add_argument("--aishell1", help="Path to AISHELL-1 root")
    p.add_argument("--commonvoice", help="Path to Common Voice zh-CN root")
    p.add_argument("--out", default="./data/speech", help="Output directory")
    args = p.parse_args()

    print(f"[M3 STUB] Speech dataset preparer")
    print(f"  Target words: {len(TARGET_WORDS)}")
    print(f"  AISHELL-1: {args.aishell1 or '(not specified)'}")
    print(f"  CommonVoice: {args.commonvoice or '(not specified)'}")
    print(f"  Output: {args.out}")
    print(f"\n  M5 will implement:")
    print(f"    1. Download datasets")
    print(f"    2. Filter by target word list")
    print(f"    3. VAD segmentation")
    print(f"    4. 16kHz mono wav export")
    print(f"    5. Train/val/test split")

    os.makedirs(args.out, exist_ok=True)

if __name__ == "__main__":
    main()
