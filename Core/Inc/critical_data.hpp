#ifndef __CRITICAL_DATA_HPP
#define __CRITICAL_DATA_HPP

#ifndef __cplusplus
#error "This header is only for C++"
#endif

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_crc.h"

extern "C" CRC_HandleTypeDef hcrc;

template<typename T>
class critical_data final
{
public:
    // Ensure that the size of the data type is a multiple of 4 bytes
    static_assert(sizeof(T) % sizeof(uint32_t) == 0);

    // Skip crc calculate for static initializations
    explicit critical_data() noexcept {}
    explicit critical_data(const T& data) noexcept;
    explicit critical_data(T&& data) noexcept;
    explicit critical_data(const critical_data& other) noexcept;
    explicit critical_data(critical_data&& other) noexcept;
    ~critical_data() noexcept {};
    critical_data& operator=(const T& data) noexcept;
    critical_data& operator=(T&& data) noexcept;
    critical_data& operator=(const critical_data& other) noexcept;
    critical_data& operator=(critical_data&& other) noexcept;

    volatile T& get() noexcept;
    volatile const T& get() const noexcept;
    void set(const T& data) noexcept;
    bool is_valid() const noexcept;

    uint32_t calculate_crc() const noexcept;
    uint32_t update_crc(uint32_t new_crc) const noexcept;

    bool operator!() const noexcept;

    operator volatile T&() noexcept;
    operator volatile const T&() const noexcept;

private:
    volatile T data_;
    volatile uint32_t crc_;
};

template<typename T>
critical_data<T>::critical_data(const T& data) noexcept
{
    set((const T&)data);
}

template<typename T>
critical_data<T>::critical_data(T&& data) noexcept
{
    set((const T&)data);
}

template<typename T>
critical_data<T>::critical_data(const critical_data& other) noexcept
{
    set((const T&)other.data_);
}

template<typename T>
critical_data<T>::critical_data(critical_data&& other) noexcept
{
    set((const T&)other.data_);
}

template<typename T>
critical_data<T>& critical_data<T>::operator=(const T& data) noexcept
{
    set((const T&)data);
    return *this;
}

template<typename T>
critical_data<T>& critical_data<T>::operator=(T&& data) noexcept
{
    set((const T&)data);
    return *this;
}

template<typename T>
critical_data<T>& critical_data<T>::operator=(const critical_data& other) noexcept
{
    set((const T&)other.data_);
    return *this;
}

template<typename T>
critical_data<T>& critical_data<T>::operator=(critical_data&& other) noexcept
{
    set((const T&)other.data_);
    return *this;
}

template<typename T>
uint32_t critical_data<T>::calculate_crc() const noexcept
{
    return HAL_CRC_Calculate(&hcrc, (uint32_t*)&data_, sizeof(data_) / sizeof(uint32_t));
}

template<typename T>
volatile T& critical_data<T>::get() noexcept
{
    return data_;
}

template<typename T>
volatile const T& critical_data<T>::get() const noexcept
{
    return data_;
}

template<typename T>
void critical_data<T>::set(const T& data) noexcept
{
    data_ = data;
    crc_ = calculate_crc();
}

template<typename T>
bool critical_data<T>::operator!() const noexcept
{
    return !is_valid();
}

template<typename T>
critical_data<T>::operator volatile T&() noexcept
{
    return data_;
}

template<typename T>
critical_data<T>::operator volatile const T&() const noexcept
{
    return data_;
}

template<typename T>
uint32_t critical_data<T>::update_crc(uint32_t new_crc) const noexcept
{
    crc_ = new_crc;
    return crc_;
}

template<typename T>
bool critical_data<T>::is_valid() const noexcept
{
    return crc_ == calculate_crc();
}

#define CRITICAL(T, N) __attribute__((section(".critical"))) critical_data<T> N

#endif