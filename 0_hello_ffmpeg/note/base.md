# 一、基础知识

## 1、使用ffmpeg生成示例文件

```python
ffmpeg -f lavfi -i testsrc=size=1280*720:rate=25 \  # lavfi: LibAVFilter,输入的是滤镜生成的数据；帧率25fps
-t 10 \  # 长度10秒
-c:v mpeg1video \  # mpeg: moving picture experts group,视频编码器mpeg-1
-q:v 2 \  # quantizer(量化参数)，这条命令表示视频编码质量等级设为2（数值越小，质量越高，体积越大）
test.mpg  # 输出文件名＆格式
```

## 2、ffprobe -show_packets test.mpg命令输出

```python
[PACKET]
codec_type=video  # 表示该packet属于哪一类流
stream_index=0    # 表示这是第几路流，0表示视频，1表示音频
pts=682200  	  # pts: presentation time stamp，这一帧应该在什么时候显示，单位：time_base = {1, 90000}
pts_time=7.580000 # 换算成秒，pts * time_base = 682200 * (1 / 90000) = 7.58秒
dts=678600        # dts: decoding time stamp，这一帧什么时候送去解码
dts_time=7.540000 # 同pts_time的计算
duration=3600     # 这个packet在时间轴上占多长时间
duration_time=0.040000  # 0.04 = 1/25，这是25帧视频，0.04*25=1，所以视频帧率是25fps
size=1692         # 这个packet占用多少字节
pos=923660        # 这个packet在文件中的偏移（字节为单位）
flags=___
[/PACKET]
```

## 3、mpg文件

是MPEG(Moving Picture Experts Group) Program Stream，是本地文件，连续字节流，没有固定分包。MPEG-TS(Transport Stream)是在网络上传输的音视频文件，有包的概念（网络传输中的包概念）。

## 4、输出前1秒的所有帧信息

1、命令：`ffprobe -show_frames -select_streams v -read_intervals 0%+1 test.mpg`

2、输出如下。一共25个帧

```python
media_type=video
stream_index=0 # 视频流
key_frame=1  # 该帧是关键帧
pts=48600
pts_time=0.540000 # 显示时间
pkt_dts=48600
pkt_dts_time=0.540000
best_effort_timestamp=48600
best_effort_timestamp_time=0.540000
duration=3600
duration_time=0.040000  # 显示多久
pkt_pos=27
pkt_size=37582
width=1280 # 分辨率-宽度，单位：像素
height=720 # 分辨率-高度，单位：像素
crop_top=0
crop_bottom=0
crop_left=0
crop_right=0
pix_fmt=yuv420p  # 像素格式
sample_aspect_ratio=1:1
pict_type=I # 等价于key_frame=1, Intra frame, 帧内编码
interlaced_frame=0
top_field_first=0
lossless=0
repeat_pict=0
color_range=tv
color_space=unknown
color_primaries=unknown
color_transfer=unknown
chroma_location=center
TAG:timecode=00:00:00:00
[SIDE_DATA]
side_data_type=AVPanScan
[/SIDE_DATA]
[SIDE_DATA]
side_data_type=GOP timecode
timecode=00:00:00:00
[/SIDE_DATA]
[/FRAME]
...
[FRAME]
media_type=video
stream_index=0
key_frame=0  # 非关键帧
pts=131400
pts_time=1.460000
pkt_dts=131400
pkt_dts_time=1.460000
best_effort_timestamp=131400
best_effort_timestamp_time=1.460000
duration=3600
duration_time=0.040000
pkt_pos=122892
pkt_size=1771
width=1280
height=720
crop_top=0
crop_bottom=0
crop_left=0
crop_right=0
pix_fmt=yuv420p
sample_aspect_ratio=1:1
pict_type=P  # 前向帧，predictive-coded picture，前向预测编码图像帧
interlaced_frame=0
top_field_first=0
lossless=0
repeat_pict=0
color_range=tv
color_space=unknown
color_primaries=unknown
color_transfer=unknown
chroma_location=center
TAG:timecode=00:00:00:24
[SIDE_DATA]
side_data_type=AVPanScan
[/SIDE_DATA]
[SIDE_DATA]
side_data_type=GOP timecode
timecode=00:00:00:24
[/SIDE_DATA]
[/FRAME]
[FRAME]
media_type=video
stream_index=0
key_frame=1  # 关键帧
pts=135000
pts_time=1.500000
pkt_dts=N/A
pkt_dts_time=N/A
best_effort_timestamp=135000
best_effort_timestamp_time=1.500000
duration=3600
duration_time=0.040000
pkt_pos=124940
pkt_size=37023
width=1280
height=720
crop_right=0
pix_fmt=yuv420p
sample_aspect_ratio=1:1
pict_type=I
interlaced_frame=0
top_field_first=0
lossless=0
repeat_pict=0
color_range=tv
color_space=unknown
color_primaries=unknown
color_transfer=unknown
chroma_location=center
[SIDE_DATA]
side_data_type=AVPanScan
[/SIDE_DATA]
[/FRAME]