#include "parser.h"

#include <format>
#include <list>
#include <array>
#include <algorithm>
#include <functional>

template<typename T, T... S, typename F>
static constexpr void for_sequence(std::integer_sequence<T, S...>, F&& f) {
    (static_cast<void>(f(std::integral_constant<T, S>{})), ...);
}

namespace parser {

static const char EOT = '\0';
static const Variant variantNone = Variant();

enum class TokenClass { OPERATOR, SINGLE, TEXT, SPACE, END };
enum class Priority { XOR, AND, OR, UNKNOWN };

static const std::string& toString(Affinity affinity)
{
    static const std::vector<std::string> values { "UNKNOWN", "BOOLEAN", "INTEGER", "DOUBLE", "STRING" };
    return values[(size_t)affinity];
}
static Variant toAffinity(const Variant& v, Affinity affinity)
{
    return v;
}
static TokenClass getTokenClass(char c)
{
    switch (c) {
        case '(': case '"': return TokenClass::SINGLE;
        case '-': case '+': case '*': case '/': case '%':
        case '=': case '!': case '<': case '>': case '&': case '|': case '^': return TokenClass::OPERATOR;
        case ')': case ',': case EOT: return TokenClass::END;
        default: return std::isspace(c) ? TokenClass::SPACE : TokenClass::TEXT;
    }
}

static std::string charToVisible(char c)
{
    char buffer[8] { };
    if (c < 31)
        return std::string(buffer, snprintf(buffer, sizeof(buffer), "'\\%d'", (int)c));
    return std::string(buffer, snprintf(buffer, sizeof(buffer), "'%c'", c));
}

class ParserOperator : public Operator
{
public:
    std::string filter;
    size_t position;
    Affinity affinity;
    Priority priority = Priority::UNKNOWN;
    bool isNode;

    ParserOperator(const std::string& filter, size_t position, Affinity affinity, bool isNode) :
        filter(filter), position(position), affinity(affinity), isNode(isNode) {}

    virtual ~ParserOperator() = default;
    Priority getPriority() { return priority; }
    virtual Affinity narrowAffinity(Affinity target) = 0;
};
using Operators = std::list<std::unique_ptr<ParserOperator>>;

class Exception : public std::exception
{
public:
    std::string reason;
    Exception(const std::string& token, size_t position, const std::string& reason_)
    {
        reason = std::format("Invalid token at position {} {}: {}", position, token, reason_);
    }
    Exception(const ParserOperator& op, const std::string& reason) : Exception(op.filter, op.position, reason) {}
    Exception(const ParserOperator& op, Affinity expected, Affinity current)
    {
        reason = std::format("Invalid token at position {} {}: Expecting a '{}' but current operator is a '{}'",
            op.position, op.filter, toString(expected), toString(current));
    }
};

class Parser
{
public:
    std::string filter { };
    Header header { };
    size_t currentPosition { 0 };

    Parser(const std::string& filter, const Header& header) :
        filter(filter), header(header)
    {
        for (auto& headerItem : this->header)
            std::transform(headerItem.name.begin(), headerItem.name.end(), headerItem.name.begin(), tolower);
    }
    void skipSpace()
    {
        while (std::isspace(peek()))
            ++currentPosition;
    }
    char peek() { return filter[currentPosition]; }
    char advance() { return filter[currentPosition++]; }
    std::unique_ptr<ParserOperator> parse(char endToken = EOT);
    std::unique_ptr<ParserOperator> parseSingleToken(Operators& operators);
};

std::unique_ptr<Operator> Operator::parse(const std::string& filter, const Header& header)
{
    auto filterOperator = Parser(filter, header).parse();
    if (filterOperator)
        filterOperator->narrowAffinity(Affinity::BOOLEAN);
    return filterOperator;
}

/*************************************************/
/*                 SECTION TOKEN                 */
/*************************************************/

template<auto O, typename T>
static inline Variant call(const std::array<std::unique_ptr<ParserOperator>, 0>& values, const T& row)
{ return O(); }
template<auto O, typename T>
static inline Variant call(const std::array<std::unique_ptr<ParserOperator>, 1>& values, const T& row)
{ return O(std::get<0>(values)->evaluate(row)); }
template<auto O, typename T>
static inline Variant call(const std::array<std::unique_ptr<ParserOperator>, 2>& values, const T& row)
{ return O(std::get<0>(values)->evaluate(row), std::get<1>(values)->evaluate(row)); }
template<auto O, typename T>
static inline Variant call(const std::array<std::unique_ptr<ParserOperator>, 3>& values, const T& row)
{ return O(std::get<0>(values)->evaluate(row), std::get<1>(values)->evaluate(row), std::get<2>(values)->evaluate(row)); }

class PlaceholderOperator : public ParserOperator
{
public:
    PlaceholderOperator(const std::string& filter, size_t position, Affinity affinity) :
        ParserOperator(filter, position, affinity, false) {}
    virtual void assignValues(std::vector<std::unique_ptr<ParserOperator>>& values) = 0;
};

class Constant : public ParserOperator
{
public:
    Variant value;
    Constant(const std::string& filter, size_t position) :
        ParserOperator(filter, position, Affinity::UNKNOWN, true), value(filter) { }
    Affinity narrowAffinity(Affinity targetAffinity) override
    {
        if (targetAffinity != Affinity::UNKNOWN || affinity == Affinity::UNKNOWN)
            return affinity;
        switch (affinity) {
            case Affinity::DOUBLE:  value = atof(std::get<std::string>(value).c_str());  break;
            case Affinity::BOOLEAN: value = parseBool(std::get<std::string>(value));     break;
            case Affinity::INTEGER: value = atoll(std::get<std::string>(value).c_str()); break;
            case Affinity::STRING:                                                       break;
            default: throw Exception(*this, "Conversion unavailable");
        }
        return affinity;
    }
    Variant evaluate(const Row& row) const override
    { return value; }
    void write_to(std::ostream& stream) const override
    { stream << std::get<std::string>(value); }
private:
    static bool parseBool(const std::string& val)
    {
        if (val.size() == 1) {
            if (val[0] == '0') return false;
            if (val[0] == '1') return true;
            throw std::bad_cast();
        }
        if (val.size() == 4) {
            if (val == "true" || val == "TRUE") return true;
            throw std::bad_cast();
        }
        if (val.size() == 5) {
            if (val == "false" || val == "FALSE") return false;
            throw std::bad_cast();
        }
        throw std::bad_cast();
    }
};

class HeaderValue : public ParserOperator
{
public:
    size_t index;
    HeaderValue(const std::string& filter, size_t position, size_t index, Affinity affinity) :
        ParserOperator(filter, position, affinity, true), index(index) { }
    Affinity narrowAffinity(Affinity targetAffinity) override
    {
        if (targetAffinity != Affinity::UNKNOWN && targetAffinity != affinity)
            throw Exception(*this, targetAffinity, affinity);
        return affinity;
    }
    Variant evaluate(const Row& row) const override
    {
        if (index < row.size())
            return toAffinity(row[index], affinity);
    }
    void write_to(std::ostream& stream) const override
    {
        stream << filter;
    }
};

template<size_t S, auto O, Affinity R, std::array<Affinity, S> P>
class FunctionOperator : public PlaceholderOperator
{
public:
    std::array<std::unique_ptr<ParserOperator>, S> values;
    FunctionOperator(const std::string& filter, size_t position) :
        PlaceholderOperator(filter, position, R) { }
    void assignValues(std::vector<std::unique_ptr<ParserOperator>>& _values) override
    {
        if (_values.size() != S)
            throw Exception(*this, std::format("Invalid number of arguments, got {} expected {}", _values.size(), S));
        for (size_t i = 0; i < S; ++i)
            values[i].swap(_values[i]);
        isNode = true;
    }

    Affinity narrowAffinity(Affinity targetAffinity) override
    {
        // Affinity from return value -> push to parameters
        if (affinity == Affinity::UNKNOWN && targetAffinity != Affinity::UNKNOWN) {
            affinity = targetAffinity;
            for_sequence(std::make_index_sequence<S>{}, [&](auto i) constexpr {
                if constexpr (P[i] == Affinity::UNKNOWN)
                    values[i]->narrowAffinity(affinity);
                else
                    values[i]->narrowAffinity(P[i]);
                });
            return affinity;
        }

        if (targetAffinity != Affinity::UNKNOWN && affinity != targetAffinity)
            throw Exception(*this, targetAffinity, affinity);

        Affinity foundAffinity = Affinity::UNKNOWN;
        for_sequence(std::make_index_sequence<S>{}, [&](auto i) constexpr {
            if (Affinity returned = values[i]->narrowAffinity(P[i]); returned != P[i])
                foundAffinity = returned;
            }); //  (283,648 bytes)

        if (foundAffinity == Affinity::UNKNOWN && affinity != Affinity::UNKNOWN)
            foundAffinity = Affinity::STRING;
        if (foundAffinity != Affinity::UNKNOWN) {
            if (affinity == Affinity::UNKNOWN)
                affinity = foundAffinity;
            for_sequence(std::make_index_sequence<S>{}, [&](auto i) constexpr {
                if constexpr (P[i] == Affinity::UNKNOWN)
                    values[i]->narrowAffinity(foundAffinity);
                });
        }
        return affinity;
    }
    Variant evaluate(const Row& row) const override
    {
        return call<O>(values, row);
    }
    void write_to(std::ostream& stream) const override
    {
        stream << "(";
        if (S == 2 && getTokenClass(filter[0]) == TokenClass::OPERATOR) {
            stream << *values[0] << " " << filter << " " << *values[1];
        }
        else {
            for (size_t i = 0; i < S; ++i) {
                if (i != 0)
                    stream << ", ";
                stream << *values[i];
            }
        }
        stream << ")";
    }
};

/*************************************************/
/*            SECTION FUNCTION PARSER            */
/*************************************************/
#define A_B Affinity::BOOLEAN
#define A_D Affinity::DOUBLE
#define A_S Affinity::STRING
#define A_U Affinity::UNKNOWN

static constexpr std::array<Affinity, 1> AA_B { A_B };
static constexpr std::array<Affinity, 1> AA_D { A_D };
static constexpr std::array<Affinity, 1> AA_S { A_S };
static constexpr std::array<Affinity, 1> AA_U { A_U };
static constexpr std::array<Affinity, 2> AA_BB { A_B, A_B };
static constexpr std::array<Affinity, 2> AA_DD { A_D, A_D };
static constexpr std::array<Affinity, 2> AA_SS { A_S, A_S };
static constexpr std::array<Affinity, 2> AA_UU { A_U, A_U };
static constexpr std::array<Affinity, 3> AA_BUU { A_B, A_U, A_U };

using ParseFunction = std::function<std::unique_ptr<ParserOperator>(Parser&, Operators&, const std::string&)>;

static std::unique_ptr<ParserOperator> parseQuote(Parser& parser, Operators&, const std::string&)
{
    size_t initialPosition = parser.currentPosition;
    std::string token;
    token.push_back(parser.advance());
    while (token.back() != '"' && token.back() != EOT) {
        token.push_back(parser.advance());
    }
    if (token.back() == EOT)
        throw Exception(token, initialPosition, "Unexpected end of QuotedString");
    token.pop_back();

    return std::make_unique<Constant>(token, initialPosition);
}

static std::unique_ptr<ParserOperator> parseFreeText(Parser& parser, Operators&, const std::string& freeText)
{
    size_t initialPosition = parser.currentPosition - freeText.size();
    std::string name = freeText;
    std::transform(name.begin(), name.end(), name.begin(), tolower);
    size_t headerPosition = 0;
    for (; headerPosition < parser.header.size(); ++headerPosition) {
        if (name == parser.header[headerPosition].name)
            return std::make_unique<HeaderValue>(freeText, initialPosition, headerPosition, parser.header[headerPosition].affinity);
    }
    return std::make_unique<Constant>(freeText, initialPosition);
}

static std::unique_ptr<ParserOperator> parseParenthesis(Parser& parser, Operators&, const std::string&)
{
    return parser.parse(')');
}

template<class C, Priority P>
static std::unique_ptr<ParserOperator> parsePlaceholder(Parser& parser, Operators&, const std::string& currentToken)
{
    auto ret = std::make_unique<C>(currentToken, parser.currentPosition - currentToken.size());
    ret->priority = P;
    return ret;
}

template<size_t S, auto O, Affinity R, std::array<Affinity, S> P, bool ExpectParenthesis=true>
static std::unique_ptr<ParserOperator> parseFunction(Parser& parser, Operators&, const std::string& currentToken)
{
    size_t initialPosition = parser.currentPosition - currentToken.size();
    std::vector<std::unique_ptr<ParserOperator>> operators;
    parser.skipSpace();
    if constexpr (ExpectParenthesis) {
        if (parser.peek() != '(')
            throw Exception(currentToken, initialPosition, "Expecting a '(' and current token is a " + charToVisible(parser.peek()));
        parser.advance();
    }

    if constexpr (S == 0) {
        if constexpr (ExpectParenthesis) {
            parser.skipSpace();
            if (parser.peek() != ')')
                throw Exception(currentToken, initialPosition, "Expecting a ')' and current token is a " + charToVisible(parser.peek()));
            parser.advance();
        }
    }
    else {
        for (size_t i = 0; i < S - 1; ++i) {
            operators.push_back(parser.parse(','));
        }
        if constexpr (ExpectParenthesis) {
            operators.push_back(parser.parse(')'));
        }
        else {
            Operators rhsOperators;
            operators.push_back(parser.parseSingleToken(rhsOperators));
        }
        auto functionOperator = std::make_unique<FunctionOperator<S, O, R, P>>(currentToken, initialPosition);
        functionOperator->assignValues(operators);
        return functionOperator;
    }
}

template<auto O, Affinity R, std::array<Affinity, 2> P>
static std::unique_ptr<ParserOperator> parseBinaryOperator(Parser& parser, Operators& previousOperators, const std::string& currentToken)
{
    size_t initialPosition = parser.currentPosition - currentToken.size();
    if (previousOperators.empty() || previousOperators.back())
        throw Exception(currentToken, initialPosition, "Left hand side of operator is empty");
    parser.skipSpace();
    std::vector<std::unique_ptr<ParserOperator>> operators(1);
    operators[0].swap(previousOperators.back());
    previousOperators.pop_back();
    Operators rhsOperators;
    operators.push_back(parser.parseSingleToken(rhsOperators));
    if (!operators.back())
        throw Exception(currentToken, initialPosition, "Right hand side of operator is empty");
    auto functionOperator = std::make_unique<FunctionOperator<2, O, R, P>>(currentToken, initialPosition);
    functionOperator->assignValues(operators);
    return functionOperator;
}

template<auto UO, Affinity UR, std::array<Affinity, 1> UP, auto BO, Affinity BR, std::array<Affinity, 2> BP>
static std::unique_ptr<ParserOperator> parseUnaryOrBinaryOperator(Parser& parser, Operators& previousOperators, const std::string& currentToken)
{
    if (previousOperators.empty())
        return parseFunction<1, UO, UR, UP, false>(parser, previousOperators, currentToken);
    else
        return parseBinaryOperator<BO, BR, BP>(parser, previousOperators, currentToken);
}

static Variant variantNot(const Variant& v) { return !std::get<bool>(v); }
static Variant variantOr(const Variant& l, const Variant& r) { return std::get<bool>(l) || std::get<bool>(r); }
static Variant variantAnd(const Variant& l, const Variant& r) { return std::get<bool>(l) && std::get<bool>(r); }
static Variant variantEqual(const Variant& l, const Variant& r) { return l == r; }
static Variant variantDifferent(const Variant& l, const Variant& r) { return l != r; }
static Variant variantSuperior(const Variant& l, const Variant& r) { return l > r; }
static Variant variantInferior(const Variant& l, const Variant& r) { return l < r; }
static Variant variantSuperiorEqual(const Variant& l, const Variant& r) { return l >= r; }
static Variant variantInferiorEqual(const Variant& l, const Variant& r) { return l <= r; }
static Variant variantPlus(const Variant& l, const Variant& r) { return std::get<double>(l) + std::get<double>(r); }
static Variant variantMinus(const Variant& l, const Variant& r) { return std::get<double>(l) - std::get<double>(r); }
static Variant variantUnaryPlus(const Variant& v) { return std::get<double>(v); }
static Variant variantUnaryMinus(const Variant& v) { return -std::get<double>(v); }
static Variant variantMultiply(const Variant& l, const Variant& r) { return std::get<double>(l) * std::get<double>(r); }
static Variant variantDivide(const Variant& l, const Variant& r)
{
    if (double rd = std::get<double>(r); rd != 0)
        return std::get<double>(l) / rd;
    return 0.0;
}
static Variant variantContains(const Variant& a1, const Variant& a2)
{
    return std::get<std::string>(a1).find(std::get<std::string>(a2)) != std::string::npos;
}
static Variant variantStartsWith(const Variant& a1, const Variant& a2)
{
    return std::get<std::string>(a1).rfind(std::get<std::string>(a2), 0) != std::string::npos;
}
static Variant variantMin(const Variant& l, const Variant& r) { return l < r ? l : r; }
static Variant variantMax(const Variant& l, const Variant& r) { return l > r ? l : r; }
static Variant variantUc(const Variant& v)
{
    std::string val = std::get<std::string>(v);
    std::transform(val.begin(), val.end(), val.begin(), toupper);
    return val;
}
static Variant variantLc(const Variant& v)
{
    std::string val = std::get<std::string>(v);
    std::transform(val.begin(), val.end(), val.begin(), tolower);
    return val;
}
static Variant variantLog(const Variant& v)
{
    if (double vd = std::get<double>(v); vd > 0)
        return log(vd);
    return 0.0;
}
static Variant variantExp(const Variant& v)
{
    if (double vd = std::get<double>(v); vd != 0)
        return exp(vd);
    return 0.0;
}
static Variant variantAbs(const Variant& v)
{
    return abs(std::get<double>(v));
}
static Variant variantIf(const Variant& cond, const Variant& trueCond, const Variant& falseCond)
{
    return std::get<bool>(cond) ? trueCond : falseCond;
}

static ParseFunction parseNot = parseFunction<1, variantNot, A_B, AA_B, false>;
static ParseFunction parseOr = parsePlaceholder<FunctionOperator<2, variantOr, A_B, AA_BB>, Priority::OR>;
static ParseFunction parseAnd = parsePlaceholder<FunctionOperator<2, variantAnd, A_B, AA_BB>, Priority::AND>;
static ParseFunction parseEqual = parseBinaryOperator<variantEqual, A_B, AA_UU>;
static ParseFunction parseDifferent = parseBinaryOperator<variantDifferent, A_B, AA_UU>;
static ParseFunction parseSuperior = parseBinaryOperator<variantSuperior, A_B, AA_UU>;
static ParseFunction parseInferior = parseBinaryOperator<variantInferior, A_B, AA_UU>;
static ParseFunction parseSuperiorEqual = parseBinaryOperator<variantSuperiorEqual, A_B, AA_UU>;
static ParseFunction parseInferiorEqual = parseBinaryOperator<variantInferiorEqual, A_B, AA_UU>;
static ParseFunction parsePlus = parseUnaryOrBinaryOperator<variantUnaryPlus, A_D, AA_D, variantPlus, A_D, AA_DD>;
static ParseFunction parseMinus = parseUnaryOrBinaryOperator<variantUnaryMinus, A_D, AA_D, variantMinus, A_D, AA_DD>;
static ParseFunction parseMultiply = parseBinaryOperator<variantMultiply, A_D, AA_DD>;
static ParseFunction parseDivide = parseBinaryOperator<variantDivide, A_D, AA_DD>;
static ParseFunction parseContains = parseFunction<2, variantContains, A_B, AA_SS>;
static ParseFunction parseStartsWith = parseFunction<2, variantStartsWith, A_B, AA_SS>;
static ParseFunction parseMin = parseFunction<2, variantMin, A_U, AA_UU>;
static ParseFunction parseMax = parseFunction<2, variantMax, A_U, AA_UU>;
static ParseFunction parseUc = parseFunction<1, variantUc, A_S, AA_S>;
static ParseFunction parseLc = parseFunction<1, variantLc, A_S, AA_S>;
static ParseFunction parseLog = parseFunction<1, variantLog, A_D, AA_D>;
static ParseFunction parseExp = parseFunction<1, variantExp, A_D, AA_D>;
static ParseFunction parseAbs = parseFunction<1, variantLog, A_D, AA_D>;
static ParseFunction parseIf = parseFunction<3, variantIf, A_U, AA_BUU>;

static std::unordered_map<std::string, ParseFunction> functionMap{
    { "\"", parseQuote },
    { "(", parseParenthesis },
    { "!", parseNot },
    { "not", parseNot },
    { "|", parseOr },
    { "||", parseOr },
    { "or", parseOr },
    { "&", parseAnd },
    { "&&", parseAnd },
    { "and", parseAnd },
    { "=", parseEqual },
    { "==", parseEqual },
    { "!=", parseDifferent },
    { ">", parseSuperior },
    { "<", parseInferior },
    { ">=", parseSuperiorEqual },
    { "<=", parseInferiorEqual },
    { "+", parsePlus },
    { "-", parseMinus },
    { "*", parseMultiply },
    { "/", parseDivide },
    { "contains", parseContains },
    { "startswith", parseStartsWith },
    { "min", parseMin },
    { "max", parseMax },
    { "uc", parseUc },
    { "lc", parseLc },
    { "log", parseLog },
    { "exp", parseExp },
    { "abs", parseAbs },
    { "if", parseIf },
};

std::unique_ptr<ParserOperator> Parser::parseSingleToken(Operators& operators)
{
    TokenClass currentTokenClass = getTokenClass(peek());
    if (currentTokenClass == TokenClass::END)
        return nullptr;

    size_t initialPosition = currentPosition;
    std::string token;
    token.push_back(advance());
    if (currentTokenClass != TokenClass::SINGLE) {
        while (currentTokenClass == getTokenClass(peek()))
            token.push_back(advance());
    }

    std::string lowerToken = token;
    std::transform(lowerToken.begin(), lowerToken.end(), lowerToken.begin(), tolower);

    if (currentTokenClass == TokenClass::TEXT)
        return parseFreeText(*this, operators, token);
    else if (auto it = functionMap.find(lowerToken); it != functionMap.end())
        return it->second(*this, operators, token);
    else
        throw Exception(token, initialPosition, "Unexpected token class unmatched");
}

std::unique_ptr<ParserOperator> Parser::parse(char endToken)
{
    Operators operators;
    skipSpace();

    while (getTokenClass(peek()) != TokenClass::END) {
        operators.push_back(parseSingleToken(operators));
        skipSpace();
    }
    if (peek() != endToken) {
        throw Exception(charToVisible(peek()), currentPosition,
            std::format("Expecting {} and current token is {}", charToVisible(endToken), charToVisible(peek())));
    }
    advance();

    for (size_t p = 0; static_cast<Priority>(p) < Priority::UNKNOWN; ++p) {
        for (auto it = operators.begin(); it != operators.end(); ++it) {
            if (!it->get())
                throw Exception("", 0, "Encountered an empty operator during parsing");
            if (it->get()->getPriority() != static_cast<Priority>(p) || !it->get()->isNode)
                continue;
            
            auto* binaryOp = dynamic_cast<PlaceholderOperator*>(it->get());
            if (!binaryOp)
                throw Exception(*it->get(), "Operator is not a binary operator");
            if (it == operators.begin())
                throw Exception(*it->get(), "Left hand side of operator is empty");
            auto lhs = std::prev(it);
            auto rhs = std::next(it);
            if (rhs == operators.end())
                throw Exception(*it->get(), "Right hand side of operator is empty");
            if (!lhs->get()->isNode)
                throw Exception(*it->get(), "Left hand side of operator is not fully resolved");
            if (!rhs->get()->isNode)
                throw Exception(*it->get(), "Right hand side of operator is not fully resolved");
            std::vector<std::unique_ptr<ParserOperator>> args(2);
            args[0].swap(*lhs);
            operators.erase(lhs);
            args[1].swap(*rhs);
            operators.erase(rhs);
            binaryOp->assignValues(args);
        }
    }

    if (operators.size() > 1)
        throw Exception(*operators.front(), "Another operator has been detected after parsing");
    return (operators.size() == 0 ? std::unique_ptr<ParserOperator>() : std::move(operators.front()));
}
} /* !namespace parser */