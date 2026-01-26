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
```

## 5、AVPacket结构体描述

```c
/**
 * This structure stores compressed data. It is typically exported by demuxers
 * and then passed as input to decoders, or received as output from encoders and
 * then passed to muxers.
 * 该结构体存储了压缩的数据。它通常由解封装器导出，然后传递给解码器。或者来自编码器的输出，然后传递给封装器
 * For video, it should typically contain one compressed frame. For audio it may
 * contain several compressed frames. Encoders are allowed to output empty
 * packets, with no compressed data, containing only side data
 * (e.g. to update some stream parameters at the end of encoding).
 * 对于视频来说，packet里通常包括一个压缩帧。对于音频，它可能包含多个压缩帧。编码器被允许输出空的packet，即不带有压缩数据，只包含辅助数据。（例如，在编码结束的时候更新流参数）
 * The semantics of data ownership depends on the buf field.
 * If it is set, the packet data is dynamically allocated and is
 * valid indefinitely until a call to av_packet_unref() reduces the
 * reference count to 0.
 * 数据的语义拥有者依赖于buf字段。如果它被设置，packet的数据被动态分配，数据无期限有效直至
 * av_packet_unref()被调用，该函数将引用技术置为0
 * If the buf field is not set av_packet_ref() would make a copy instead
 * of increasing the reference count.
 * 如果buf字段没有被设置，av_packet_ref()会制作一个拷贝而不是增加引用计数
 * The side data is always allocated with av_malloc(), copied by
 * av_packet_ref() and freed by av_packet_unref().
 * 辅助数据总是由av_malloc()分配，由av_packet_ref()拷贝，由av_packet_unref()释放。
 * sizeof(AVPacket) being a part of the public ABI is deprecated. once
 * av_init_packet() is removed, new packets will only be able to be allocated
 * with av_packet_alloc(), and new fields may be added to the end of the struct
 * with a minor bump.
 * ABI: Application Binary Interface，二进制层面的“接口规范”，编译器/链接器/CPU用的接口
 * sizeof(AVPacket)作为公共ABI的一部分是弃用的。当av_init_packet()移除的时候，新的packet只能够通过av_packet_alloc()被分配，新的字段可以被添加到结构体的末尾通过增加字段。
 * @see av_packet_alloc
 * @see av_packet_ref
 * @see av_packet_unref
 */
typedef struct AVPacket {
    /**
     * A reference to the reference-counted buffer where the packet data is
     * stored.
     * May be NULL, then the packet data is not reference-counted.
     * 对引用可计数的buffer的引用，buffer中存放packet数据。可以为null，这样的话，packet数据不是引用可计数的。
     */
    AVBufferRef *buf;
    /**
     * Presentation timestamp in AVStream->time_base units; the time at which
     * the decompressed packet will be presented to the user.
     * Can be AV_NOPTS_VALUE if it is not stored in the file.
     * pts MUST be larger or equal to dts as presentation cannot happen before
     * decompression, unless one wants to view hex dumps. Some formats misuse
     * the terms dts and pts/cts to mean something different. Such timestamps
     * must be converted to true pts/dts before they are stored in AVPacket.
     * 
     */
    int64_t pts;
    /**
     * Decompression timestamp in AVStream->time_base units; the time at which
     * the packet is decompressed.
     * Can be AV_NOPTS_VALUE if it is not stored in the file.
     */
    int64_t dts;
    uint8_t *data;
    int   size;
    int   stream_index;
    /**
     * A combination of AV_PKT_FLAG values
     */
    int   flags;
    /**
     * Additional packet data that can be provided by the container.
     * Packet can contain several types of side information.
     */
    AVPacketSideData *side_data;
    int side_data_elems;

    /**
     * Duration of this packet in AVStream->time_base units, 0 if unknown.
     * Equals next_pts - this_pts in presentation order.
     */
    int64_t duration;

    int64_t pos;                            ///< byte position in stream, -1 if unknown

    /**
     * for some private data of the user
     */
    void *opaque;

    /**
     * AVBufferRef for free use by the API user. FFmpeg will never check the
     * contents of the buffer ref. FFmpeg calls av_buffer_unref() on it when
     * the packet is unreferenced. av_packet_copy_props() calls create a new
     * reference with av_buffer_ref() for the target packet's opaque_ref field.
     *
     * This is unrelated to the opaque field, although it serves a similar
     * purpose.
     */
    AVBufferRef *opaque_ref;

    /**
     * Time base of the packet's timestamps.
     * In the future, this field may be set on packets output by encoders or
     * demuxers, but its value will be by default ignored on input to decoders
     * or muxers.
     */
    AVRational time_base;
} AVPacket;
```

