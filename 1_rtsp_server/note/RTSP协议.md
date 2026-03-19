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

# 3、服务器支持AAC音频拉取
## 3.1 PCM介绍
全称Pulse Code Modulation, 脉冲编码调制，即是将模拟信号经过采样、量化、编码为二进制码的一种技术。
### 3.1.1 采样
采样：对模拟信号进行时间上的离散。按照一定的采样率得到若干个离散的幅值。
- 采样率：每秒对模拟信号采样的个数，单位赫兹Hz
- 采样点：某一时刻的模拟信号幅值被量化后的一个数值。

一个采样点表示在某一时刻对连续模拟信号幅值进行量化后得到的离散数值，其精度由采样位数决定，位数越高，量化级别越多，对原始信号的逼近越精确。
### 3.1.2 量化
量化：对幅值进行数值上的离散。按照一定的采样位数对幅值进行离散化。量化本质：将连续值[1,3]映射到离散集合{0,...,255}。

采样位数有8位或者16位，8位有2^8个阶位可以去逼近振幅，而16位有2^16个阶位可以去逼近，因此，越大的采样位数具有更小的近似误差。
1、采样位数：
**8-bit：**
- 2⁸ = 256 个等级
- 振幅只能取 256 个离散值
👉 波形会“阶梯化”，失真明显
**16-bit：**
- 2¹⁶ = 65536 个等级
- 更精细地逼近真实波形
👉 声音更平滑、更接近原始信号
### 3.1.3 编码
将采样点数值编码为二进制数
### 3.1.4 音频帧
通常1024个采样点构成一个帧，那么44100Hz的话，即1秒钟44100个采样，有44100/1024~=43帧，每帧时长1000/43~=23.3ms
## 3.1 AAC编码格式介绍
全称Advanced Audio Coding，即高级音频编码。有两种格式，
- ADIF（audio data interchange format），只有一个统一的头，必须在得到所有数据之后开始解码；
- ADTS（audio data transport stream），每一帧都有头信息，可以在任意帧进行解码，适用于网络传输场景。
1、ADTS是什么？
是AAC的一种封装格式（帧级封装），即给每一帧AAC数据加一个头，让它可以“流式传输、随便切、随便接”
2、ADTS帧结构？
```c
[ADTS Header] + [AAC Raw data]
```
Header长度7字节（无CRC），9字节（有CRC）。

## 3.2 AAC一帧大小
一帧1024个采样点，PCM中双声道，8位位深的话，一个采样点2字节，那么一帧数据的大小是1024×2个字节，这是错误的。AAC是压缩后的数据的，其帧大小的计算不该如此。

正确计算公式：帧大小（字节）= (码率 / 采样点) × 1024 / 8
解释：一帧就是1秒的数据，码率是1秒钟的比特数，/采样点计算出来一个采样点的数据大小(单位bit)，×1024计算一帧的数据大小(单位bit)，/8将数据大小转化为字节(单位byte)。
举例：128kbps，44100Hz. AAC一帧数据大小：128000/44100*1024/8~=372字节

64 kbps，44.1 kHz ≈ 185 字节，256 kbps，48 kHz ≈ 682 字节，不会超过1000。RTP_MAX_PKT_SIZE为1400，因此采用单一打包的方式就可以了，一个AAC帧放到一个RTP包里。

## 3.3 制作aac文件
```c
ffmpeg -i csf.mp3 -c:a aac -b:a 128k -ar 44100 -f adts test.aac
```
**-c:a aac**: 音频编码使用aac
**-b:a 128k**: 音频码率使用128k
**-ar 44100**: 采样率44100Hz
**-f adts**: 封装类型adts
