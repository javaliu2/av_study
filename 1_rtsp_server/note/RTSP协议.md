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
1、Network Abstraction Layer Unit, NALU, 即网络抽象层单元。他是h.264码流里的最小网络传输单元。组成：起始码 + NAL header + payload
2、起始码：00 00 01 或者 00 00 00 01
3、NAL header(1字节)构成: forbidden_bit(1), nal_ref_idc(2), nal_unit_type(5).
4、常见nal_unit_type介绍
| 值  |  含义 | 全称 |
| ------------ | ------------ |
| 1  |  非IDR帧 | |
| 5  |  IDR帧 | Instantaneous Decoding Refresh(瞬时解码刷新帧) |
| 6  |  SEI  |  Supplemental Enhancement Infomation(补充增强信息) |
| 7  |  SPS  | Sequence Parameter Set(序列参数集) |
| 8  |  PPS  | Picture  Parameter Set(图像参数集) |
