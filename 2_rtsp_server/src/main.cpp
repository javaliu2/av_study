#include "scheduler/EventScheduler.h"
#include "scheduler/ThreadPool.h"
#include "scheduler/UsageEnvironment.h"
#include "live/MediaSessionManager.h"
#include "live/RtspServer.h"
#include "live/H264FileMediaSource.h"
#include "live/H264FileSink.h"
#include "live/AACFileMediaSource.h"
#include "live/AACFileSink.h"
#include "base/Logger.h"

int main() {
    srand(time(nullptr));
    EventScheduler* scheduler = EventScheduler::createNew(EventScheduler::POLLER_SELECT);  // 创建了全局poller和timerManager
    ThreadPool* threadPool = ThreadPool::createNew(1);
    MediaSessionManager* sessMgr = MediaSessionManager::createNew();
    UsageEnvironment* env = UsageEnvironment::createNew(scheduler, threadPool);

    Ipv4Address rtspAddr("127.0.0.1", 8554);
    RtspServer* rtspServer = RtspServer::createNew(env, sessMgr, rtspAddr);

    LOG_INFO("------------ session init start ------------");
    {
        MediaSession* session = MediaSession::createNew("test");
        MediaSource* source = H264FileMediaSource::createNew(env, "../resource/test.h264");
        Sink* sink = H264FileSink::createNew(env, source);  // 构造函数中构造 消费h264文件定时器事件，然后将事件加入timerManager
        session->addSink(MediaSession::TrackId0, sink);

        source = AACFileMediaSource::createNew(env, "../resource/test.aac");
        sink = AACFileSink::createNew(env, source);  // 构造消费aac文件定时器事件，将事件加入timerManager
        session->addSink(MediaSession::TrackId1, sink);

        // session->startMulticast();
        sessMgr->addSession(session);
    }
    LOG_INFO("------------ session init end ------------");

    rtspServer->start();
    env->scheduler()->loop();  // detach的线程处理定时器事件。本线程，即main线程处理io事件和trigger事件
    return 0;
}