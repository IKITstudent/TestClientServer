#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <nlohmann/json.hpp>
//#include "Parser.hpp"
//#include "logger.hpp"
#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/spirit/home/x3.hpp>
#include <iomanip>

using namespace boost::asio;
using namespace boost::posix_time;
using nlohmann::json;
io_service service;

//перенс логер и парсер в код сервера, так как пока только разбираюсь как подключать в compile explorer внутренние библиотеки
//проект реализовывал на visual studio 2022 используя заголовочные файлы
class Log
{
public:
    Log()
    {
        logs.open("Logs.txt", std::ios::app);
        logs << Delimit_log();
        Logging("start programm");
    }
    ~Log()
    {
        Logging("stop programm");
        logs.close();
    }
    void Logging(std::string message)
    {
        logs << getTime() << '\n' << message << '\n' << Delimit_log();
    }
    template <typename T>
    void Logging(std::string message, const T log)
    {
        logs << getTime() << '\n' << message << '\n' << log << '\n' << Delimit_log();
    }



private:
    std::string Delimit_log()
    {
        return "=====================\n";
    }
    boost::posix_time::ptime getTime()
    {
        boost::posix_time::ptime datetime = boost::posix_time::microsec_clock::universal_time();
        return datetime;
    }

private:
    std::ofstream logs;
};

namespace x3 = boost::spirit::x3;

using V = int32_t;
namespace Parser
{

    x3::rule<struct expr, V> const   expr{ "expr" };
    x3::rule<struct simple, V> const simple{ "simple" };
    x3::rule<struct factor, V> const factor{ "factor" };

    auto assign = [](auto& ctx) { _val(ctx) = _attr(ctx); };
#define BINOP(op, rhs) (x3::lit(#op) >> x3::as_parser(rhs) \
         [([](auto& ctx) { _val(ctx) = _val(ctx) op _attr(ctx); })])

    auto simple_def = x3::double_ | '(' >> expr >> ')';

    auto factor_def = simple[assign] >>
        *(BINOP(*, factor)
            | BINOP(/ , factor)
            | BINOP(%, factor));

    auto expr_def = factor[assign] >>
        *(BINOP(+, expr)
            | BINOP(-, expr));

    BOOST_SPIRIT_DEFINE(expr, factor, simple)
};

class Math_Parser
{
public:
    static V evaluate(std::string_view text)
    {
        V value{};
        if (!phrase_parse(text.begin(), text.end(), Parser::expr >> x3::eoi,
            x3::space, value))
            throw std::runtime_error("error in expression");
        return value;
    }
};

Log logger;

class talk_to_client;
typedef boost::shared_ptr<talk_to_client> client_ptr;
typedef std::vector<client_ptr> array;
array clients;

#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y)
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)


class talk_to_client : public boost::enable_shared_from_this<talk_to_client>
    , boost::noncopyable {
    typedef talk_to_client self_type;
    talk_to_client() : sock_(service), started_(false)
    {
    }
public:
    typedef boost::system::error_code error_code;
    typedef boost::shared_ptr<talk_to_client> ptr;

    void start() {
        started_ = true;
        do_read();
    }
    static ptr new_() {
        ptr new_(new talk_to_client);
        return new_;
    }
    void stop() {
        if (!started_) return;

        logger.Logging("Закрытие соединения", sock_.remote_endpoint().address());
        started_ = false;
        sock_.close();
        ptr self = shared_from_this();
    }
    bool started() const { return started_; }
    ip::tcp::socket& sock() { return sock_; }
private:

    json Convert_input_to_json(std::string& input)
    {
        logger.Logging("Конвертирование входящего bson в json");
        json in;
        std::vector<uint8_t> bytes;

        try {
            for (int i = 0; input[i] != '\n'; ++i)
            {
                bytes.push_back(input[i]);
            }

            in = json::from_bson(bytes);
            logger.Logging("Bson конвертирован в json");
        }

        catch (const json::parse_error err)
        {
            logger.Logging("Ошибка в конвертации", err.what());
        }

        return in;
    }

    std::string Convert_output_json_to_bson(json& output)
    {
        std::string out = "";
        try {
            std::vector<uint8_t> bytes = json::to_bson(output);
            for (int i = 0; i < bytes.size(); ++i)
            {
                out += bytes.at(i);
            }

            out += '\n';
            logger.Logging("Json конвертирован в bson");
        }
        catch (const std::error_code err)
        {
            logger.Logging("Ошибка в обработке исходящего сообщения", err.message());
        }
        return out;
    }

    std::string Calculate(std::string& input)
    {
        logger.Logging("Вычисление выражения");
        std::string result = std::to_string(Math_Parser::evaluate(input));
        logger.Logging("Выражение вычислено", result);
        return result;
    }

    void on_read(const error_code& err, size_t bytes) 
    {
        logger.Logging("Чтение сообщения с клиентом");
        std::string msg(read_buffer_, bytes);
        if (err)
        {
            logger.Logging("Ошибка чтения сообщения с клиента", err.message());
            stop();
        }
        if (!started()) 
            return;
        
        logger.Logging("Конвертирование входящего файла в json");
        json json_message = Convert_input_to_json(msg);

        std::string message;

        logger.Logging("Проверка на наличие мат. выражения");
        if(json_message.contains("primer"))
        {
            message=json_message.at("primer");
            logger.Logging("Успешно", message);

            json_message["answer"] = Calculate(message);

            if (sock_.is_open())
                Send_result(json_message);
        }
        else
        {
            logger.Logging("Не найдено");

            json_message["answer"] = "Ошибка";

            if (sock_.is_open())
                Send_result(json_message);
        }

        
    }

    void Send_result(json &json_message)
    {
        logger.Logging("Отправка результатов");
        std::string message;
        try {
             message= Convert_output_json_to_bson(json_message);
        }
        catch (const json::parse_error err)
        {
            logger.Logging("Ошибка", err.what());
        }
        do_write(message);
        stop();
    }

    void on_write(const error_code& err, size_t bytes) {
        do_read();
    }
    void do_read() {
        async_read(sock_, buffer(read_buffer_),
            MEM_FN2(read_complete, _1, _2), MEM_FN2(on_read, _1, _2));
    }

    void do_write(const std::string& msg) {
        if (!started()) return;
        std::copy(msg.begin(), msg.end(), write_buffer_);
        sock_.async_write_some(buffer(write_buffer_, msg.size()),
            MEM_FN2(on_write, _1, _2));
    }
    size_t read_complete(const boost::system::error_code& err, size_t bytes) {
        if (err) return 0;
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
        return found ? 0 : 1;
    }
private:
    ip::tcp::socket sock_;
    enum { max_msg = 1024 };
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
    std::string message_;    
};

ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 8001));

void handle_accept(talk_to_client::ptr client, const boost::system::error_code& err) {
    client->start();
    logger.Logging("Начато соединение", client.get()->sock().remote_endpoint().address());
    talk_to_client::ptr new_client = talk_to_client::new_();
    acceptor.async_accept(new_client->sock(), boost::bind(handle_accept, new_client, _1));
}


int main(int argc, char* argv[]) {
    talk_to_client::ptr client = talk_to_client::new_();
    acceptor.async_accept(client->sock(), boost::bind(handle_accept, client, _1));
    service.run();
}