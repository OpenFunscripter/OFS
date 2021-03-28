 #include <ctime>
 #include <string>
 #include <iomanip>
 #include <sstream>
 #include <cmath>
 #include <type_traits>
 #include <cstdint>
namespace asap {
  static constexpr uint64_t SECONDS_IN_MINUTE = 60;
  static constexpr uint64_t SECONDS_IN_HOUR = SECONDS_IN_MINUTE * 60;
  static constexpr uint64_t SECONDS_IN_DAY = SECONDS_IN_HOUR * 24;
  static constexpr uint64_t SECONDS_IN_MONTH = SECONDS_IN_DAY * 30;
  static constexpr uint64_t SECONDS_IN_YEAR = SECONDS_IN_DAY * 365;
  static constexpr uint64_t SECONDS_IN_WEEK = SECONDS_IN_DAY * 7;
}
namespace asap {
   template<uint64_t convert = 1>
    class duration {
      public:
        explicit duration(double v = 0);
        template<uint64_t convertfrom> duration(const asap::duration<convertfrom> & other);
        explicit operator double() const;
        duration<convert> & operator=(double v);
        double operator*() const;
        duration<convert> operator-();
        template<uint64_t other> operator duration<other>() const;
        template<uint64_t convert2> duration<convert> & operator+=(const duration<convert2> & other);
        duration<convert> & operator+=(const duration<convert> & other);
        template<uint64_t convert2> duration<convert> & operator-=(const duration<convert2> & other);
        template<uint64_t convert2> duration<convert> & operator=(const duration<convert2> & other);
        std::string str() const;
      private:
        double value;
    };
  using seconds = duration<1>; using second = seconds;
  using minutes = duration<SECONDS_IN_MINUTE>; using minute = minutes;
  using hours = duration<SECONDS_IN_HOUR>; using hour = hours;
  using days = duration<SECONDS_IN_DAY>; using day = days;
  using weeks = duration<SECONDS_IN_WEEK>; using week = weeks;
  using months = duration<SECONDS_IN_MONTH>; using month = months;
  using years = duration<SECONDS_IN_YEAR>; using year = years;
}
namespace asap {
  class period;
  class datetime {
    public:
      explicit datetime(time_t time = std::time(nullptr)) noexcept;
      explicit datetime(const std::string & datetime, const std::string & format = "%x %X", const std::string & locale = "");
      datetime(uint32_t year, uint32_t month, uint32_t day, uint32_t hours = 0, uint32_t minutes = 0, uint32_t seconds = 0);
      datetime & operator+=(const seconds & d);
      datetime & operator+=(const minutes & d);
      datetime & operator+=(const hours & d);
      datetime & operator+=(const days & d);
      datetime & operator+=(const weeks & d);
      datetime & operator+=(const months & d);
      datetime & operator+=(const years & d);
      asap::seconds operator-(const datetime & other) const;
      template<uint64_t convert> asap::datetime & operator-=(const asap::duration<convert> & c);
      asap::datetime & operator+=(time_t stamp);
      asap::datetime & operator-=(time_t stamp);
      asap::datetime morning();
      asap::datetime afternoon();
      asap::datetime midnight();
      time_t timestamp() const;
      std::string str(const std::string & fmt = "%x %X") const;
      asap::period until(const asap::datetime & dt) const;
      int second() const;
      asap::datetime & second(int value);
      int minute() const;
      asap::datetime & minute(int value);
      int hour() const;
      asap::datetime & hour(int value);
      int wday() const;
      asap::datetime & wday(int value);
      int mday() const;
      asap::datetime & mday(int value);
      int yday() const;
      asap::datetime & yday(int value);
      int month() const;
      asap::datetime & month(int value);
      int year() const;
      asap::datetime & year(int value);
    private:
      void add(long seconds);
      std::tm when;
  };
}
namespace asap {
  template<uint64_t convert>
  inline static asap::datetime operator+(const asap::datetime & a, const duration<convert> & b) {
    asap::datetime r = a;
    r += b;
    return r;
  }
  template<uint64_t convert>
  inline static asap::datetime operator-(const asap::datetime & a, const duration<convert> & b) {
    asap::datetime r = a;
    r -= b;
    return r;
  }
  inline static asap::datetime operator+(const asap::datetime & a, std::time_t b) {
    asap::datetime r = a;
    r += b;
    return r;
  }
  inline static asap::datetime operator-(const asap::datetime & a, std::time_t b) {
    asap::datetime r = a;
    r -= b;
    return r;
  }
  template<uint64_t c1, uint64_t c2>
  inline static duration<c1> operator+(const duration<c1> & a, const duration<c2> & b) {
    duration<c1> r = a;
    r += b;
    return r;
  };
  template<uint64_t conv>
  inline static duration<conv> operator+(const duration<conv> & a, const duration<conv> & b) {
    duration<conv> r = a;
    r += b;
    return r;
  };
  template<uint64_t c1, uint64_t c2>
  inline static duration<c1> operator-(const duration<c1> & a, const duration<c2> & b) {
    duration<c1> r = a;
    r -= b;
    return r;
  };
  template<uint64_t c1>
  inline static duration<c1> operator-(const duration<c1> & a) {
    duration<c1> r;
    r -= a;
    return r;
  };
  inline static bool operator<(const asap::datetime & a, const asap::datetime & b) {
    return a.timestamp() < b.timestamp();
  }
  inline static bool operator>(const asap::datetime & a, const asap::datetime & b) {
    return a.timestamp() > b.timestamp();
  }
  inline static bool operator==(const asap::datetime & a, const asap::datetime & b) {
    return a.timestamp() == b.timestamp();
  }
  template<typename ostream>
  inline static ostream & operator<<(ostream & os, const asap::datetime & dt) {
    return os << dt.str(), os;
  }
  template<typename ostream, uint64_t convert>
  inline static ostream & operator<<(ostream & os, const duration<convert> & du) {
    return os << du.str(), os;
  }
}
namespace asap {
  class period;
  namespace detail { template <uint64_t T> class accessor; }
  class period {
    public:
      explicit period(const asap::datetime & a = asap::datetime(), const asap::datetime & b = asap::datetime());
      const asap::datetime & from() const;
      void from(const asap::datetime & begin);
      const asap::datetime & to() const;
      void to(const asap::datetime & end);
      template<typename T> T difference() const;
      asap::seconds difference() const;
      template<uint64_t stepconv>
      asap::detail::accessor<stepconv> every(const asap::duration<stepconv> & d) const;
    private:
      asap::datetime begin_;
      asap::datetime end_;
  };
}
namespace asap {
  namespace literals {
    inline static asap::years operator"" _years(long double v) { return asap::years(static_cast<double>(v)); }
    inline static asap::year operator"" _year(long double v) { return asap::year(static_cast<double>(v)); }
    inline static asap::years operator"" _yrs(long double v) { return asap::years(static_cast<double>(v)); }
    inline static asap::years operator"" _Y(long double v) { return asap::years(static_cast<double>(v)); }
    inline static asap::years operator"" _years(unsigned long long v) { return asap::years(static_cast<double>(v)); }
    inline static asap::year operator"" _year(unsigned long long v) { return asap::year(static_cast<double>(v)); }
    inline static asap::years operator"" _yrs(unsigned long long v) { return asap::years(static_cast<double>(v)); }
    inline static asap::years operator"" _Y(unsigned long long v) { return asap::years(static_cast<double>(v)); }
    inline static asap::months operator"" _months(long double v) { return asap::months(static_cast<double>(v)); }
    inline static asap::month operator"" _month(long double v) { return asap::month(static_cast<double>(v)); }
    inline static asap::months operator"" _mon(long double v) { return asap::month(static_cast<double>(v)); }
    inline static asap::months operator"" _m(long double v) { return asap::month(static_cast<double>(v)); }
    inline static asap::months operator"" _months(unsigned long long v) { return asap::months(static_cast<double>(v)); }
    inline static asap::month operator"" _month(unsigned long long v) { return asap::month(static_cast<double>(v)); }
    inline static asap::months operator"" _mon(unsigned long long v) { return asap::month(static_cast<double>(v)); }
    inline static asap::months operator"" _m(unsigned long long v) { return asap::month(static_cast<double>(v)); }
    inline static asap::days operator"" _days(long double v) { return asap::days(static_cast<double>(v)); }
    inline static asap::day operator"" _day(long double v) { return asap::day(static_cast<double>(v)); }
    inline static asap::days operator"" _d(long double v) { return asap::day(static_cast<double>(v)); }
    inline static asap::days operator"" _days(unsigned long long v) { return asap::days(static_cast<double>(v)); }
    inline static asap::day operator"" _day(unsigned long long v) { return asap::day(static_cast<double>(v)); }
    inline static asap::days operator"" _d(unsigned long long v) { return asap::day(static_cast<double>(v)); }
    inline static asap::hours operator"" _hours(long double v) { return asap::hours(static_cast<double>(v)); }
    inline static asap::hour operator"" _hour(long double v) { return asap::hour(static_cast<double>(v)); }
    inline static asap::hours operator"" _hrs(long double v) { return asap::hours(static_cast<double>(v)); }
    inline static asap::hours operator"" _H(long double v) { return asap::hours(static_cast<double>(v)); }
    inline static asap::hours operator"" _hours(unsigned long long v) { return asap::hours(static_cast<double>(v)); }
    inline static asap::hour operator"" _hour(unsigned long long v) { return asap::hour(static_cast<double>(v)); }
    inline static asap::hours operator"" _hrs(unsigned long long v) { return asap::hours(static_cast<double>(v)); }
    inline static asap::hours operator"" _H(unsigned long long v) { return asap::hours(static_cast<double>(v)); }
    inline static asap::minutes operator"" _minutes(long double v) { return asap::minutes(static_cast<double>(v)); }
    inline static asap::minute operator"" _minute(long double v) { return asap::minute(static_cast<double>(v)); }
    inline static asap::minute operator"" _min(long double v) { return asap::minute(static_cast<double>(v)); }
    inline static asap::minutes operator"" _M(long double v) { return asap::minute(static_cast<double>(v)); }
    inline static asap::minutes operator"" _minutes(unsigned long long v) { return asap::minutes(static_cast<double>(v)); }
    inline static asap::minute operator"" _minute(unsigned long long v) { return asap::minute(static_cast<double>(v)); }
    inline static asap::minute operator"" _min(unsigned long long v) { return asap::minute(static_cast<double>(v)); }
    inline static asap::minutes operator"" _M(unsigned long long v) { return asap::minute(static_cast<double>(v)); }
    inline static asap::seconds operator"" _seconds(long double v) { return asap::seconds(static_cast<double>(v)); }
    inline static asap::second operator"" _second(long double v) { return asap::second(static_cast<double>(v)); }
    inline static asap::seconds operator"" _sec(long double v) { return asap::seconds(static_cast<double>(v)); }
    inline static asap::seconds operator"" _S(long double v) { return asap::seconds(static_cast<double>(v)); }
    inline static asap::seconds operator"" _seconds(unsigned long long v) { return asap::seconds(static_cast<double>(v)); }
    inline static asap::second operator"" _second(unsigned long long v) { return asap::second(static_cast<double>(v)); }
    inline static asap::seconds operator"" _sec(unsigned long long v) { return asap::seconds(static_cast<double>(v)); }
    inline static asap::seconds operator"" _S(unsigned long long v) { return asap::seconds(static_cast<double>(v)); }
  }
}
namespace asap {
  static inline asap::datetime now() { return datetime{}; }
  static inline asap::datetime tomorrow() {
    auto n = asap::now();
    n += asap::days(1);
    n.hour(0);
    n.minute(0);
    n.second(0);
    return n;
  }
  static inline asap::datetime yesterday() {
      auto n = asap::now();
      n -= asap::days(1);
      n.hour(0);
      n.minute(0);
      n.second(0);
      return n;
  }
}
namespace asap {
  inline asap::datetime::datetime(time_t time) noexcept : when{} {
    when = *(std::localtime(&time));
  }
  inline asap::datetime::datetime(const std::string & datetime, const std::string & format, const std::string & locale) : when{} {
    static std::array<std::string, 7> fmts = {
        format,
        "%x %X",
        "%Y-%m-%dT%H:%M:%S",
        "%d/%m/%Y %H:%M:%S",
        "%H:%M:%S",
        "%d/%m/%Y",
        "%c"
    };
    for (std::string & fmt : fmts) {
      when = {0};
      std::stringstream ss(datetime);
      ss.imbue(std::locale(""));
      ss >> std::get_time(&when, fmt.c_str());
      if (str(fmt) == datetime) break;
    }
  }
  inline time_t asap::datetime::timestamp() const {
    std::tm temp = when;
    return mktime(&temp);
  }
  inline std::string asap::datetime::str(const std::string & fmt) const {
    char data[256];
    std::strftime(data, sizeof(data), fmt.c_str(), &when);
    return std::string(data);
  }
  inline asap::datetime & asap::datetime::operator+=(time_t stamp) {
    add(static_cast<long>(stamp));
    return *this;
  }
  inline asap::datetime & asap::datetime::operator-=(time_t stamp) {
    add(-static_cast<long>(stamp));
    return *this;
  }
  inline void asap::datetime::add(long seconds) {
    std::time_t time = std::mktime(&when);
    time += seconds;
    when = *(std::localtime(&time));
  }
  inline asap::datetime::datetime(uint32_t year, uint32_t month, uint32_t day, uint32_t hours, uint32_t minutes, uint32_t seconds)
      : datetime() {
    when.tm_year = year - 1900;
    when.tm_mon = month;
    when.tm_mday = day;
    when.tm_hour = hours;
    when.tm_min = minutes;
    when.tm_sec = seconds;
  }
  inline asap::datetime & asap::datetime::operator+=(const asap::seconds & d) {
    when.tm_sec += *d;
    mktime(&when);
    return *this;
  }
  inline asap::seconds asap::datetime::operator-(const datetime & other) const {
    const std::tm & a = when, b = other.when;
    uint64_t r = (a.tm_year - b.tm_year) * SECONDS_IN_YEAR +
                 (a.tm_mon - b.tm_mon) * SECONDS_IN_MONTH +
                 (a.tm_mday - b.tm_mday) * SECONDS_IN_DAY +
                 (a.tm_hour - b.tm_hour) * SECONDS_IN_HOUR +
                 (a.tm_min - b.tm_min) * SECONDS_IN_MINUTE +
                 (a.tm_sec - b.tm_sec);
    return asap::seconds(r);
  }
  inline asap::datetime & asap::datetime::operator+=(const asap::minutes & d) {
    when.tm_min += *d;
    return *this += asap::seconds((*d - std::floor(*d)) * SECONDS_IN_MINUTE);
  }
  inline asap::datetime & asap::datetime::operator+=(const hours & d) {
    when.tm_hour += *d;
    return *this += asap::minutes((*d - std::floor(*d)) * (SECONDS_IN_HOUR / SECONDS_IN_MINUTE));
  }
  inline asap::datetime & asap::datetime::operator+=(const asap::days & d) {
    when.tm_mday += *d;
    return *this += asap::hours((*d - std::floor(*d)) * (SECONDS_IN_DAY / SECONDS_IN_HOUR));
  }
  inline asap::datetime & asap::datetime::operator+=(const asap::weeks & d) {
    return *this += asap::days(*d * 7);
  }
  inline asap::datetime & asap::datetime::operator+=(const asap::months & d) {
    when.tm_mon += *d;
    return *this += asap::days((*d - std::floor(*d)) * (SECONDS_IN_MONTH / SECONDS_IN_DAY));
  }
  inline asap::datetime & asap::datetime::operator+=(const asap::years & d) {
    when.tm_year += *d;
    return *this += asap::months((*d - std::floor(*d)) * (SECONDS_IN_YEAR / SECONDS_IN_MONTH));
  }
  template<uint64_t convert>
  inline asap::datetime & asap::datetime::operator-=(const asap::duration<convert> & c) {
    return *this += -c;
  }
  inline int datetime::second() const { return when.tm_sec; }
  inline asap::datetime & datetime::second(int value) { when.tm_sec = value; std::mktime(&when); return *this; }
  inline int datetime::minute() const { return when.tm_min; }
  inline asap::datetime & datetime::minute(int value) { when.tm_min = value; std::mktime(&when); return *this; }
  inline int datetime::hour() const { return when.tm_hour; }
  inline asap::datetime & datetime::hour(int value) { when.tm_hour = value; std::mktime(&when); return *this; }
  inline int datetime::wday() const { return when.tm_wday; }
  inline asap::datetime & datetime::wday(int value) { when.tm_wday = value; std::mktime(&when); return *this; }
  inline int datetime::mday() const { return when.tm_mday; }
  inline asap::datetime & datetime::mday(int value) { when.tm_mday = value; std::mktime(&when); return *this; }
  inline int datetime::yday() const { return when.tm_yday; }
  inline asap::datetime & datetime::yday(int value) { when.tm_yday = value; std::mktime(&when); return *this; }
  inline int datetime::month() const { return when.tm_mon; }
  inline asap::datetime & datetime::month(int value) { when.tm_mon = value; std::mktime(&when); return *this; }
  inline int datetime::year() const { return when.tm_year + 1900; }
  inline asap::datetime & datetime::year(int value) { when.tm_year = value - 1900; std::mktime(&when); return *this; }
  inline asap::period datetime::until(const asap::datetime & dt) const {
    return asap::period(*this, dt);
  }
  inline asap::datetime asap::datetime::morning() {
    auto r = *this;
    r.hour(8); r.minute(0); r.second(0);
    return r;
  }
  inline asap::datetime datetime::afternoon() {
    auto r = *this;
    r.hour(12); r.minute(0); r.second(0);
    return r;
  }
  inline asap::datetime datetime::midnight() {
    auto r = *this;
    r.hour(0); r.minute(0); r.second(0);
    return r;
  }
}
namespace asap {
  namespace detail {
    inline static int append(std::string & str, int seconds, int count, const std::string & singular,
                             const std::string & plural = "") {
      long r = static_cast<unsigned>(seconds) / count;
      if (!r) return seconds;
      if (!str.empty() && str[str.length() -1] != '-') str += ", ";
      str += std::to_string(r) + " ";
      if (r < 2) str += singular;
      else str += plural.empty() ? singular + 's' : plural;
      return static_cast<unsigned>(seconds) % count;
    }
  }
  template <uint64_t convert> inline duration<convert>::duration(double v) : value(v) { }
  template <uint64_t convert> inline duration<convert>::operator double() const { return value; }
  template <uint64_t convert> inline duration<convert> & duration<convert>::operator=(double v) { value = v; return *this; }
  template <uint64_t convert> inline double duration<convert>::operator*() const { return value; }
  template <uint64_t convert> inline duration<convert> duration<convert>::operator-() { return duration<convert>(-value); }
  template <uint64_t convert> template <uint64_t other> inline duration<convert>::operator duration<other>() const {
    double asseconds = value * convert;
    return duration<other>(asseconds / other);
  }
  template <uint64_t convert> inline duration<convert> & duration<convert>::operator+=(const duration<convert> & other) {
    value = (value + *other);
    return *this;
  }
  template <uint64_t convert> template <uint64_t convert2> inline duration<convert> & duration<convert>::operator-=(const duration<convert2> & other) {
    value = ((value * convert) - (*other * convert2)) / convert;
    return *this;
  }
  template <uint64_t convert> template <uint64_t convert2> inline duration<convert> & duration<convert>::operator=(const duration<convert2> & other) {
    value = (*other * convert2) / convert;
    return *this;
  }
  template <uint64_t convert> inline std::string duration<convert>::str() const {
    auto seconds = static_cast<int>(std::fabs(value) * convert);
    std::string str;
    if (value < 0) str = "-";
    str.reserve(100);
    seconds = detail::append(str, seconds, SECONDS_IN_YEAR, "year");
    seconds = detail::append(str, seconds, SECONDS_IN_MONTH, "month");
    seconds = detail::append(str, seconds, SECONDS_IN_WEEK, "week");
    seconds = detail::append(str, seconds, SECONDS_IN_DAY, "day");
    seconds = detail::append(str, seconds, SECONDS_IN_HOUR, "hour");
    seconds = detail::append(str, seconds, SECONDS_IN_MINUTE, "minute");
    detail::append(str, seconds, 1, "second");
    return str;
  }
  template <uint64_t convert>
  template <uint64_t convertfrom>
  duration<convert>::duration(const asap::duration<convertfrom> & other)
    : value{(*other * convertfrom) / convert}
  { }
  template <uint64_t convert> template <uint64_t convert2> inline duration<convert> & duration<convert>::operator+=(const duration<convert2> & other) {
    value = ((value * convert) + (*other * convert2)) / convert;
    return *this;
  }
}
namespace asap {
  namespace detail {
    template<uint64_t stepconv>
    class accessor {
      private:
        using step_t = asap::duration<stepconv>;
      public:
        struct iterator {
          asap::datetime now;
          asap::datetime begin;
          asap::datetime end;
          step_t step;
          iterator(const asap::datetime & now,
                   const asap::datetime & begin,
                   const asap::datetime & end,
                   const step_t & step);
          const asap::datetime & operator++();
          asap::datetime operator++(int);
          const asap::datetime & operator*();
          bool operator==(const iterator & other);
          bool operator!=(const iterator & other);
        };
        accessor::iterator begin() const;
        accessor::iterator end() const;
      private:
        accessor(const asap::period & range, step_t step);
        const asap::datetime & from() const;
        const asap::datetime & to() const;
      private:
        const asap::period & range;
        step_t step;
        friend class accessor::iterator;
        friend class asap::period;
    };
  }
  template <uint64_t stepconv>
  inline asap::detail::accessor<stepconv>::iterator::iterator(const asap::datetime & now, const asap::datetime & begin,
                                                       const asap::datetime & end,
                                                       const asap::detail::accessor<stepconv>::step_t & step)
      : now(now), begin(begin), end(end), step(step) { }
  template <uint64_t stepconv>
  inline const asap::datetime & detail::accessor<stepconv>::iterator::operator++() {
    now += step;
    if (now > end) now = end;
    return now;
  }
  template <uint64_t stepconv>
  inline asap::datetime detail::accessor<stepconv>::iterator::operator++(int) {
    datetime prev = now;
    now += step;
    if (now > end) now = end;
    return prev;
  }
  template <uint64_t stepconv>
  inline const asap::datetime & detail::accessor<stepconv>::iterator::operator*() { return now; }
  template <uint64_t stepconv>
  inline bool detail::accessor<stepconv>::iterator::operator==(const detail::accessor<stepconv>::iterator & other) {
    return other.now == now;
  }
  template <uint64_t stepconv>
  inline bool detail::accessor<stepconv>::iterator::operator!=(const detail::accessor<stepconv>::iterator & other) {
    return !(other.now == now);
  }
  template<uint64_t stepconv>
  inline const asap::datetime & detail::accessor<stepconv>::from() const { return range.from(); }
  template<uint64_t stepconv>
  inline const asap::datetime & detail::accessor<stepconv>::to() const { return range.to(); }
  template <uint64_t stepconv>
  inline typename detail::accessor<stepconv>::iterator detail::accessor<stepconv>::begin() const {
    if (from() > to()) return {to(), to(), from() - 1, step};
    else return {from(), from(), to() + 1, step};
  }
  template <uint64_t stepconv>
  inline typename detail::accessor<stepconv>::iterator detail::accessor<stepconv>::end() const {
    if (from() > to()) return {from() - 1, to(), from() - 1, step};
    else return {to() + 1, from(), to() + 1, step};
  }
  template <uint64_t stepconv>
  inline detail::accessor<stepconv>::accessor(const asap::period & range, detail::accessor<stepconv>::step_t step)
      : range(range)
      , step(step) { }
  inline period::period(const datetime & a, const datetime & b)
      : begin_(a)
      , end_(b) { }
  template<uint64_t stepconv>
  inline asap::detail::accessor<stepconv> period::every(const asap::duration<stepconv> & d) const {
    return {*this, d};
  }
  inline const asap::datetime & period::from() const { return begin_; }
  inline void period::from(const asap::datetime & begin) { begin_ = begin; }
  inline const asap::datetime & period::to() const { return end_; }
  inline void period::to(const asap::datetime & end) { end_ = end; }
  asap::seconds period::difference() const { return {end_ - begin_}; }
  template<typename T> T period::difference() const { return static_cast<T>(difference()); }
}
