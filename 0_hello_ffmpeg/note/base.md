# 一、基础知识

## 1、ffprobe -show_packets test.mpg命令输出

```python
[PACKET]
codec_type=video  # 表示该packet属于哪一类流
stream_index=0    # 表示这是第几路流，0表示视频，1表示音频
pts=682200  	  # pts: presentation time stamp，这一帧应该在什么时候显示，单位：time_base = {1, 90000}
pts_time=7.580000 # 换算成秒，pts * time_base = 682200 * (1 / 90000) = 7.58秒
dts=678600        # dts: decoding time stamp，这一帧什么时候送去解码
dts_time=7.540000 # 同pts_time的计算
duration=3600     # 这个packet在时间轴上占多长时间
duration_time=0.040000  # 0.04 = 1/25，这是25帧视频
size=1692         # 这个packet占用多少字节
pos=923660        # 这个packet在文件中的偏移（字节为单位）
flags=___
[/PACKET]
```