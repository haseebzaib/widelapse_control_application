#include "mqttController.hpp"
#include "utils/utils.hpp"
#include <memory>
#include <cstdlib>

namespace cloudController
{

    namespace mqttController
    {

        static const std::string logTag = "mqttController";
        struct logLevel
        {
            static constexpr std::string_view info = "INFO";
            static constexpr std::string_view warn = "WARN";
            static constexpr std::string_view err = "ERR";
        };
        static std::jthread mqttWorker_;
        static std::unique_ptr<mqtt::async_client> mqttAsyncClient;
        static mqtt::connect_options connOpts;

        static std::string clientId;

        static void logging(std::string_view loggingLevel, std::string msg)
        {
            std::cout << std::format("[{}][{}] {}\n", loggingLevel, logTag, msg);
        }

        class actionListener : public virtual mqtt::iaction_listener
        {
            std::string name_;

            void on_failure(const mqtt::token &tok) override
            {
                logging(logLevel::info, std::format("CB: failure"));
                if (tok.get_message_id() != 0)
                {
                    logging(logLevel::info, std::format("CB:on_failure for token: [{}]", tok.get_message_id()));
                }
            }

            void on_success(const mqtt::token &tok) override
            {
                logging(logLevel::info, std::format("CB: success"));
                if (tok.get_message_id() != 0)
                {
                    logging(logLevel::info, std::format("CB:on_success for token: [{}]", tok.get_message_id()));
                }

                auto top = tok.get_topics();
                if (top && !top->empty())
                {
                    std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
                    logging(logLevel::info, std::format("CB:on_success \t token topic: [{}] ...", (*top)[0]));
                }
            }

        public:
            actionListener(const std::string &name) : name_(name) {}
        };

        class callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener
        {
            // Counter for the number of connection retries
            int nretry_;
            // The MQTT client
            mqtt::async_client &cli_;
            // Options to use if we need to reconnect
            mqtt::connect_options &connOpts_;
            // An action listener to display the result of actions.
            actionListener subListener_;

            void reconnect()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
                try
                {
                    cli_.connect(connOpts_, nullptr, *this);
                }
                catch (const mqtt::exception &exc)
                {
                    logging(logLevel::err, std::format("reconnect: Error: {}", exc.what()));
                    exit(1);
                }
            }

            // Re-connection failure
            void on_failure(const mqtt::token &tok) override
            {
                logging(logLevel::err, "CB: Connection attempt failed");
                if (++nretry_ > n_retry_attempts)
                    exit(1);
                reconnect();
            }

            // (Re)connection success
            // Either this or connected() can be used for callbacks.
            void on_success(const mqtt::token &tok) override {}

            // (Re)connection success
            void connected(const std::string &cause) override
            {

                logging(logLevel::info, "CB: connection success");

                cli_.subscribe(subTopic, qos, nullptr, subListener_);
            }

            // Callback for when the connection is lost.
            // This will initiate the attempt to manually reconnect.
            void connection_lost(const std::string &cause) override
            {
                logging(logLevel::info, "CB: connection lost");
                if (!cause.empty())
                {
                    logging(logLevel::info, std::format("CB: \tcause: {}", cause));
                }

                logging(logLevel::info, "CB: reconnecting ..");
                nretry_ = 0;
                reconnect();
            }

            // Callback for when a message arrives.
            void message_arrived(mqtt::const_message_ptr msg) override
            {

                logging(logLevel::info, "CB: Message Arrived");
                logging(logLevel::info, std::format("CB: \ttopic: {}", msg->get_topic()));
                logging(logLevel::info, std::format("CB: \tpayload: {}", msg->to_string()));
            }

            void delivery_complete(mqtt::delivery_token_ptr token) override {}

        public:
            callback(mqtt::async_client &cli, mqtt::connect_options &connOpts)
                : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription")
            {
            }
        };

        static std::unique_ptr<callback> mqttCallback;

        static void
        generate_client_id()
        {
            clientId = "widelapse_";
            std::string mac = utils::get_ethernet_mac();

            if (mac.empty())
            {

                int randomNum = std::rand() % 90000 + 10000;
                clientId += std::to_string(randomNum);
                logging(logLevel::warn, std::format("no mac so random clientID: {}", clientId));
                return;
            }
            mac.erase(std::remove(mac.begin(), mac.end(), ':'), mac.end());
            clientId += mac;
            logging(logLevel::info, std::format("MQTT clientID: {}", clientId));
        }

        static void mqtt_init()
        {

            generate_client_id();

            mqttAsyncClient = std::make_unique<mqtt::async_client>(serverUri, clientId);
            connOpts.set_clean_session(true);
            connOpts.set_keep_alive_interval(30);

            mqttCallback = std::make_unique<callback>(
                *mqttAsyncClient,
                connOpts);

            // Register MQTT event callbacks
            mqttAsyncClient->set_callback(*mqttCallback);

            try
            {
                logging(logLevel::info, std::format("Connecting to the MQTT server : {}", serverUri));

                mqttAsyncClient->connect(connOpts, nullptr, *mqttCallback);
            }
            catch (const mqtt::exception &exc)
            {

                logging(logLevel::err, std::format("unable to connect to the MQTT server : {}, exeception: {}", serverUri, exc.what()));
            }
        }

        static void mqtt_thread(std::stop_token stopToken)
        {
            mqtt_init();

            while (!stopToken.stop_requested())
            {

                logging(logLevel::info, "working every second");

                if (mqttAsyncClient && mqttAsyncClient->is_connected())
                {
                    logging(logLevel::info, "MQTT Connected");
                }
                else
                {
                    logging(logLevel::warn, "MQTT Not Connected");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
        }

        void init()
        {

            mqttWorker_ = std::jthread(mqtt_thread);
        }

        void stop()
        {
            mqttWorker_.request_stop();
            if (mqttWorker_.joinable())
            {
                mqttWorker_.join();
            }
        }

    }

}