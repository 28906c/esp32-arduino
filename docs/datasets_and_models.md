# 声语同行 — 数据集与模型路线

## 一、现状声明

M3 阶段不做真实语音识别（ASR）或语音合成（TTS）。原因：

- ESP32-P4 不适合跑通用大模型 ASR
- 失语症患者语音无公开数据集
- 第一版应验证完整流程，非准确率

M5 引入真实 ASR/TTS，路线如下。

---

## 二、数据集路线

### ASR 训练数据

| 数据集 | License | 规模 | 用途 |
|--------|---------|------|------|
| AISHELL-1 | Apache 2.0 | 178小时, 400人 | 普通话基座 ASR |
| Mozilla Common Voice zh-CN | CC0 | ~100小时+ | 口音泛化/真实用户 |

合并上述两个数据集，与项目词表（训练模块目标词约 200 词）做适配：

```
AISHELL-1 + Common Voice → 过滤 → 项目词表对齐 → 小词表分类模型
```

### TTS 训练数据

| 数据集 | License | 规模 | 用途 |
|--------|---------|------|------|
| AISHELL-3 | Apache 2.0 | 85小时, 218人, 88035条 | 多说话人中文 TTS |

### 不使用的数据

- 无公开可用的中文失语症患者语音数据集
- 不宣称模型适配失语症
- 只能说"面向康复场景，设备端逐步采集"

---

## 三、端侧部署路线

### ASR（命名训练/小词表识别）

```
ESP32-P4:
  音频输入 → AFE (AEC/NS/VAD) → MFCC/Log-Mel → ESP-DL 小模型 → target_word + confidence
```

- **不做通用语音转文字**
- 做 **小词表分类**（200-500 目标词）
- 模型: DTW 模板匹配 或 小 CNN分类器
- 量化: INT8
- 框架: ESP-DL / ESP-SR

### TTS（提示音播放）

```
AISHELL-3 → PC 端离线训练 → 预生成 wav → TF 卡 /assets/prompts/
ESP32: 只播放，不合成
```

- 格式: 16kHz, 16bit, mono, PCM
- 框架: ESP-IDF I2S driver / audio pipeline

---

## 四、训练流程 (PC 端)

1. 下载 AISHELL-1 + Common Voice zh-CN
2. 预处理: 16kHz 降采样, VAD 切段, 去噪
3. 项目词表对齐: 筛选包含目标词的音频段
4. 训练小词表分类模型（MobileNetV2/DTW）
5. INT8 量化
6. 导出 ESP-DL 格式 (.dlc)
7. 部署到 ESP32-P4

---

## 五、里程碑映射

| 里程碑 | ASR | TTS |
|--------|-----|-----|
| M3 | 架构+接口 | 架构+接口 |
| M4 | 音频子系统(AFE/VAD) | 预生成提示音播放 |
| M5 | 小词表模型部署 | 多提示音管理 |
| M6 | 端侧采集数据回传 | PC 端 TTS 工具链 |
| M7 | 模型迭代 | TF 卡资源包 |

---

## 六、参考资料

- AISHELL: https://www.aishelltech.com/
- Common Voice: https://commonvoice.mozilla.org/
- ESP-DL: https://github.com/espressif/esp-dl
- ESP-SR: https://github.com/espressif/esp-sr
