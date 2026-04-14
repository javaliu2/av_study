#include <iostream>

using namespace std;

enum TrackId {
        TrackIdNone = -1,
        TrackId0 = 0,
        TrackId1 = 1
    };
const char* trackIdToString(TrackId id) {
    switch (id) {
        case TrackIdNone: return "TrackIdNone";
        case TrackId0: return "TrackId0";
        case TrackId1: return "TrackId1";
        default: return "Unknown";
    }
}
int main() {
    for (int i = -2; i < 4; ++i) {
        TrackId item = (TrackId)i;
        cout << trackIdToString(item) << endl;
    }
    return 0;
}