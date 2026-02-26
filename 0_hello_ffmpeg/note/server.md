# 1、流媒体协议

## 1、RTP & RTCP & RTSP

1、RTP(Real-time Transport Protocol)，实时传输协议，负责视频数据传输

2、RTCP(Real-time Transport Control Protocol)，实时传输控制协议，负责视频质量

3、RTSP(Real-time Transport Streaming Protocol)，实时传输流协议，负责视频控制

## 2、RTMP & HLS

1、RTMP(Real Time Messaging Protocol)，实时消息传输协议，基于TCP，是一个协议族，包括RTMPT/RTMPS/RTMPE等多个协议。

2、HLS(HTTP Live Streaming)是由苹果公司提出的基于HTTP的流媒体网络传输协议。

# 2、nginx-rtmp部署笔记

1、下载rtmp module，在/home/xs/Downloads目录下执行

```shell
git clone https://github.com/arut/nginx-rtmp-module.git
```

2、下载nginx源码，并且解压缩

```shell
wget http://nginx.org/download/nginx-1.24.0.tar.gz
tar -xvf nginx-1.24.0.tar.gz
cd nginx-1.24.0
```

3、执行configure配置脚本

```shell
./configure \
--prefix=/usr/share/nginx \
--sbin-path=/usr/sbin/nginx \  # s是superuser或者system的意思
--conf-path=/etc/nginx/nginx.conf \
--modules-path=/usr/lib/nginx/modules \
--with-http_ssl_module \
--with-http_stub_status_module \
--with-stream \
--with-debug \
--add-module=/home/xs/Downloads/nginx-rtmp-module  # nginx-rtmp-module所在目录
```

4、执行make命令

make的作用：按照Makefile里的规则，把nginx源码编译成可执行程序和库文件

```shell
make -j$(nproc)  # -j:并行编译；$(nproc)变量存储本机CPU的核数
sudo make install
```

5、修改nginx.conf文件配置rtmp服务

```c
rtmp {
	server {
		listen 1935;
		chunk_size 4096;
		
		# live on，直播
		application rtmp_live {
			live on;
		}
		# play videos，点播
		application rtmp_play {
			play /home/xs/Downloads/videos;
		}
	}
}
```

# 3、推拉流验证

1、生成样本视频

```shell
ffmpeg -f lavfi -i testsrc=size=1280x720:rate=30 \
       -f lavfi -i sine=frequency=440 \
       -t 10 \
       -c:v libx264 -preset veryfast -pix_fmt yuv420p \
       -c:a aac \
       test.mp4
```

2、使用ffmpeg推流

```shell
ffmpeg -re -stream_loop -1 -i test.mp4 \
       -c copy \
       -f flv rtmp://127.0.0.1:1935/rtmp_live/test
```

3、使用ffplay拉流

```shell
ffplay -fflags nobuffer -flags low_delay \
       rtmp://127.0.0.1:1935/rtmp_live/test
```

# 4、点播验证

在`/home/xs/Downloads/videos`目录下有视频文件test.mp4，在terminal执行`ffplay rtmp://127.0.0.1:1935/rtmp_play/test.mp4`播放test.mp4视频文件

# 5、other tips

1、linux中stdin的文件描述符是0，stdout的fd是1，stderr的fd是2

```shell
(base) xs@xslab:~$ nginx -V > out.txt 2> err.txt  # 将nginx -V输出内容，写到文件out.txt和err.txt中
(base) xs@xslab:~$ cat err.txt  # 不为空
nginx version: nginx/1.24.0
built by gcc 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04.2) 
built with OpenSSL 3.0.2 15 Mar 2022
TLS SNI support enabled
configure arguments: --prefix=/usr/share/nginx --sbin-path=/usr/sbin/nginx --conf-path=/etc/nginx/nginx.conf --modules-path=/usr/lib/nginx/modules --with-http_ssl_module --with-http_stub_status_module --with-stream --with-debug --add-module=/home/xs/Downloads/nginx-rtmp-module
(base) xs@xslab:~$ cat out.txt  # 为空，证明nginx -V的输出通道是stderr
(base) xs@xslab:~$ nginx -V 2>&1 | grep rtmp  # 2>&1:将stderr的内容重定向到stdout, |只筛选stdout,因此需要重定向
configure arguments: --prefix=/usr/share/nginx --sbin-path=/usr/sbin/nginx --conf-path=/etc/nginx/nginx.conf --modules-path=/usr/lib/nginx/modules --with-http_ssl_module --with-http_stub_status_module --with-stream --with-debug --add-module=/home/xs/Downloads/nginx-rtmp-module
```