#pragma once

#include <vector>

#include <boost/variant.hpp>
#include <boost/none.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <libral/cast.hpp>
#include <libral/result.hpp>

/* Heavily based on
   https://github.com/puppetlabs/puppetcpp/blob/master/lib/include/puppet/runtime/values/value.hpp
*/
namespace libral {

  /**
   * Represents all possible value types.
   */
  using array = std::vector<std::string>;

  using value_base = boost::variant<
    boost::none_t,
    bool,
    std::string,
    array
    >;

  /**
   * Represents a runtime value.
   */
  struct value : value_base
  {
    using json_container = leatherman::json_container::JsonContainer;
    using json_keys = std::vector<leatherman::json_container::JsonContainerKey>;

    /**
     * Default constructor for value.
     */
    value() : value_base(boost::none) { };

    /**
     * Constructs a value based on a variant type.
     * @tparam T The variant type to construct with.
     * @param value The value to initialize the variant with.
     */
    template <
      typename T,
      typename = typename std::enable_if<
        !std::is_pointer<T>::value &&
        !std::is_same<typename std::remove_reference<T>::type, value>::value
        >::type
      >
    value(T const& value) :
      value_base(value)
    {
    }

    /**
     * Constructs a value based on a variant type.
     * @tparam T The variant type to construct with.
     * @param value The value to initialize the variant with.
     */
    template <
      typename T,
      typename = typename std::enable_if<
        !std::is_pointer<T>::value &&
        !std::is_const<T>::value &&
        std::is_rvalue_reference<T&&>::value &&
        !std::is_same<typename std::remove_reference<T>::type, value>::value
        >::type
      >
    value(T&& value) noexcept :
      value_base(rvalue_cast(value))
    {
    }

    /**
     * Constructs a value given a C string.
     * @param string The string to construct the value with.
     */
    value(char const* string);

    /**
     * Intentionally deleted constructor to prevent implicit conversion to a boolean value.
     */
    value(void const*) = delete;

    /**
     * Copy assignment operator given a C-string to set as a string value.
     * @param string The string to assign to the value.
     * @return Returns this value.
     */
    value& operator=(char const* string);

    /**
     * Gets a pointer to the requested value type.
     * Use this over boost::get for values to properly dereference variables.
     * @tparam T The requested value type.
     * @return Returns a pointer to the given type or nullptr if this value is not of that type.
     */
    template <typename T>
    T const* as() const
    {
      return boost::get<T>(this);
    }

    /**
     * Requires that the value hold the given type and returns a reference to it.
     * @tparam T The requested value type.
     * @return Returns a reference to the given type or throws an exception if the value is not of that type.
     */
    template <typename T>
    T const& require() const
    {
      auto ptr = as<T>();
      if (!ptr) {
        throw std::runtime_error("invalid cast requested.");
      }
      return *ptr;
    }

    /**
     * Moves the value as the given value type.
     * Note: throws boost::bad_get if the value is not of the given type.
     * @tparam T The requested value type.
     * @return Returns the value moved into the given type or a copy if the value is a variable.
     */
    template <typename T>
    T move_as()
    {
      // Move this value
      return rvalue_cast(boost::get<T>(*this));
    }

    /**
     * Determines if the given value is anything but none.
     * @return Returns false for none values or true for anything else.
     */
    bool is_present() const { return type() != typeid(boost::none_t); };

    /**
     * Determines if the given value is anything but none.
     * @return Returns false for none values or true for anything else.
     */
    operator bool() const { return type()  != typeid(boost::none_t); };

    /**
     * Determines if a value is the "true" value.
     * @return Returns true if the value is exactly "true", or false if not.
     */
    bool is_true() const;

    /**
     * Determines if a value is the "false" value.
     * @return Returns true if the value is exactly "false", or false if not.
     */
    bool is_false() const;

    std::string to_string() const;

    /**
     * Converts this value into a JSON value and places it into the
     * document \p js at the position specified by \p keys.
     */
    // FIXME: rather than passing in a location where to put the value, it
    // would be cleaner to have this return a json_value. But
    // json_container doesn't give us a good interface to interact with
    // that
    void to_json(json_container& js, const json_keys& key) const;

    static const value none;
  };

  /**
   * Stream insertion operator for runtime value.
   * @param os The output stream to write the runtime value to.
   * @param val The runtime value to write.
   * @return Returns the given output stream.
   */
  std::ostream& operator<<(std::ostream& os, value const& val);

  /**
   * Equality operator for value.
   * @param left The left value to compare.
   * @param right The right value to compare.
   * @return Returns true if the two values are equal or false if not.
   */
  bool operator==(value const& left, value const& right);

  /**
   * Inequality operator for value.
   * @param left The left value to compare.
   * @param right The right value to compare.
   * @return Returns true if the two values are not equal or false if they are equal.
   */
  bool operator!=(value const& left, value const& right);
}
