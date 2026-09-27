#include <haio/internal/platform/asio.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/system/system_error.hpp>

#include <cctype>
#include <iostream>
#include <memory>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

using Request = http::request<http::string_body>;
using Response = http::response<http::vector_body<uint8_t>>;

/**
 * each socket operation is its own small asio coroutine, awaited from the task that
 * owns the socket. the task holds the state; these only borrow it for one step.
 */
asio::awaitable<tcp::acceptor> bind(std::string host, unsigned short port) {
    auto executor = co_await asio::this_coro::executor;
    tcp::resolver resolver(executor);
    try {
        const auto resolved = co_await resolver.async_resolve(host, std::to_string(port), asio::use_awaitable);
        co_return tcp::acceptor(executor, *resolved.begin());
    } catch (const boost::system::system_error& err) {
        // what() carries the boost source line it was thrown from, which is for us
        // and not for whoever typed the port
        throw std::runtime_error("cannot listen on " + host + ":" + std::to_string(port) + ": " + err.code().message());
    }
}

asio::awaitable<tcp::socket> accept(tcp::acceptor& acceptor) {
    co_return co_await acceptor.async_accept(asio::use_awaitable);
}

asio::awaitable<Request> readRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    Request req;
    co_await http::async_read(socket, buffer, req, asio::use_awaitable);
    co_return req;
}

asio::awaitable<void> writeResponse(tcp::socket& socket, Response& res) {
    co_await http::async_write(socket, res, asio::use_awaitable);
}

Haio::Platform::HttpServerRequest toPlatform(const Request& req, const std::string& client) {
    Haio::Platform::HttpServerRequest out;
    out.method = std::string(req.method_string());
    out.target = std::string(req.target());
    out.client = client;
    for (const auto& field : req) {
        std::string name(field.name_string());
        for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        out.headers[std::move(name)] = std::string(field.value());
    }
    return out;
}

Response fromPlatform(Haio::Platform::HttpServerResponse res, const std::string& serverName) {
    Response out{static_cast<http::status>(res.status), 11};
    out.set(http::field::server, serverName);
    for (const auto& [name, value] : res.headers) out.set(name, value);
    out.body() = std::move(res.body);
    out.prepare_payload();
    return out;
}

Haio::Task<void> session(tcp::socket socket, std::shared_ptr<const Haio::Platform::HttpServerOptions> options,
                         std::shared_ptr<const Haio::Platform::HttpHandler> handler) {
    /**
     * the address is read once, before anything can close the socket under us.
     *
     * @todo behind a proxy every request arrives from the proxy, so the per address
     * limits would see one caller. honouring x-forwarded-for needs a list of proxies
     * worth believing, since the header is written by whoever sent the request.
     */
    std::string client;
    if (boost::system::error_code ec; true) {
        const auto peer = socket.remote_endpoint(ec);
        if (!ec) client = peer.address().to_string();
    }

    beast::flat_buffer buffer;
    try {
        for (;;) {
            const auto req = co_await Haio::Platform::Desktop::await(readRequest(socket, buffer));
            const bool close = req.need_eof();
            const bool headOnly = req.method() == http::verb::head;

            auto res = fromPlatform(co_await (*handler)(toPlatform(req, client)), options->name);
            if (headOnly) {
                const auto size = res.body().size();
                res.body().clear();
                res.content_length(size);
            }
            res.keep_alive(!close);
            co_await Haio::Platform::Desktop::await(writeResponse(socket, res));
            if (close) break;
        }
    } catch (const boost::system::system_error& err) {
        // a client closing a keep alive connection arrives here as end_of_stream, and
        // it is the ordinary way a request ends rather than something to report. every
        // other reason is logged, because a swallowed error leaves nothing to debug
        if (err.code() != http::error::end_of_stream) {
            std::cerr << "connection dropped: " << err.code().message() << "\n";
        }
    } catch (const std::exception& err) {
        std::cerr << "connection dropped: " << err.what() << "\n";
    }

    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

}

namespace Haio::Platform {

Task<void> serveHttp(HttpServerOptions options, HttpHandler handler) {
    auto acceptor = co_await Desktop::await(bind(options.host, options.port));
    if (options.ready) options.ready();

    // shared by every session rather than copied into each one
    const auto sharedOptions = std::make_shared<const HttpServerOptions>(std::move(options));
    const auto sharedHandler = std::make_shared<const HttpHandler>(std::move(handler));

    for (;;) {
        auto socket = co_await Desktop::await(accept(acceptor));
        spawn(session(std::move(socket), sharedOptions, sharedHandler));
    }
}

}
