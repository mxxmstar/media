#include "rtp.h"
#include <stdexcept>
#include <algorithm>
std::vector<uint8_t> RTCPSdes::Pack() const {
    std::vector<uint8_t> buf;
    uint8_t chunks_size = static_cast<uint8_t>(chunks_.size());
    //头部, v=2, p=0, rc
    buf.push_back((2 << 6) | (0 << 5) | (chunks_size & 0x1F));  
    buf.push_back(RTCP_PACKET_TYPE_SDES);
    buf.push_back(0);buf.push_back(0);  //占位
    
    for(auto& chunk : chunks_) {
        writeU32(buf, chunk.ssrc);
        for(auto& item : chunk.items) {
            buf.push_back(static_cast<uint8_t>(item.type));
            buf.push_back(static_cast<uint8_t>(item.data.size()));
            buf.insert(buf.end(), item.data.begin(), item.data.end());
        }
        buf.push_back(0);  //结束,插入一个type为0的条目
        while(buf.size() % 4 != 0) {
            buf.push_back(0);
        }
    }

    //更新长度字段
    uint16_t length_words = (buf.size() / 4) - 1; //减去头部
    buf[2] = (length_words >> 8) & 0xFF;
    buf[3] = length_words & 0xFF;
    return buf;
}

void RTCPSdes::Parse(const uint8_t* data, size_t size) {
    if(size < 4) {
        throw std::runtime_error("Invalid SDES packet size");
    }
    chunks_.clear();

    uint8_t v = data[0] >> 6;
    if(v != 2) {
        throw std::runtime_error("Invalid SDES packet version");
    }
    uint8_t rece_cnt = data[0] & 0x1F;
    if(rece_cnt == 0) {
        throw std::runtime_error("Invalid SDES packet receiver count");
    }

    if(data[1] != RTCP_PACKET_TYPE_SDES) {
        throw std::runtime_error("Invalid SDES packet type");
    }

    // 解析SDES项
    const uint8_t* p = data + 4;
    for(int i = 0; i < rece_cnt; i++) {
        if(p + 4 > data + size) {
            throw std::runtime_error("Invalid SDES packet Truncated");
        }
        uint32_t ssrc = readU32(p);
        p += 4;

        RTCP_SdesChunk chunk{ssrc, {}};
        while(true) {
            if(p > data + size) {
                throw std::runtime_error("Invalid SDES packet Truncated");
            }
            uint8_t type = *p++;
            if(type == 0) {
                //END
                while((p - data) % 4 != 0) {
                    ++p;
                }
                break;
            }

            if(p >= data + size) {
                throw std::runtime_error("Invalid SDES packet Truncated");
            }
            uint8_t len = *p++;
            if(p + len > data + size) {
                throw std::runtime_error("Invalid SDES packet Truncated");
            }
            chunk.items.push_back({static_cast<RTCP_SdesItemType>(type), std::vector<uint8_t>(p, p + len)});
            p += len;
        }
        //存到chunks_中
        chunks_.push_back(chunk);
    }
}
void RTCPSdes::AddItem(uint32_t ssrc, RTCP_SdesItemType type, const std::string& value){
    auto& chunk = findOrCreateChunk(ssrc);
    // 查找是否已存在相同类型的项
    auto it = std::find_if(chunk.items.begin(), chunk.items.end(), [&](const RTCP_SdesItem& item){
        return item.type == type;
    });
    // 如果已存在，更新数据
    if(it != chunk.items.end()) {
        it->data.assign(value.begin(), value.end());
    } else {
        chunk.items.push_back({type, std::vector<uint8_t>(value.begin(), value.end())});
    }
}

void RTCPSdes::RemoveItem(uint32_t ssrc, RTCP_SdesItemType type) {
    RTCP_SdesChunk* chunk = findChunk(ssrc);
    if(!chunk) {
        return;
    }
    // 查找并删除指定类型的项
    chunk->items.erase(std::remove_if(
        chunk->items.begin(), 
        chunk->items.end(), 
        [&](const RTCP_SdesItem& item){
            return item.type == type;
        }),
        chunk->items.end()
    );
}
void RTCPSdes::writeU32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back(val & 0xFF);
}

uint32_t RTCPSdes::readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];   
}

RTCP_SdesChunk& RTCPSdes::findOrCreateChunk(uint32_t ssrc) {
    auto* chunk = findChunk(ssrc);
    if(!chunk) {
        chunks_.push_back({ssrc, {}});
        return chunks_.back();
    }
    return *chunk;
}
RTCP_SdesChunk* RTCPSdes::findChunk(uint32_t ssrc) {
    for (auto& c : chunks_) { 
        if (c.ssrc == ssrc) {
            return &c; 
        } 
        return nullptr;
    }
}
const RTCP_SdesChunk* RTCPSdes::findChunk(uint32_t ssrc) const {
    for (auto& c : chunks_) { 
        if (c.ssrc == ssrc) {
            return &c; 
        } 
        return nullptr;
    }
}

