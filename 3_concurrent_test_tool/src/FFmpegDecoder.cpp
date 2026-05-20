#include "FFmpegDecoder.h"
#include "Common.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}
#include <thread>

#pragma warning(disable: 4996)

FFmpegDecoder::FFmpegDecoder() : mVideoCodecCtx(nullptr) {
    /**
     * Allocate an AVFormatContext.
     * avformat_free_context() can be used to free the context and everything
     * allocated by the framework within it.
     * 分配一个AVFormatContext，avformat_free_context()用于释放context以及由框架分配的与context有关的所有东西。
     */
    mFmtCtx = avformat_alloc_context();
}

FFmpegDecoder::~FFmpegDecoder() {
    if (mVideoCodecCtx) {
        // Free the codec context and everything associated with it and write NULL to the provided pointer.
        // 释放codec上下文，以及与上下文有关的任何东西，并且将指针参数置为null
        avcodec_free_context(&mVideoCodecCtx);
        mVideoCodecCtx = nullptr;
    }
    if (mFmtCtx) {
        // Close an opened input AVFormatContext. Free it and all its contents and set *s to NULL.
        // 关闭一个打开的AVFormatContext，释放该context以及它的内容，并且设置*s为null
        avformat_close_input(&mFmtCtx);
        avformat_free_context(mFmtCtx);  // Free an AVFormatContext and all its streams.释放context以及它所有的流。
        mFmtCtx = nullptr;
    }
}

void FFmpegDecoder::testPullAndDecodeStream(int thread_num, const char* input, const char* rtsp_transport, const char* decoder) {
    logw("thread-" + std::to_string(thread_num) + "], testPullDecodeStream start");

    AVDictionary* format_dict = nullptr;
    // 如果format_dict为空，那么av_dict_set方法会创建该对象，这和面向对象中创建对象的方式不太一致
    av_dict_set(&format_dict, "rtsp_transport", rtsp_transport, 0);  // 设置rtsp底层网络协议 tcp or udp
    av_dict_set(&format_dict, "stimeout", "3000000", 0);  // 设置rtsp连接超时（单位us, 10^6 us = 1s）
    av_dict_set(&format_dict, "rw_timeout", "3000000", 0);  // 设置rtmp/http-flv连接超时
    /**
     * Open an input stream and read the header. The codecs are not opened.
     * The stream must be closed with avformat_close_input().
     * 打开一个输入流并且读取头部。编解码器没有打开，打开的流必须使用avformat_close_input()进行关闭。
     * 
     * @param ps       
     * Pointer to user-supplied AVFormatContext (allocated by avformat_alloc_context). 
     * 指向用户提供的AVFormatContext对象地址的指针变量，AVFormatContext对象由avformat_alloc_context()分配。
     * 
     * May be a pointer to NULL, in which case an AVFormatContext is allocated by this function and written into ps.
     * 可以是指向null的指针，在这种情况下，AVFormatContext对象由该函数分配，并且将对象地址写入ps。
     * 
     * Note that a user-supplied AVFormatContext will be freed on failure and its pointer set to NULL.
     * 注意在失败的情况下，用户提供的AVFormatContext对象会被释放，然后置ps值为null。
     * 
     * @param url      URL of the stream to open. 要打开流的URL
     * 
     * @param fmt      If non-NULL, this parameter forces a specific input format.
     *                 Otherwise the format is autodetected.
     * 如果非空，该参数强迫一个指定的输入格式，否则格式是自动检测的。
     * 
     * @param options  
     * A dictionary filled with AVFormatContext and demuxer-private options.
     * 填充了AVFormatContext和解复用器私用选项的字典。
     * On return this parameter will be destroyed and replaced with a dict containing options that were not found. May be NULL.
     * 函数返回的时候，该参数会被销毁，取而代之的是包含没有找到的选项的一个字典。该参数可以为null。
     * @return 0 on success; on failure: frees ps, sets its pointer to NULL,
     *         and returns a negative AVERROR.
     * 成功时返回0；失败时: 释放ps，设置其值为null并且返回一个负的AVERROR编码。
     * @note If you want to use custom IO, preallocate the format context and set its pb field.
     * 如果你想使用定制化IO，预分配format context，并且设置它的pb选项。
     */
    if (avformat_open_input(&mFmtCtx, input, nullptr, &format_dict) != 0) {
        logw("avformat_open_input error:" + std::string(input));
        return;
    }
    /**
     * Read packets of a media file to get stream information. 
     * 读取媒体文件的包以获取流信息。
     * This is useful for file formats with no headers such as MPEG. 
     * 该函数对于像MPEG等没有头部的文件格式是有用的。
     * This function also computes the real framerate in case of MPEG-2 repeat frame mode.
     * 针对MPEG-2重复帧模式，该函数也可以计算实际帧率。
     * The logical file position is not changed by this function; examined packets may be buffered for later processing.
     * 该函数不会改变逻辑文件位置，为了后续的处理，检测的包可能会被缓存。
     * @param ic media file handle 媒体文件句柄
     * @param options  If non-NULL, an ic.nb_streams long array of pointers to
     *                 dictionaries, where i-th member contains options for
     *                 codec corresponding to i-th stream.
     * 如果非空，一个ic.nb_streams长度的指针数组作为字典，其中第i个成员包含第i个流的编解码器选项。
     *                 On return each dictionary will be filled with options that were not found.
     * 该函数返回的时候，每一个字典包含没有找到的选项。
     * @return >=0 if OK, AVERROR_xxx on error
     * 
     * @note this function isn't guaranteed to open all the codecs, so
     *       options being non-empty at return is a perfectly normal behavior.
     * 该函数不保证打开所有的编解码器，因此函数返回时，形参options为非空是一个正常行为。
     * @todo Let the user decide somehow what information is needed so that
     *       we do not waste time getting stuff the user does not need.
     */
    if (avformat_find_stream_info(mFmtCtx, nullptr) < 0) {
        logw("avformat_find_stream_info error:" + std::string(input));
        return;
    }

    // video start
    /**
     * Find the "best" stream in the file.
     * 查找文件中的最佳的流。
     * The best stream is determined according to various heuristics as the most
     * likely to be what the user expects.
     * 最佳的流媒体内容是根据多种算法来去确定的，其目的是确保最有可能符合用户的预期。
     * If the decoder parameter is non-NULL, av_find_best_stream will find the
     * default decoder for the stream's codec; streams for which no decoder can
     * be found are ignored.
     * 如果解码器参数非空，该函数会针对流的编解码器，查找默认解码器，针对找不到解码器的流，这些流会被忽略。
     * @param ic                media file handle
     * @param type              stream type: video, audio, subtitles, etc.
     * @param wanted_stream_nb  user-requested stream number,
     *                          or -1 for automatic selection
     * @param related_stream    try to find a stream related (eg. in the same
     *                          program) to this one, or -1 if none
     * @param decoder_ret       if non-NULL, returns the decoder for the
     *                          selected stream
     * @param flags             flags; none are currently defined
     *
     * @return  the non-negative stream number in case of success,
     * 成功的话，返回非负数，表示流的编号
     *          AVERROR_STREAM_NOT_FOUND if no stream with the requested type
     *          could be found,
     * 如果请求类型的流未能找到，返回AVERROR_STREAM_NOT_FOUND
     *          AVERROR_DECODER_NOT_FOUND if streams were found but no decoder
     * 如果流可以找到，但是没有解码器，返回AVERROR_DECODER_NOT_FOUND
     * @note  If av_find_best_stream returns successfully and decoder_ret is not
     *        NULL, then *decoder_ret is guaranteed to be set to a valid AVCodec.
     * 如果该函数返回成功，并且参数decoder_ret非空，那么*decoder_ret保证被设置为一个有效的AVCodec。
     */
    int srcVideoIndex = av_find_best_stream(mFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    int curVideoWidth = -1;
    int curVideoHeight = -1;
    int curVideoFps = -1;
    int curVideoChannels = 3;
    if (srcVideoIndex > -1) {

    } else {
        logw("av_find_best_stream error:" + std::string(input));
        return;
    }
    // video end
    AVBufferRef* device_ref = nullptr;
    /**
     * Open a device of the specified type and create an AVHWDeviceContext for it.
     * 打开指定类型的设备，并且针对它创建一个AVHWDeviceContext对象。
     * This is a convenience function intended to cover the simple cases. 
     * 这是一个方便方法为了覆盖最简单的情况。
     * Callers who need to fine-tune device creation/management should open the device
     * manually and then wrap it in an AVHWDeviceContext using
     * av_hwdevice_ctx_alloc()/av_hwdevice_ctx_init().
     * 需要微调设备创建/管理的调用者应该手动打开设备，然后使用av_hwdevice_ctx_alloc或av_hwdevice_ctx_init方法
     * 包装该设备在AVHWDeviceContext中。
     * The returned context is already initialized and ready for use, the caller
     * should not call av_hwdevice_ctx_init() on it. 
     * 返回的context已经被初始化，并且可用，调用者不应该在context上调用av_hwdevice_ctx_init()
     * The user_opaque/free fields of the created AVHWDeviceContext are set by this function 
     * and should not be touched by the caller.
     * 创建AVHWDeviceContext对象的user_opaque/free字段由该函数设置，并且不应该被调用者接触。
     * @param device_ctx On success, a reference to the newly-created device context
     *                   will be written here. 
     * 成功的话，新建的设备context对象引用被写入该变量。
     * The reference is owned by the caller and must be released with av_buffer_unref() when no longer needed.
     * 上面的引用被调用者持有，在不再使用的情况下，必须使用av_buffer_unref()释放
     * On failure, NULL will be written to this pointer.
     * 失败的话，null被写入该指针变量。
     * @param type The type of the device to create. 要创建的设备的类型。
     * @param device A type-specific string identifying the device to open.
     * 
     * @param opts A dictionary of additional (type-specific) options to use in
     *             opening the device. The dictionary remains owned by the caller.
     * @param flags currently unused
     *
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int ret = av_hwdevice_ctx_create(&device_ref, AV_HWDEVICE_TYPE_CUDA, "auto", nullptr, 0);
    if (ret < 0) {
        logw("av_hwdevice_ctx_create error:" + std::string(input));
        return;
    }
    AVCodecParameters* videoCodecPar = mFmtCtx->streams[srcVideoIndex]->codecpar;
    const AVCodec* videoCodec = nullptr;
    videoCodec = avcodec_find_decoder_by_name(decoder);
    if (videoCodecPar->codec_id == AV_CODEC_ID_H264) {
        // decoders: h264 h264_qsv h264_cuvid
    } else if (videoCodecPar->codec_id == AV_CODEC_ID_HEVC) {
        // decoders: hevc hevc_qsv hevc_cuvid
    }

    if (videoCodec) {
        logw("hardware decode h264 success:" + std::string(decoder));
    } else {
        logw("hardware decode h264 error:" + std::string(decoder));
        return;
    }

    if (!videoCodec) {
        videoCodec = avcodec_find_decoder(videoCodecPar->codec_id);
        if (!videoCodec) {
            logw("hardware cpu decode h264 error");
            return;
        }
    }

    /**
     * Allocate an AVCodecContext and set its fields to default values. 
     * 分配一个AVCodecContext，然后设置它的字段为默认值。
     * The resulting struct should be freed with avcodec_free_context().
     * 结果结构体对象需要使用avcodec_free_context()进行释放。
     * @param codec if non-NULL, allocate private data and initialize defaults
     *              for the given codec. 
     * 如果非空，分配私有数据以及初始化他的默认值。
     * It is illegal to then call avcodec_open2() with a different codec.
     * 在该方法之后使用不同的编解码器调用avcodec_open2()是非法的。
     *              If NULL, then the codec-specific defaults won't be initialized,
     *              which may result in suboptimal default settings (this is
     *              important mainly for encoders, e.g. libx264).
     * 如果为空，那么编解码器指定的默认值不会被初始化，这可能导致次优默认设置，对于像libx264等编码器来说，这是很重要的。
     *
     * @return An AVCodecContext filled with default values or NULL on failure.
     */
    mVideoCodecCtx = avcodec_alloc_context3(videoCodec);
    if (avcodec_parameters_to_context(mVideoCodecCtx, videoCodecPar) != 0) {
        logw("avcodec_parameters_to_context error:" + std::string(input));
        return;
    }

    if (avcodec_open2(mVideoCodecCtx, videoCodec, nullptr) < 0) {
        logw("avcodec_open2 error:" + std::string(input));
        return;
    }

    curVideoWidth = mVideoCodecCtx->width;
    curVideoHeight = mVideoCodecCtx->height;
    mVideoStream = mFmtCtx->streams[srcVideoIndex];

    if (0 == mVideoStream->avg_frame_rate.den) {
        char log[300];
        sprintf(log, "url=%s,index=%d,avg_frame_rate(num=%d,den=%d),r_frame_rate(num=%d,den=%d)",
        input, srcVideoIndex, mVideoStream->avg_frame_rate.num, mVideoStream->avg_frame_rate.den,
        mVideoStream->r_frame_rate.num, mVideoStream->r_frame_rate.den);
        logw(log);
        curVideoFps = 25;
    } else {
        curVideoFps = mVideoStream->avg_frame_rate.num / mVideoStream->avg_frame_rate.den;
    }

    char log[300];
    sprintf(log, "url=%s,videoIndex=%d,videoWidth=%d,videoHeight=%d,videoFps=%d",
        input, srcVideoIndex, curVideoWidth, curVideoHeight, curVideoFps);
    logw(log);

    // 开始解码
    mSrcVideoWidth = curVideoWidth;
    mSrcVideoHeight = curVideoHeight;
    int srcVideoChannels = curVideoChannels;
    int srcVideoFps = curVideoFps;

    AVPacket pkt;  // 未解码的视频帧
    // Allocate an AVFrame and set its fields to default values. The resulting struct must be freed using av_frame_free().
    // 分配一个AVFrame结构体对象并且设置他的字段为默认值，需要使用av_frame_free()释放该结构体对象
    AVFrame* frame_yuv420p = av_frame_alloc();
    AVFrame* frame_bgr = av_frame_alloc();
    
    // Return the size in bytes of the amount of data required to store an image with the given parameters.
    // 返回存储给定参数的图片数据的大小，以字节为单位
    int frame_bgr_buff_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, mSrcVideoWidth, mSrcVideoHeight, 1);
    // Allocate a memory block with alignment suitable for all memory accesses (including vectors if available on the CPU).
    // 对于所有的内存访问，分配合适对齐的内存块。（如果在CPU上可用的话，包括向量）
    uint8_t* frame_bgr_buff = (uint8_t*)av_malloc(frame_bgr_buff_size);
    av_image_fill_arrays(frame_bgr->data, frame_bgr->linesize, frame_bgr_buff, AV_PIX_FMT_BGR24, mSrcVideoWidth, mSrcVideoHeight, 1);
    
    // Allocate and return an SwsContext. You need it to perform scaling/conversion operations using sws_scale().
    // 分配然后返回一个SwsContext，使用sws_scale()执行缩放和转换操作。
    SwsContext* sws_ctx_yuv420p2bgr = sws_getContext(
        mSrcVideoWidth, mSrcVideoHeight,
        mVideoCodecCtx->pix_fmt,
        mSrcVideoWidth, mSrcVideoHeight, AV_PIX_FMT_BGR24,
        SWS_BICUBIC, nullptr, nullptr, nullptr);
    
        int64_t frameCount = 0;
        while (true) {
            if (av_read_frame(mFmtCtx, &pkt) >= 0) {
                if (pkt.stream_index == srcVideoIndex) {
                    ret = avcodec_send_packet(mVideoCodecCtx, &pkt);
                    if (ret == 0) {
                        ret = avcodec_receive_frame(mVideoCodecCtx, frame_yuv420p);
                        if (ret == 0) {
                            frameCount++;
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        } else {
                            logw("avcodec_receive_frame error: ret=" + std::to_string(ret));
                        }
                    } else {
                        char log[200];
                        sprintf(log, "avcodec_send_packet error: ret=%d", ret);
                        logw(log);
                    }
                }
                // 获取的pkt，必须释放
                av_packet_unref(&pkt);
            } else {
                break;
            }
        }
        av_frame_free(&frame_yuv420p);
        frame_yuv420p = nullptr;
        av_frame_free(&frame_bgr);
        frame_bgr = nullptr;

        av_free(frame_bgr_buff);
        frame_bgr_buff = nullptr;

        sws_freeContext(sws_ctx_yuv420p2bgr);
        sws_ctx_yuv420p2bgr = nullptr;

        logw("[thread-" + std::to_string(thread_num) + "],testPullDecodeStream end");
}

void FFmpegDecoder::testPullStream(int thread_num, const char* input, const char* rtsp_transport) {

}

