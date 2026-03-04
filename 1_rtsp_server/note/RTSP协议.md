# 1、RTSP抓包分析
1、启动自定义rtsp服务器
服务器进程输出：
```bash
PS C:\Users\23590\Documents\av_study\1_rtsp_server\build> ."C:/Users/23590/Documents/av_study/1_rtsp_server/build/main.exe"
C:\Users\23590\Documents\av_study\1_rtsp_server\src\main.cpp rtsp:127.0.0.1:8554
```

2、使用ffplay连接服务器
命令：```ffplay -i rtsp://127.0.0.1:8554```

3、bug记录，启动服务器之后，客户端连接服务器报错```rtsp://127.0.0.1:8554: Invalid data found when processing input```这是由于将```line = strtok(NULL, sep); ```中的strtok函数错写成strstr从而报错segment fault
# 2、服务器支持h264视频拉流
## 1、h.264格式文件描述
1、Network Abstraction Layer Unit, NALU, 即网络抽象层单元。他是h.264码流里的最小网络传输单元。码流组成：起始码 + NAL header + payload。NALU只有header和payload

2、起始码：00 00 01 或者 00 00 00 01

3、NAL header(1字节)构成: forbidden_bit(1), nal_ref_idc(2), nal_unit_type(5).
```c
 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-
|F|NRI|  Type  |
+-+-+-+-+-+-+-+-
```
4、常见nal_unit_type介绍
| 值  |  含义 | 全称 |
| ------------ | ------------ | ------------ |
| 1  |  非IDR帧 | |
| 5  |  IDR帧 | Instantaneous Decoding Refresh(瞬时解码刷新帧) |
| 6  |  SEI  |  Supplemental Enhancement Infomation(补充增强信息) |
| 7  |  SPS  | Sequence Parameter Set(序列参数集) |
| 8  |  PPS  | Picture  Parameter Set(图像参数集) |

## 2、RTP传输H264码流数据的打包方式
### 1、单一打包，一个NALU对应一个RTP包
### 2、分片打包，一个NALU拆分成多个RTP包
FU，全称Fragmentation Unit，分片单元，它由两部分组成，分别是FU Indicator和FU Header，两者的详细描述见下。
这两者位于rtp payload的前两个字节。

1、FU Indicator(1 byte)
```c
+-+-+-+-+-+-+-+
|F|NRI|Type=28|
+-+-+-+-+-+-+-+
```
F: 禁止位；NRI: 重要性；Type=28(表示FU-A，常用)；Type=29(表示FU-B)

2、FU Header(1 byte)
```c
+-+-+-+-+-+-+-+-+
|S|E|R| Type    |
+-+-+-+-+-+-+-+-+
```
S = Start bit；E = End bit；R = Reserved；Type = 原始 NALU 的 type

3、RTP_MAX_PKT_SIZE 取值分析
代码中
```c
#define RTP_MAX_PKT_SIZE 1400
```
这是由于MTU(Maximum Transmission Unit, 最大传输单元，一次链路层能传输的最大数据大小)按照以太网标准设置为1500字节。IP头部通常为20字节，UDP头部固定8字节，RTP头部通常12字节，那么真是可用数据负载大小为
```c
可用payload=1500-20-8-12=1460字节
```

为什么实际常用 1400 左右？
因为：
防止隧道封装（VPN）
防止 PPPoE
防止 VLAN tag
防止路径 MTU 更小
工程里常用：
1200 ~ 1400
WebRTC 甚至常用：
1200
因为要适配公网复杂网络。
