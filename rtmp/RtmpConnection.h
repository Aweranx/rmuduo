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

class RtmpConnection {
 public:
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
};

}  // namespace rmuduo::rtmp
