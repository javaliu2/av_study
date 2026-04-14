#include "MediaSessionManager.h"
#include "MediaSession.h"

MediaSessionManager* MediaSessionManager::createNew() {
    return new MediaSessionManager();
}

MediaSessionManager::MediaSessionManager() {

}

MediaSessionManager::~MediaSessionManager() {
    
}

bool MediaSessionManager::addSession(MediaSession* session) {
    if (mSessMap.find(session->name()) != mSessMap.end()) {
        return false;
    }
    mSessMap.insert(std::make_pair(session->name(), session));
    return true;
}

bool MediaSessionManager::removeSession(MediaSession* session) {
    auto it = mSessMap.find(session->name());
    if (it == mSessMap.end()) {
        return false;
    }
    mSessMap.erase(it);
    return true;
}

MediaSession* MediaSessionManager::getSession(const std::string& name) {
    auto it = mSessMap.find(name);
    if (it == mSessMap.end()) {
        return nullptr;
    }
    return it->second;
}