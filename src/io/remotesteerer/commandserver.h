#ifndef LIBGEODECOMP_IO_REMOTESTEERER_COMMANDSERVER_H
#define LIBGEODECOMP_IO_REMOTESTEERER_COMMANDSERVER_H

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <cerrno>
#include <iostream>
#include <string>
#include <stdexcept>
#include <map>
#include <libgeodecomp/config.h>
#include <libgeodecomp/io/logger.h>
#include <libgeodecomp/io/remotesteerer/action.h>
#include <libgeodecomp/io/remotesteerer/interactor.h>
#include <libgeodecomp/misc/stringops.h>

namespace LibGeoDecomp {

namespace RemoteSteererHelpers {

using boost::asio::ip::tcp;

/**
 * A server which can be reached by TCP (nc, telnet, ...). Its purpose
 * is to do connection handling and parsing of incoming user commands.
 * Action objects can be bound to certain commands and will be
 * invoked. This allows a flexible extension of the CommandServer's
 * functionality by composition, without having to resort to inheritance.
 */
template<typename CELL_TYPE>
class CommandServer
{
public:
    typedef SuperMap<std::string, boost::shared_ptr<Action<CELL_TYPE> > > ActionMap;

    /**
     * This helper class lets us and the user safely close the
     * CommandServer's network service, which is nice as it is using
     * blocking IO and it's a major PITA to cancel that.
     */
    class QuitAction : public Action<CELL_TYPE>
    {
    public:
        QuitAction(bool *continueFlag) :
            Action<CELL_TYPE>("quit", "Terminates the CommandServer and closes its socket."),
            continueFlag(continueFlag)
        {}

        void operator()(const StringVec& parameters, Pipe& pipe)
        {
            LOG(INFO, "QuitAction called");
            *continueFlag = false;
        }

    private:
        bool *continueFlag;
    };

    /**
     * This Action is helpful if a given user command has to be
     * executed by a Handler on the simulation node (i.e. all commands
     * which work on grid data).
     */
    class PassThroughAction : public Action<CELL_TYPE>
    {
    public:
        using Action<CELL_TYPE>::key;

        PassThroughAction(const std::string& key, const std::string& helpMessage) :
            Action<CELL_TYPE>(key, helpMessage)
        {}

        void operator()(const StringVec& parameters, Pipe& pipe)
        {
            pipe.addSteeringRequest(key() + " " + StringOps::join(parameters, " "));
        }
    };

    /**
     * This class is just a NOP, which may be used by the client to
     * retrieve new steering feedback. This can't happen automatically
     * as the CommandServer's listener thread blocks for input from
     * the client.
     */
    class PingAction : public Action<CELL_TYPE>
    {
    public:
        using Action<CELL_TYPE>::key;

        PingAction() :
            Action<CELL_TYPE>("ping", "wake the CommandServer, useful to retrieve a new batch of feedback"),
            c(0)
        {}

        void operator()(const StringVec& parameters, Pipe& pipe)
        {
            // Do only reply if there is no feedback already waiting.
            // This is useful if the client is using ping to keep us
            // alive, but can only savely read back one line in
            // return. In that case this stragety avoids a memory leak
            // in our write buffer.
            if (pipe.copySteeringFeedback().size() == 0) {
                pipe.addSteeringFeedback("pong " + StringOps::itoa(++c));
            }
        }

    private:
        int c;
    };

    class GetAction : public PassThroughAction
    {
    public:
        GetAction() :
            PassThroughAction("get", "usage: \"get X Y [Z] MEMBER\", will return member MEMBER of cell at grid coordinate (X, Y, Z) if the model is 3D, or (X, Y) in the 2D case")
        {}
    };

    class SetAction : public PassThroughAction
    {
    public:
        SetAction() :
            PassThroughAction("set", "usage: \"get X Y [Z] MEMBER VALUE\", will set member MEMBER of cell at grid coordinate (X, Y, Z) (if the model is 3D, or (X, Y) in the 2D case) to value VALUE")
        {}
    };

    class WaitAction : public Action<CELL_TYPE>
    {
    public:
        WaitAction() :
            Action<CELL_TYPE>("wait", "usage: \"wait\", will wait until feedback from the simulation has been received")
        {}

        void operator()(const StringVec& parameters, Pipe& pipe)
        {
            pipe.waitForFeedback();
        }
    };

    CommandServer(
        int port,
        boost::shared_ptr<Pipe> pipe) :
        port(port),
        pipe(pipe),
        serverThread(&CommandServer::runServer, this)
    {
        addAction(new QuitAction(&continueFlag));
        addAction(new SetAction);
        addAction(new GetAction);
        addAction(new WaitAction);
        addAction(new PingAction);

        // The thread may take a while to start up. We need to wait
        // here so we don't try to clean up in the d-tor before the
        // thread has created anything.
        boost::unique_lock<boost::mutex> lock(mutex);
        while(!acceptor) {
            threadCreationVar.wait(lock);
        }
    }

    ~CommandServer()
    {
        signalClose();
        LOG(DEBUG, "CommandServer waiting for network thread");
        serverThread.join();
    }

    /**
     * Sends a message back to the end user. This is the primary way
     * for (user-defined) Actions to give feedback.
     */
    void sendMessage(const std::string& message)
    {
        LOG(DEBUG, "CommandServer::sendMessage(" << message << ")");
        boost::system::error_code errorCode;
        boost::asio::write(
            *socket,
            boost::asio::buffer(message),
            boost::asio::transfer_all(),
            errorCode);

        if (errorCode) {
            LOG(WARN, "CommandServer::sendMessage encountered " << errorCode.message());
        }
    }

    /**
     * A convenience method to send a string to a CommandServer
     * listeting on the given host/port combination.
     */
    static void sendCommand(const std::string& command, int port, const std::string& host = "127.0.0.1")
    {
        sendCommandWithFeedback(command, 0, port, host);
    }

    static StringVec sendCommandWithFeedback(const std::string& command, int feedbackLines, int port, const std::string& host = "127.0.0.1")
    {
        LOG(DEBUG, "CommandServer::sendCommandWithFeedback(" << command << ", port = " << port << ", host = " << host << ")");
        Interactor interactor(command, feedbackLines, false, port, host);
        interactor();
        return interactor.feedback();
    }

    /**
     * Register a server-side callback for handling user input. The
     * CommandServer will assume ownership of the action and free its
     * memory upon destruction.
     */
    void addAction(Action<CELL_TYPE> *action)
    {
        actions[action->key()] = boost::shared_ptr<Action<CELL_TYPE> >(action);
    }

private:
    int port;
    boost::shared_ptr<Pipe> pipe;
    boost::asio::io_service ioService;
    boost::shared_ptr<tcp::acceptor> acceptor;
    boost::shared_ptr<tcp::socket> socket;
    boost::thread serverThread;
    boost::condition_variable threadCreationVar;
    boost::mutex mutex;
    ActionMap actions;
    bool continueFlag;

    void runSession()
    {
        for (;;) {
            boost::array<char, 1024> buf;
            boost::system::error_code errorCode;
            size_t length = socket->read_some(boost::asio::buffer(buf), errorCode);

            if (length > 0) {
                std::string input(buf.data(), length);
                handleInput(input);
            }

            if (errorCode == boost::asio::error::eof) {
                LOG(INFO, "CommandServer::runSession(): client closed connection");
                return;
            }

            if (errorCode) {
                LOG(WARN, "CommandServer::runSession encountered " << errorCode.message());
            }

            if (!socket->is_open()) {
                return;
            }

            StringVec feedback = pipe->retrieveSteeringFeedback();
            for (StringVec::iterator i = feedback.begin();
                 i != feedback.end();
                 ++i) {
                LOG(DEBUG, "CommandServer::runSession sending »" << *i << "«");
                sendMessage(*i + "\n");
            }
        }
    }

    void handleInput(const std::string& input)
    {
        LOG(DEBUG, "CommandServer::handleInput(" << input << ")");
        StringVec lines = StringOps::tokenize(input, "\n");
        std::string zeroString("x");
        zeroString[0] = 0;

        for (StringVec::iterator iter = lines.begin();
             iter != lines.end();
             ++iter) {
            StringVec parameters = StringOps::tokenize(*iter, " \n\r");

            if (*iter == zeroString) {
                // silently ignore strings containing a single 0
                continue;
            }

            if (parameters.size() == 0) {
                sendMessage("no command given\n");
                continue;
            }

            std::string command = parameters.pop_front();
            if (actions.count(command) == 0) {
                std::string message = "command not found: " + command;
                LOG(WARN, message);

                message += "\ntry \"help\"\n";
                sendMessage(message);
            } else {
                (*actions[command])(parameters, *pipe);
            }
        }
    }

    int runServer()
    {
        try {
            // Thread-aware initialization, allows c-tor to exit safely.
            {
                boost::unique_lock<boost::mutex> lock(mutex);
                acceptor.reset(new tcp::acceptor(ioService, tcp::endpoint(tcp::v4(), port)));
            }
            threadCreationVar.notify_one();

            boost::system::error_code errorCode;
            continueFlag = true;

            while (continueFlag) {
                LOG(DEBUG, "CommandServer: waiting for new connection");
                socket.reset(new tcp::socket(ioService));
                acceptor->accept(*socket, errorCode);

                if (errorCode) {
                    LOG(WARN, "CommandServer::runServer() encountered " << errorCode.message());
                } else {
                    LOG(INFO, "CommandServer: client connected");
                    runSession();
                    LOG(INFO, "CommandServer: client disconnected");
                }
            }
        }
        catch (std::exception& e) {
            LOG(FATAL, "CommandServer::runServer() caught exception " << e.what() << ", exiting");
            return 1;
        }

        return 0;
    }

    void signalClose()
    {
        sendCommand("quit", port);
    }
};


}

}

#endif
