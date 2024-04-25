#include <variant>
#include <sstream>
#include <memory>
#include <vector>

namespace parser {

using Variant = std::variant<std::monostate, std::string, int64_t, double, bool>;

using Row = std::vector<Variant>;

enum class Affinity
{
    UNKNOWN,
    BOOLEAN,
    INTEGER,
    DOUBLE,
    STRING,
};
struct HeaderItem {
    std::string name;
    Affinity affinity;
};
using Header = std::vector<HeaderItem>;

class Operator
{
public:
    virtual ~Operator() = default;
    virtual Variant evaluate(const Row& row) const = 0;

    static std::unique_ptr<Operator> parse(const std::string& filter, const Header& header);

    std::string toString() const
    {
        std::ostringstream ss;
        ss << *this;
        return ss.str();
    }
    friend std::ostream& operator<<(std::ostream& lhs, const Operator& rhs)
    {
        rhs.write_to(lhs);
        return lhs;
    }
protected:
    virtual void write_to(std::ostream& stream) const = 0;
};

class TrueOperator : public Operator
{
public:
    Variant evaluate(const Row& row) const override
    {
        return Variant(true);
    }
protected:
    void write_to(std::ostream& stream) const
    {
        stream << "True";
    }
};

}