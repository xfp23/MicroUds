# MicroUDS 中文文档

轻量级 UDS (统一诊断服务) 协议栈实现

## 特性

- 符合 ISO 14229-1 标准
- 内存占用小

## 目录结构

```
MicroUDS/
├── inc/                  # 公共头文件
│   ├── core/             # 核心功能
│   ├── service/          # UDS 服务 (0x10, 0x11, 0x27 等)
│   ├── transport/isotp/  # ISO-TP 传输层
│   ├── security/         # 安全访问
│   ├── did/              # 数据标识符
│   ├── download/         # 下载/上传
│   ├── dtc/              # 故障码
├── port/                 # 平台适配层
└── doc/                  # 文档
```

## 快速开始

1. 配置 `MicroUds_conf.h`
2. 在 `port/` 目录实现平台相关函数
3. 编译并集成到您的项目

## 许可证

详见 LICENSE 文件
