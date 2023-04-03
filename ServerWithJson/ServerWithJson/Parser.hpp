#pragma once
#include <boost/spirit/home/x3.hpp>
#include <iostream>
#include <iomanip>
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

static class Math_Parser
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
