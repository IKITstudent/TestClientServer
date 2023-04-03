#pragma once
#include <iostream>
#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>

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
