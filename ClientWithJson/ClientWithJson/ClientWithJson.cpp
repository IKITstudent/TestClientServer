//#pragma once
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>
//#include "logger.hpp"

#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y)
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)


using nlohmann::json;
using namespace boost::asio;
io_service service;

//перенс логер в код клиента, так как пока только разбираюсь как подключать в compile explorer внутренние библиотеки
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

Log logger;

class talk_to_svr : public boost::enable_shared_from_this<talk_to_svr>
    , boost::noncopyable {
    typedef talk_to_svr self_type;
    talk_to_svr(const std::string& message)
        : sock_(service), started_(true), message_(message)
    {
        logger.Logging("Попытка отправить сообщение:", message);
        try
        {
            logger.Logging("Попытка конвертирования сообщения в json:", message);
            message_ = Covert_output_json_to_bson(Convert_output_to_json(message));
        }
        catch(const std::error_code err)
        {
            logger.Logging("Ошибка конвертирования сообщения в json:", err.message());
        }
    }
    void start(ip::tcp::endpoint ep) {
        sock_.async_connect(ep, MEM_FN1(on_connect, _1));
    }
public:
    typedef boost::system::error_code error_code;
    typedef boost::shared_ptr<talk_to_svr> ptr;

    static ptr start(ip::tcp::endpoint ep, const std::string& message) {
        ptr new_(new talk_to_svr(message));
        new_->start(ep);
        return new_;
    }
    void stop() {
        if (!started_) return;
        
        logger.Logging("Закрытие соединения");
        started_ = false;
        sock_.close();
    }
    bool started() { return started_; }
private:
    void output_result(json message)
    {
        logger.Logging("Вывод сообщения в консоль");
        std::cout << message.at("primer") << " = " << message.at("answer")<<std::endl;
    }
    std::string Covert_output_json_to_bson(json message)
    {
        std::string output = "";
        logger.Logging("Конвертирование исходящего json в bson");
        try
        {
            std::vector<uint8_t>bytes = json::to_bson(message);

            for (int i = 0; i < bytes.size(); ++i)
            {
                output += bytes[i];
            }

            logger.Logging("Json конвертирован в bson");
        }
        catch (const std::error_code err)
        {
            logger.Logging("Ошибка в обработке исходящего сообщения", err.message());
        }

        return output;
    }

    json Convert_output_to_json(std::string message)
    {
        json output;
        logger.Logging("создание json файла с сообщением:",message );
        output["primer"] = message;
        return output;
    }

    json Convert_input_to_json(std::string message)
    {
        logger.Logging("Конвертирование входящего bson в json");
        json input;
        std::vector<uint8_t> bytes;
        try
        {
            for (int i = 0; message[i] != '\n'; ++i)
            {
                bytes.push_back(message[i]);
            }

            input = json::from_bson(bytes);
            logger.Logging("Bson конвертирован в json");
        }
        catch (const json::parse_error err)
        {
            logger.Logging("Ошибка в конвертации", err.what());
        }
        return input;
    }

    void on_connect(const error_code& err) {
        if (!err)
        {
            logger.Logging("Отправка сообщение:", message_);
            do_write(message_ + '\n');
        }
        else
        {
            logger.Logging("Ошибка:", err.message());
            stop();
        }
    }
    void on_read(const error_code& err, size_t bytes) {
        logger.Logging("Чтение сообщения с сервера");
        if (err)
        {   
            logger.Logging("Ошибка чтения сообщения с сервера",err.message());
            stop();
        }
        if (!started()) return;
        // process the msg
        std::string msg(read_buffer_, bytes);

        output_result(Convert_input_to_json(msg));
        //std::cout << Convert_input_to_json(msg) << std::endl;
    }

    void on_write(const error_code& err, size_t bytes) {
        do_read();
    }
    void do_read() {
        async_read(sock_, buffer(read_buffer_),
            MEM_FN2(read_complete, _1, _2), MEM_FN2(on_read, _1, _2));
    }
    void do_write(const std::string& msg)
    {
        if (!started()) return;
        std::copy(msg.begin(), msg.end(), write_buffer_);
        sock_.async_write_some(buffer(write_buffer_, msg.size()),
            MEM_FN2(on_write, _1, _2));
    }
    size_t read_complete(const boost::system::error_code& err, size_t bytes) {
        if (err) return 0;
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
        // we read one-by-one until we get to enter, no buffering
        return found ? 0 : 1;
    }

private:
    ip::tcp::socket sock_;
    enum { max_msg = 2048 };
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
    std::string message_;
};

void Input_math_expression(ip::tcp::endpoint &ep)
{
    setlocale(LC_ALL, "Russian");
    std::cout << "Введите простое математическое выражение" << std::endl;
    std::string input;
    std::cin >> input;
    talk_to_svr::start(ep, input);
    service.run();
}

int main(int argc, char* argv[]) 
{
    ip::tcp::endpoint ep(ip::address::from_string("127.0.0.1"), 8001);
    logger.Logging("адресс сервера, порт", ep.address().to_string()+" "+std::to_string(ep.port()));
    Input_math_expression(ep);

}