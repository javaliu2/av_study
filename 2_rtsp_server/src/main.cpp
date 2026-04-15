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
    EventScheduler* scheduler = EventScheduler::createNew(EventScheduler::POLLER_SELECT);
    ThreadPool* threadPool = ThreadPool::createNew(1);
    MediaSessionManager* sessMgr = MediaSessionManager::createNew();
    UsageEnvironment* env = UsageEnvironment::createNew(scheduler, threadPool);

    Ipv4Address rtspAddr("127.0.0.1", 8554);
    RtspServer* rtspServer = RtspServer::createNew(env, sessMgr, rtspAddr);

    LOG_INFO("------------ session init start ------------");
    {
        MediaSession* session = MediaSession::createNew("test");
        MediaSource* source = H264FileMediaSource::createNew(env, "../resource/test.h264");
        Sink* sink = H264FileSink::createNew(env, source);
        session->addSink(MediaSession::TrackId0, sink);

        source = AACFileMediaSource::createNew(env, "../resource/test.aac");
        sink = AACFileSink::createNew(env, source);
        session->addSink(MediaSession::TrackId1, sink);

        // session->startMulticast();
        sessMgr->addSession(session);
    }
    LOG_INFO("------------ session init end ------------");

    rtspServer->start();
    env->scheduler()->loop();
    return 0;
}