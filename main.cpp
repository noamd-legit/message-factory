#include <stdio.h>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <type_traits>

template<typename First, typename... Rest>
struct max_type_size
{
    using rest_type = typename max_type_size<Rest...>::type;

    using type = typename std::conditional<(sizeof(First) > sizeof(rest_type)), First, rest_type>::type;
};

template<typename T>
struct max_type_size<T>
{
    using type = T;
};

template<typename Base, typename First, typename... Rest>
struct is_all_base_of
{
    static constexpr bool value = std::is_base_of<Base, First>::value &&
                                  is_all_base_of<Base, Rest...>::value;
    
};

template<typename Base, typename Message>
struct is_all_base_of<Base, Message>
{
    static constexpr bool value = std::is_base_of<Base, Message>::value;
};

template<typename T, typename First, typename... Rest>
struct is_in_list
{
    static constexpr bool value = std::is_same<T, First>::value ||
                                  is_in_list<T, Rest...>::value;
};

template<typename T, typename First>
struct is_in_list<T, First>
{
    static constexpr bool value = std::is_same<T, First>::value;
};

template<size_t N>
struct raw_data
{
    static_assert(N > 0, "raw data array must be bigger then zero");

    template<typename T>
    operator T&()
    {
        return reinterpret_cast<T&>(data);
    }

    operator uint8_t*()
    {
        return data;
    }

    uint8_t data[N];
};

template<typename F, typename... R>
struct overload_set : public F, public overload_set<R...>
{
    overload_set(F f, R... r) : F(f), overload_set<R...>(r...)
    {}

    using F::operator();
    using overload_set<R...>::operator();
};

template<typename F>
struct overload_set<F> : F
{
    overload_set(F f) : F(f)
    {}

    using F::operator();
};
template<typename F, typename... R>
struct using_msg_type : public using_msg_type<F>,
                        public using_msg_type<R...>
{
    using using_msg_type<F>::get_message_type;
    using using_msg_type<R...>::get_message_type;
};

template<typename F>
struct using_msg_type<F> : public F
{
    using F::get_message_type;
};

template<typename Base, typename... Messages>
struct messages_factory
// : public using_msg_type<Base, Messages...>
{
    // User defined functions to access the data.
    //using using_msg_type<Base, Messages...>::get_message_type;

    static_assert(is_all_base_of<Base, Messages...>::value, "All the messages must have a common base");

    using max_type = typename max_type_size<Messages...>::type;
    static constexpr size_t max_message_size = sizeof(max_type); 

    static_assert(max_message_size > 0, "Max message size must be greater than zero");

    template<typename T>
    T& as()
    {
        static_assert(is_in_list<T, Messages...>::value, "must be in list");
        return _data;
    }


    template<size_t N>
    void set_data(const uint8_t (&arr)[N])
    {
        constexpr size_t buff_size = N > sizeof(_data) ? sizeof(_data) : N;
        uint8_t* data = static_cast<uint8_t*>(_data);
        std::memcpy(data, arr, buff_size);
    }

    /*
    template<typename F, typename... R>
    void print_types()
    {
        std::cout << as<F>.get_message_type() << std::endl;
        print_types<R...>();
    }*/

    template<typename T>
    bool print_type(T)
    {
        std::cout << as<T>().get_message_type() << std::endl;
        return true;
    }

    void print_all_types()
    {
        (void)std::initializer_list<bool>{print_type(Messages{})...};
    }

    template<typename T, typename Func>
    bool handle_type(T, size_t type, Func&& func)
    {
        auto msg = as<T>();

        if(msg.get_message_type() != type)
        {
            return false;
        }

        return func(msg);
    }

    template<typename Func>
    bool handle_message(size_t type, Func&& func)
    {
        // Instead of trying to use the handler on each message I could metaloop on the types
        // and stop when finding the right one.
        auto test = std::initializer_list<bool>{(handle_type(Messages{}, type, func))...};
        
        // It's problematic to return bool from this function because the false value has
        // double meaning - handler faild and the message type doesn't exists.
        return std::count(test.begin(), test.end(), true) == 1;
    }
    
    template<typename Func>
    bool handle_message(Func&& func)
    {
        Base& header = _data; 
        size_t msg_type = header.get_message_type();

        return handle_message(msg_type, func);
    }

    template<typename... Func>
    bool handle_message(Func... lambdas)
    {
        overload_set<Func...> func{lambdas...};
        
        return handle_message(func);
    }

    raw_data<sizeof(max_type)> _data;
};

struct A
{
    int a;
    uint32_t get_message_type()
    {
        return a;
    }
};

struct B : A {
    int b;
    static constexpr uint32_t get_message_type()
    {
        return 1;
    }
};
struct C : A 
{
    int c;
    static constexpr uint32_t get_message_type()
    {
        return 2;
    }
};
struct D{};



template<typename... F>
auto make_overload(F... fs)
{
    return overload_set<F...>(fs...);
}

int main()
{
    std::cout << "hello world" << std::endl;
    max_type_size<int, double, short>::type x;
    static_assert(std::is_same<decltype(x), double>::value, "max type is not int");
    static_assert(is_all_base_of<A,B,C>::value, "Nope");
    messages_factory<A, B, C> fac;

    uint8_t arr[sizeof(int)];
    auto  a = new(arr) int(2);
    std::cout << "arr addr: " << &arr << " int addr: " << a << std::endl;
    fac.set_data(arr);
    //fac.find_matching_message();
    fac.print_all_types();

    struct functor
    {
        bool operator()(B& a)
        {
            std::cout << "B" << std::endl;
            return true;
        };

        bool operator()(C& c)
        {
            std::cout << "C" << std::endl;
            return true;
        }
    }funca;

    //fac.handle_message([](B& m) -> bool {
    //        std::cout << "test" << std::endl;
    //}
    //);

    fac.handle_message(funca);

    auto test = make_overload(
        [](B& b){
        std::cout << "b" << std::endl;
        return true;
    },
        [](C& c){
        std::cout << "c" << std::endl;
        return true;
    });

    fac.handle_message(test);
    
    a = new(arr) int(1);
    fac.set_data(arr);

    fac.handle_message(
    [](B& b){
        std::cout << "b2" << std::endl;
        return true;
    },
    [](C& c){
        std::cout << "c3" << std::endl;
        return true;
    });

    return 0;
}
