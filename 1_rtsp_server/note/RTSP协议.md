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
