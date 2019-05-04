/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <mutex>

#include <unistd.h>

#include <boost/program_options/parsers.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include <asio/connect.hpp>
#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>
#include <asio/write.hpp>

#include <asio.hpp>

#include "baton.hpp"
#include "beast.hpp"
#include "channel.hpp"
#include "commands.hpp"
#include "crypto.hpp"
//#include "ethereum.hpp"
#include "http.hpp"
#include "scope.hpp"
#include "secure.hpp"
#include "socket.hpp"
#include "task.hpp"
#include "trace.hpp"

#define _disused \
    __attribute__((__unused__))


namespace po = boost::program_options;

namespace orc {

static po::variables_map args;

static std::vector<std::string> ices_;

class Back {
  public:
    virtual task<std::string> Respond(const std::string &offer) = 0;
};

class Outgoing final :
    public Connection
{
  protected:
    void Land(rtc::scoped_refptr<webrtc::DataChannelInterface> interface) override {
    }

    void Stop(const std::string &error) override {
        std::terminate();
    }

  public:
    Outgoing() :
        Connection()
    {
    }

    virtual ~Outgoing() {
_trace();
        Close();
    }
};

class Account {
  public:
    virtual void Land(const Buffer &data) = 0;
    virtual void Bill(uint64_t amount) = 0;
};

template <typename Type_>
class Output :
    public Pump<Account>,
    public BufferDrain
{
    template <typename Base_, typename Inner_, typename Drain_>
    friend class Sink;

  private:
    const Tag tag_;

  protected:
    virtual Type_ *Inner() = 0;

    void Land(const Buffer &data) override {
        auto used(Outer());
        used->Bill(1);
        used->Land(Tie(tag_, data));
    }

    void Stop(const std::string &error) override {
        Pump<Account>::Stop();
        // XXX: implement (efficiently ;P)
    }

  public:
    Output(Account *drain, const Tag &tag) :
        Pump<Account>(drain),
        tag_(tag)
    {
_trace();
    }

    virtual ~Output() {
_trace();
    }

    task<void> Send(const Buffer &data) override {
        co_return co_await Inner()->Send(data);
    }

    task<void> Shut() override {
        co_await Inner()->Shut();
        co_await Pump<Account>::Shut();
    }

    Type_ *operator ->() {
        return Inner();
    }
};

class Replay final :
    public Pump<Account>
{
  private:
    Beam request_;
    Beam response_;

  public:
    task<void> Send(const Buffer &data) override {
        _assert(request_ == data);
        Outer()->Land(response_);
        co_return;
    }
};

class Space final :
    public std::enable_shared_from_this<Space>,
    public Pipe,
    public Account
{
  private:
    const S<Back> back_;

    Pipe *input_;

    std::map<Tag, U<Pump<Account>>> outputs_;
    std::map<Tag, S<Outgoing>> outgoing_;

    int64_t balance_;

  public:
    Space(S<Back> back) :
        back_(std::move(back))
    {
    }

    ~Space() {
_trace();
    }


    // XXX: implement a set

    void Associate(Pipe *input) {
        input_ = input;
    }

    void Dissociate(Pipe *input) {
        if (input_ == input)
            input_ = nullptr;
    }


    task<void> Send(const Buffer &data) override {
        _assert(input_ != nullptr);
        Bill(1);
        co_return co_await input_->Send(data);
    }

    task<void> Shut() {
        for (auto &output : outputs_)
            co_await output.second->Shut();
    }


    void Land(const Buffer &data) override {
        Task([self = shared_from_this(), data = Beam(data)]() -> task<void> {
            co_await self->Send(data);
        });
    }

    void Bill(uint64_t amount) override {
        balance_ -= amount;
    }


    task<void> Call(const Buffer &data, std::function<task<void> (const Buffer &)> code) {
        auto [command, args] = Take<TagSize, 0>(data);
        if (false) {


        } else if (command == BatchTag) {
            // XXX: make this more efficient
            std::string builder;
            Call(data, [&](const Buffer &data) -> task<void> {
                builder += data.str();
                co_return;
            });
            co_return co_await code(Tie(Strung(std::move(builder))));

        } else if (command == DiscardTag) {
            co_return;

        } else if (command == CloseTag) {
            const auto [tag] = Take<TagSize>(args);
            auto output(outputs_.find(tag));
            _assert(output != outputs_.end());
            co_await output->second->Shut();
            outputs_.erase(output);
            co_return co_await code(Tie());


        } else if (command == ConnectTag) {
            // XXX: use something saner than : separation?
            const auto [tag, target] = Take<TagSize, 0>(args);
            auto string(target.str());
            auto colon(string.rfind(':'));
            _assert(colon != std::string::npos);
            auto host(string.substr(0, colon));
            auto port(string.substr(colon + 1));
            auto output(std::make_unique<Sink<Output<Socket<asio::ip::udp::socket>>, Socket<asio::ip::udp::socket>>>(this, tag));
            auto socket(output->Wire<Socket<asio::ip::udp::socket>>());
            auto endpoint(co_await socket->_(host, port));
            auto place(outputs_.emplace(tag, std::move(output)));
            _assert(place.second);
            co_return co_await code(Tie(Strung(std::move(endpoint))));


        } else if (command == EstablishTag) {
            const auto [handle] = Take<TagSize>(args);
            auto outgoing(Make<Outgoing>());
            outgoing_[handle] = outgoing;
            co_return co_await code(Tie());

        } else if (command == OfferTag) {
            const auto [handle] = Take<TagSize>(args);
            auto outgoing(outgoing_.find(handle));
            _assert(outgoing != outgoing_.end());
            auto offer(Strip(co_await outgoing->second->Offer()));
            co_return co_await code(Tie(Strung(std::move(offer))));

        } else if (command == NegotiateTag) {
            const auto [handle, answer] = Take<TagSize, 0>(args);
            auto outgoing(outgoing_.find(handle));
            _assert(outgoing != outgoing_.end());
            co_await outgoing->second->Negotiate(answer.str());
            co_return co_await code(Tie());

        } else if (command == ChannelTag) {
            // XXX: add label, protocol, and maybe id arguments
            const auto [handle, tag] = Take<TagSize, TagSize>(args);
            auto outgoing(outgoing_.find(handle));
            _assert(outgoing != outgoing_.end());
            auto output(std::make_unique<Sink<Output<Channel>, Channel>>(this, tag));
            output->Wire<Channel>(outgoing->second);
            auto place(outputs_.emplace(tag, std::move(output)));
            _assert(place.second);
            co_return co_await code(Tie());

        } else if (command == CancelTag) {
            const auto [handle] = Take<TagSize>(args);
            outgoing_.erase(handle);
            co_return co_await code(Tie());

        } else if (command == FinishTag) {
            const auto [tag] = Take<TagSize>(args);
            auto output(outputs_.find(tag));
            _assert(output != outputs_.end());
            // XXX: this is extremely unfortunate
            auto channel(dynamic_cast<Output<Channel> *>(output->second.get()));
            _assert(channel != NULL);
            co_await (*channel)->_();
            co_return co_await code(Tie());


        } else if (command == AnswerTag) {
            const auto [offer] = Take<0>(args);
            auto answer(co_await back_->Respond(offer.str()));
            co_return co_await code(Tie(Strung(std::move(answer))));


        } else {
            _assert_(false, "unknown command: " << data);
        }
    }

    task<void> Call(const Buffer &data) {
        Bill(1);

        auto [nonce, rest] = Take<TagSize, 0>(data);
        auto output(outputs_.find(nonce));
        if (output != outputs_.end()) {
            Bill(1);
            co_return co_await output->second->Send(rest);
        } else {
            try {
                // reference to local binding 'nonce' declared in enclosing function 'orc::Space::Call'
                co_return co_await Call(rest, [this, &nonce = nonce](const Buffer &data) -> task<void> {
                    co_return co_await Send(Tie(nonce, data));
                });
            } catch (const Error &error) {
                co_return co_await Send(Tie(Zero, nonce, Strung(error.message)));
            }
        }
    }
};

class Ship {
  public:
    virtual S<Space> Find(const Common &common) = 0;
};

class Conduit :
    public Pipe,
    public BufferDrain
{
  private:
    S<Pipe> self_;
    S<Space> space_;

    void Assign(S<Space> space) {
        space_ = std::move(space);
        space_->Associate(this);
    }

  protected:
    virtual Secure *Inner() = 0;

    void Land(const Buffer &data) override {
        _assert(space_ != nullptr);
        Task([space = space_, data = Beam(data)]() -> task<void> {
            space->Bill(1);
            co_return co_await space->Call(data);
        });
    }

    void Stop(const std::string &error) override {
        Task([this]() -> task<void> {
            co_await Inner()->Shut();
            // XXX: space needs to be reference counted
            co_await space_->Shut();
            self_.reset();
        });
    }

  public:
    static std::pair<S<Conduit>, Sink<Secure> *> Spawn(S<Ship> ship) {
        auto conduit(Make<Sink<Conduit, Secure>>());
        conduit->self_ = conduit;
        auto secure(conduit->Wire<Sink<Secure>>(true, [ship, conduit = conduit.get()]() -> bool {
            Identity identity;
            auto space(ship->Find(identity.GetCommon()));
            conduit->Assign(std::move(space));
            return true;
        }));
        return {conduit, secure};
    }

    task<void> _() {
        co_await Inner()->_();
    }

    virtual ~Conduit() {
_trace();
        if (space_ != nullptr)
            space_->Dissociate(this);
    }

    task<void> Send(const Buffer &data) override {
        space_->Bill(1);
        co_return co_await Inner()->Send(data);
    }
};

class Incoming final :
    public Connection
{
  private:
    S<Incoming> self_;

    const S<Ship> ship_;

  protected:
    void Land(rtc::scoped_refptr<webrtc::DataChannelInterface> interface) override {
        auto [conduit, inner](Conduit::Spawn(ship_));
        auto channel(inner->Wire<Channel>(shared_from_this(), interface));

        Task([conduit = conduit, channel]() -> task<void> {
            co_await channel->_();
            co_await conduit->_();
        });
    }

    void Stop(const std::string &error) override {
        self_.reset();
    }

  public:
    Incoming(S<Ship> ship) :
        Connection(ices_),
        ship_(std::move(ship))
    {
    }

    template <typename... Args_>
    static S<Incoming> Spawn(Args_ &&...args) {
        auto self(Make<Incoming>(std::forward<Args_>(args)...));
        self->self_ = self;
        return self;
    }

    virtual ~Incoming() {
_trace();
        Close();
    }
};

class Node final :
    public std::enable_shared_from_this<Node>,
    public Back,
    public Ship,
    public Identity
{
  private:
    std::mutex mutex_;
    std::map<Common, W<Space>> spaces_;

  public:
    virtual ~Node() {
_trace();
    }

    S<Space> Find(const Common &common) override {
        std::unique_lock<std::mutex> lock(mutex_);
        auto &cache(spaces_[common]);
        if (auto space = cache.lock())
            return space;
        auto space(Make<Space>(shared_from_this()));
        cache = space;
        return space;
    }

    task<std::string> Respond(const std::string &offer) override {
        auto incoming(Incoming::Spawn(shared_from_this()));
        auto answer(co_await incoming->Answer(offer));
        //answer = boost::regex_replace(std::move(answer), boost::regex("\r?\na=candidate:[^ ]* [^ ]* [^ ]* [^ ]* 10\\.[^\r\n]*"), "")
        co_return answer;
    }
};

int Main(int argc, const char *const argv[]) {
    po::options_description options("command-line (only)");
    options.add_options()
        ("help", "produce help message")
    ;

    po::options_description configs("command-line / file");
    configs.add_options()
        ("rendezvous-port", po::value<uint16_t>()->default_value(8080), "port to advertise on blockchain")
        ("ice-stun-server", po::value<std::string>()->default_value("stun:stun.l.google.com:19302"), "stun server url to use for discovery")
    ;

    po::options_description hiddens("you can't see these");
    hiddens.add_options()
    ;

    po::store(po::parse_command_line(argc, argv, po::options_description()
        .add(options)
        .add(configs)
        .add(hiddens)
    ), args);

    if (auto path = getenv("ORCHID_CONFIG"))
        po::store(po::parse_config_file(path, po::options_description()
            .add(configs)
            .add(hiddens)
        ), args);

    po::notify(args);

    if (args.count("help")) {
        std::cout << po::options_description()
            .add(options)
            .add(configs)
        << std::endl;

        return 0;
    }

    ices_.emplace_back(args["ice-stun-server"].as<std::string>());


    auto node(Make<Node>());


    static boost::asio::posix::stream_descriptor out{Context(), ::dup(STDOUT_FILENO)};

    http::basic_router<HttpSession> router{boost::regex::ECMAScript};

    router.post("/", [&](auto request, auto context) {
        http::out::pushn<std::ostream>(out, request);

        try {
            auto offer(request.body());
            auto answer(Wait(node->Respond(offer)));

            Log() << std::endl;
            Log() << "^^^^^^^^^^^^^^^^" << std::endl;
            Log() << offer << std::endl;
            Log() << "================" << std::endl;
            Log() << answer << std::endl;
            Log() << "vvvvvvvvvvvvvvvv" << std::endl;
            Log() << std::endl;

            context.send(Response(request, "text/plain", answer));
        } catch (...) {
            context.send(Response(request, "text/plain", "", boost::beast::http::status::not_found));
        }
    });

    router.all(R"(^.*$)", [&](auto request, auto context) {
        http::out::pushn<std::ostream>(out, request);
        context.send(Response(request, "text/plain", ""));
    });

    auto fail([](auto code, auto from) {
        Log() << "ERROR " << code << " " << from << std::endl;
    });

    HttpListener::launch(Context(), {
        asio::ip::make_address("0.0.0.0"),
        args["rendezvous-port"].as<uint16_t>()
    }, [&](auto socket) {
        HttpSession::recv(std::move(socket), router, fail);
    }, fail);


    asio::signal_set signals(Context(), SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { Context().stop(); });

    Thread().join();
    return 0;
}

}

int main(int argc, const char *const argv[]) {
    return orc::Main(argc, argv);
}