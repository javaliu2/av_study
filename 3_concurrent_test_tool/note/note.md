# 测试流媒体服务器并发性能的工具
测试的服务器是ZLMediaKit，网址如下https://github.com/ZLMediaKit/ZLMediaKit

使用北小菜提供编译好的可执行程序进行测试，https://gitee.com/Vanishi/zlm

双击start.bat启动服务器，另外启动一个cmd执行如下命令进行推流
ffmpeg -re -i test.mp4 -rtsp_transport tcp -c copy -f rtsp rtsp://127.0.0.1:554/live/test
以上命令的解释：
-re: 按实时速度读取输入
-i test.mp4: 指定输入文件是当前命令执行所在目录下的test.mp4文件
-rtsp_transport tcp: 指定rtsp底层传输方式为tcp
-c copy: 不重新编码，直接拷贝码流
-f rtsp: 指定输出格式为rtsp
rtsp://127.0.0.1:554/live/test: 推送的目的地址

## 1、steady_clock和system_clock
steady_clock是逻辑时间，单调递增，用于计时/超时
system_clock是真实时间，用于日志/时间戳