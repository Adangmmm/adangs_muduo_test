#include <set>
#include <functional>
#include <string>

#include "ads_TcpServer.h"
// #include "ads_TcpConnection.h"
// #include "ads_EventLoop.h"
// #include "ads_InetAddress.h"
#include "ads_Logger.h"

using namespace std::placeholders;

class ChatServer{
public:
    ChatServer(EventLoop *loop, const InetAddress &listenAddr)
        : server_(loop, listenAddr, "ads_ChatServer")
        , loop_(loop)
    {
        server_.setConnectionCallback(
            std::bind(&ChatServer::onConnection, this, _1)
        );
        server_.setMessageCallback(
            std::bind(&ChatServer::onMessage, this, _1, _2, _3)
        );
        server_.setThreadNum(3);
    }

    void start(){
        server_.start();
    }

private:
    std::set<TcpConnectionPtr> clients_;    // 用于存储所有已连接客户端

    void onConnection(const TcpConnectionPtr &conn){
        if(conn->connected()){
            LOG_INFO("New client: %s\n", conn->peerAddress().toIpPort().c_str());
            clients_.insert(conn);
        }
        else{
            LOG_INFO("Client disconnected: %s\n", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 实现接收客户端输入，并将其广播给其余客户端
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time){
        // 接收消息
        std::string msg = buf->retrieveAllAsString();
        LOG_INFO("Received message: %s", msg.c_str());

        // 广播消息
        for(const auto &client: clients_){
            if(client != conn){
                client->send(msg);
            }
        }        
    }

    TcpServer server_;
    EventLoop *loop_;
};

int main(){
    EventLoop loop;
    InetAddress listenAddr(8081, "0.0.0.0");
    ChatServer server(&loop, listenAddr);
    server.start();
    loop.loop();
    return 0;
}