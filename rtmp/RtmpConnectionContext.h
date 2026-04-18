#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "RtmpChunkParser.h"
#include "RtmpHandshake.h"
#include "RtmpTypes.h"

namespace rmuduo::rtmp {

class RtmpConnection;
class RtmpSession;

class RtmpConnectionContext {
 public:
  RtmpConnectionContext() = default;

  // 握手、chunk size、带宽窗口、流名等都属于“连接级状态”，
  // 应跟着 TcpConnection 一起存活，而不是挂在全局 server 上。
  HandshakeState handshakeState() const { return handshake_.state(); }

  ConnectionRole role() const { return role_; }
  void setRole(ConnectionRole role) { role_ = role; }

  uint32_t inChunkSize() const { return inChunkSize_; }
  void setInChunkSize(uint32_t size) { inChunkSize_ = size; }

  uint32_t outChunkSize() const { return outChunkSize_; }
  void setOutChunkSize(uint32_t size) { outChunkSize_ = size; }

  uint32_t acknowledgementWindow() const { return acknowledgementWindow_; }
  void setAcknowledgementWindow(uint32_t size) {
    acknowledgementWindow_ = size;
  }

  uint32_t peerBandwidth() const { return peerBandwidth_; }
  void setPeerBandwidth(uint32_t size) { peerBandwidth_ = size; }

  void addReceivedBytes(size_t bytes) { receivedBytes_ += bytes; }
  size_t receivedBytes() const { return receivedBytes_; }

  const std::string& app() const { return app_; }
  void setApp(std::string app) { app_ = std::move(app); }

  const std::string& tcUrl() const { return tcUrl_; }
  void setTcUrl(std::string tc_url) { tcUrl_ = std::move(tc_url); }

  double objectEncoding() const { return objectEncoding_; }
  void setObjectEncoding(double object_encoding) {
    objectEncoding_ = object_encoding;
  }

  uint32_t streamId() const { return streamId_; }
  void setStreamId(uint32_t stream_id) { streamId_ = stream_id; }
  uint32_t allocateNextStreamId() { return nextStreamId_++; }

  const std::string& streamName() const { return streamName_; }
  void setStreamName(std::string stream_name) {
    streamName_ = std::move(stream_name);
  }

  std::string streamKey() const;

  void bindSession(const std::shared_ptr<RtmpSession>& session);
  std::shared_ptr<RtmpSession> session() const;
  void clearSession();

  RtmpHandshake& handshake() { return handshake_; }
  const RtmpHandshake& handshake() const { return handshake_; }

  RtmpChunkParser& chunkParser() { return chunkParser_; }
  const RtmpChunkParser& chunkParser() const { return chunkParser_; }

  void bindHandler(const std::shared_ptr<RtmpConnection>& handler) {
    handler_ = handler;
  }
  std::shared_ptr<RtmpConnection> handler() const { return handler_.lock(); }

 private:
  ConnectionRole role_ = ConnectionRole::kUnknown;
  uint32_t inChunkSize_ = kDefaultChunkSize;
  uint32_t outChunkSize_ = kDefaultChunkSize;
  uint32_t acknowledgementWindow_ = kDefaultAcknowledgementWindow;
  uint32_t peerBandwidth_ = kDefaultPeerBandwidth;
  size_t receivedBytes_ = 0;
  std::string app_;
  std::string tcUrl_;
  std::string streamName_;
  double objectEncoding_ = 0.0;
  uint32_t streamId_ = 0;
  uint32_t nextStreamId_ = 1;
  std::weak_ptr<RtmpSession> session_;
  // 握手状态机独立封装，避免把字节级细节塞进 server 回调里。
  RtmpHandshake handshake_;
  // chunk 解析器同样是连接级对象，因为每个连接都要维护自己的 csid 状态。
  RtmpChunkParser chunkParser_;
  std::weak_ptr<RtmpConnection> handler_;
};

}  // namespace rmuduo::rtmp
