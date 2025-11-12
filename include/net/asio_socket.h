#pragma once
#include <boost/asio.hpp>

namespace ASIO{
    using UdpSocket = boost::asio::ip::udp::socket;
    using UdpEndpoint = boost::asio::ip::udp::endpoint;
    using TcpSocket = boost::asio::ip::tcp::socket;
    using TcpEndpoint = boost::asio::ip::tcp::endpoint;
    using IoContext = boost::asio::io_context;
}

