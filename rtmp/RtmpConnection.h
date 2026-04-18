#pragma once

#include <cstdint>
#include <string>

#include <rmuduo/net/Callbacks.h>
#include <rmuduo/net/Timestamp.h>

#include "RtmpCommandMessage.h"

namespace rmuduo {
class Buffer;
}

namespace rmuduo::rtmp {

class RtmpConnectionContext;
class RtmpServer;

class RtmpConnection {
 public:
  explicit RtmpConnection(RtmpServer* server) : server_(server) {}

  bool onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime,
                 RtmpConnectionContext* context);

 private:
  bool handleMessage(const TcpConnectionPtr& conn, const RtmpMessage& message,
                     RtmpConnectionContext* context);
  bool handleCommandMessage(const TcpConnectionPtr& conn,
                            RtmpConnectionContext* context,
                            const RtmpCommandMessage& command);
  bool handleConnect(const TcpConnectionPtr& conn,
                     RtmpConnectionContext* context,
                     const RtmpCommandMessage& command);
  bool handleCreateStream(const TcpConnectionPtr& conn,
                          RtmpConnectionContext* context,
                          const RtmpCommandMessage& command);
  bool handlePublish(const TcpConnectionPtr& conn,
                     RtmpConnectionContext* context,
                     const RtmpCommandMessage& command);
  bool handlePlay(const TcpConnectionPtr& conn,
                  RtmpConnectionContext* context,
                  const RtmpCommandMessage& command);
  bool handleDeleteStream(const TcpConnectionPtr& conn,
                          RtmpConnectionContext* context,
                          const RtmpCommandMessage& command);
  bool handleMediaMessage(const TcpConnectionPtr& conn,
                          const RtmpMessage& message,
                          RtmpConnectionContext* context);

  void sendWindowAcknowledgementSize(const TcpConnectionPtr& conn,
                                     uint32_t window_size) const;
  void sendSetPeerBandwidth(const TcpConnectionPtr& conn,
                            uint32_t peer_bandwidth) const;
  void sendSetChunkSize(const TcpConnectionPtr& conn, uint32_t chunk_size) const;
  void sendConnectSuccess(const TcpConnectionPtr& conn,
                          const RtmpCommandMessage& command) const;
  void sendCreateStreamResult(const TcpConnectionPtr& conn,
                              const RtmpCommandMessage& command,
                              uint32_t stream_id) const;
  void sendOnStatus(const TcpConnectionPtr& conn, uint32_t message_stream_id,
                    std::string_view code, std::string_view description,
                    bool error) const;

  RtmpServer* server_ = nullptr;
};

}  // namespace rmuduo::rtmp
