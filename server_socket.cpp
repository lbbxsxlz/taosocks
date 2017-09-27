#include "server_socket.h"

namespace taosocks {

void ServerSocket::Start(ULONG ip, USHORT port)
{
    SOCKADDR_IN addrServer = {0};
    addrServer.sin_family = AF_INET;
    addrServer.sin_addr.s_addr = ip;
    addrServer.sin_port = htons(port);

    if(::bind(_fd, (sockaddr*)&addrServer, sizeof(addrServer)) == SOCKET_ERROR)
        assert(0);

    if(::listen(_fd, SOMAXCONN) == SOCKET_ERROR)
        assert(0);

    auto clients = _Accept();
    for(auto& client : clients) {
        client->_Read();
    }
}

void ServerSocket::OnAccepted(std::function<void(ClientSocket*)> onAccepted)
{
    _onAccepted = onAccepted;
}

ClientSocket * ServerSocket::_OnAccepted(AcceptIOContext & io)
{
    auto client = new ClientSocket(_disp, io.fd);
    io.GetAddresses(&client->local, &client->remote);
    AcceptDispatchData data;
    data.client = client;
    Dispatch(data);
    return client;
}

std::vector<ClientSocket*> ServerSocket::_Accept()
{
    std::vector<ClientSocket*> clients;

    for(;;) {
        auto acceptio = new AcceptIOContext();
        auto ret = acceptio->Accept(_fd);
        if(ret.Succ()) {
            auto client = _OnAccepted(*acceptio);
            clients.push_back(client);
            LogLog("_Accept ������ɣ�client fd:%d, remote:%s", client->fd, to_string(client->remote).c_str());
        }
        else if(ret.Fail()) {
            LogFat("_Accept ����code=%d", ret.Code());
            assert(0);
        }
        else if(ret.Async()) {
            LogLog("_Accept �첽");
            break;
        }
    }

    return std::move(clients);
}

void ServerSocket::Invoke(BaseDispatchData & data)
{
    switch(data.optype) {
    case OpType::Accept:
    {
        auto d = static_cast<AcceptDispatchData&>(data);
        _onAccepted(d.client);
        break;
    }
    }
}

void ServerSocket::Handle(BaseIOContext& bio)
{
    if(bio.optype == OpType::Accept) {
        auto aio = static_cast<AcceptIOContext&>(bio);
        auto client = _OnAccepted(aio);
        client->_Read();
        _Accept();
    }
}

}
