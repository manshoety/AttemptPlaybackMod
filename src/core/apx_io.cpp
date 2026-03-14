// apx_io.cpp
#include <ostream>
#include <istream>
#include <cstring>
#include <cstdint>
#include <vector>
#include "types.hpp"
#include "apx_format.hpp"
#include "apx_io.hpp"

bool writeAPXPracticePath(std::ostream& out, const PracticePath& path) {
    if (!out.good()) return false;

    APXPathHeaderV2 hdr{};
    hdr.version = 4;  // Version 4 with saved session stuff
    hdr.numSessions = static_cast<uint32_t>(path.sessions.size());
    hdr.activeSessionId = path.activeSessionId;
    hdr.selectedSessionId = path.selectedSessionId;
    hdr.frozen = path.frozen ? 1 : 0;

    const uint32_t type = kChunk_PATH;

    // Calculate total size
    uint32_t totalSize = sizeof(APXPathHeaderV2);
    for (const auto& session : path.sessions) {
        totalSize += sizeof(APXSessionHeader);
        totalSize += static_cast<uint32_t>(session.checkpoints.size() * sizeof(APXCheckpoint));
        totalSize += static_cast<uint32_t>(session.activeChain.size() * sizeof(int32_t));
        totalSize += static_cast<uint32_t>(session.allAttemptSerials.size() * sizeof(int32_t));
        totalSize += static_cast<uint32_t>(session.segments.size() * sizeof(APXSegment));
    }

    out.write(reinterpret_cast<const char*>(&type), sizeof(type));
    out.write(reinterpret_cast<const char*>(&totalSize), sizeof(totalSize));
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!out.good()) return false;

    for (const auto& session : path.sessions) {
        APXSessionHeader shdr{};
        shdr.sessionId = session.sessionId;
        shdr.numCheckpoints = static_cast<uint32_t>(session.checkpoints.size());
        shdr.numActiveIDs = static_cast<uint32_t>(session.activeChain.size());
        shdr.numAttemptSerials = static_cast<uint32_t>(session.allAttemptSerials.size());
        shdr.numSegments = static_cast<uint32_t>(session.segments.size());
        shdr.startX = session.startX;
        shdr.startY = session.startY;
        shdr.endX = session.endX;
        shdr.completed = session.completed ? 1 : 0;
        shdr.frozen = session.frozen ? 1 : 0;

        out.write(reinterpret_cast<const char*>(&shdr), sizeof(shdr));

        // Write checkpoints with time data
        for (const auto& checkpoint : session.checkpoints) {
            APXCheckpoint acheckpoint{};
            acheckpoint.id = checkpoint.id;
            acheckpoint.x = checkpoint.x;
            acheckpoint.y = checkpoint.y;
            acheckpoint.rot1 = checkpoint.rot1;
            acheckpoint.rot2 = checkpoint.rot2;
            acheckpoint.rot1Valid = checkpoint.rot1Valid ? 1 : 0;
            acheckpoint.rot2Valid = checkpoint.rot2Valid ? 1 : 0;
            acheckpoint.checkpointTime = checkpoint.checkpointTime;
            out.write(reinterpret_cast<const char*>(&acheckpoint), sizeof(acheckpoint));
        }

        // Write active chain
        for (int32_t id : session.activeChain) {
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        }

        // Write all attempt serials
        for (int32_t id : session.allAttemptSerials) {
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        }

        // Write segments with time data
        for (const auto& seg : session.segments) {
            APXSegment aseg{};
            aseg.startX = seg.startX;
            aseg.endX = seg.endX;
            aseg.ownerSerial = seg.ownerSerial;
            aseg.checkpointIdEnd = seg.checkpointIdEnd;
            aseg.p1Frames = (int32_t)seg.p1Frames;
            aseg.p2Frames = (int32_t)seg.p2Frames;
            aseg.maxXReached = seg.maxXReached;

            aseg.baseTimeOffset = seg.baseTimeOffset;
            aseg.localStartTime = seg.localStartTime;
            aseg.localEndTime   = seg.localEndTime;

            out.write(reinterpret_cast<const char*>(&aseg), sizeof(aseg));
        }

        if (!out.good()) return false;
    }

    return true;
}

bool readAPXPracticePath(std::istream& in, uint32_t chunkSize, PracticePath& path) {
    path.clear();
    
    std::streampos startPos = in.tellg();
    
    // Read header to determine version
    APXPathHeaderV2 hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;
    
    path.activeSessionId = hdr.activeSessionId;
    path.selectedSessionId = hdr.selectedSessionId;
    path.frozen = (hdr.frozen != 0);
    
    const bool isVersion4 = (hdr.version >= 4);
    
    //log::info("[APX load] PATH version={}, sessions={}", hdr.version, hdr.numSessions);
    
    // Read sessions
    for (uint32_t s = 0; s < hdr.numSessions; ++s) {
        PracticeSession session;
        
        if (isVersion4) {
            APXSessionHeader shdr{};
            in.read(reinterpret_cast<char*>(&shdr), sizeof(shdr));
            if (!in) return false;
            
            session.sessionId = shdr.sessionId;
            session.startX = shdr.startX;
            session.startY = shdr.startY;
            session.endX = shdr.endX;
            session.completed = (shdr.completed != 0);
            session.frozen = (shdr.frozen != 0);
            
            // Read checkpoints
            for (uint32_t i = 0; i < shdr.numCheckpoints; ++i) {
                APXCheckpoint acheckpoint{};
                in.read(reinterpret_cast<char*>(&acheckpoint), sizeof(acheckpoint));
                if (!in) return false;
                
                Checkpoint checkpoint;
                checkpoint.id = acheckpoint.id;
                checkpoint.x = acheckpoint.x;
                checkpoint.y = acheckpoint.y;
                checkpoint.rot1 = acheckpoint.rot1;
                checkpoint.rot2 = acheckpoint.rot2;
                checkpoint.rot1Valid = (acheckpoint.rot1Valid != 0);
                checkpoint.rot2Valid = (acheckpoint.rot2Valid != 0);
                checkpoint.checkpointTime = acheckpoint.checkpointTime;
                session.checkpoints.push_back(checkpoint);
            }
            
            // Read active chain
            for (uint32_t i = 0; i < shdr.numActiveIDs; ++i) {
                int32_t id;
                in.read(reinterpret_cast<char*>(&id), sizeof(id));
                if (!in) return false;
                session.activeChain.push_back(id);
            }

            // Read all attempt serials
            for (uint32_t i = 0; i < shdr.numAttemptSerials; ++i) {
                int32_t id;
                in.read(reinterpret_cast<char*>(&id), sizeof(id));
                if (!in) return false;
                session.allAttemptSerials.push_back(id);
            }
            
            // Read segments
            for (uint32_t i = 0; i < shdr.numSegments; ++i) {
                APXSegment aseg{};
                in.read(reinterpret_cast<char*>(&aseg), sizeof(aseg));
                if (!in) return false;

                PracticeSegment seg;
                seg.startX = aseg.startX;
                seg.endX = aseg.endX;
                seg.ownerSerial = aseg.ownerSerial;
                seg.checkpointIdEnd = aseg.checkpointIdEnd;
                seg.p1Frames = aseg.p1Frames;
                seg.p2Frames = aseg.p2Frames;
                seg.maxXReached = aseg.maxXReached;

                seg.baseTimeOffset = aseg.baseTimeOffset;
                seg.localStartTime = aseg.localStartTime;
                seg.localEndTime   = aseg.localEndTime;

                session.segments.push_back(seg);
            }
        } else {
            geode::log::warn("Outdated Practice Session Format. Please delete attempts save file");
        }
        
        session.updateSpan();
        path.sessions.push_back(session);
    }
    
    // Skip any remaining bytes in chunk
    auto endPos = in.tellg();
    if (endPos == std::streampos(-1)) return false;

    std::streamoff bytesConsumed = endPos - startPos;
    if (bytesConsumed < 0) return false;

    if ((uint32_t)bytesConsumed < chunkSize) {
        in.seekg(static_cast<std::streamoff>(chunkSize) - bytesConsumed, std::ios::cur);
    }
    
    //log::info("[APX load] PATH loaded: {} sessions, version {}", path.sessions.size(), hdr.version);
    return true;
}
